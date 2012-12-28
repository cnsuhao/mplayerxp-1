#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <iomanip>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>

#include "input.h"
#include "mouse.h"
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include "osdep/keycodes.h"
#include "osdep/get_path.h"
#include "osdep/timer.h"
#include "libmpconf/cfgparser.h"

#include "in_msg.h"

#ifdef HAVE_LIRC
#include "lirc.h"
#endif

#ifdef HAVE_LIRCC
#include <lirc/lircc.h>
#endif
#include "joystick.h"
#include "osdep/getch2.h"

namespace mpxp {

static const int MP_MAX_KEY_FD=10;
static const int MP_MAX_CMD_FD=10;

enum {
    MP_FD_EOF=(1<<0),
    MP_FD_DROP=(1<<1),
    MP_FD_DEAD=(1<<2),
    MP_FD_GOT_CMD=(1<<3),
    MP_FD_NO_SELECT=(1<<4)
};

static const int CMD_QUEUE_SIZE=100;

typedef int (*mp_key_func_t)(any_t* ctx); // These functions should return the key code or one of the error code
typedef int (*mp_cmd_func_t)(any_t* ctx,char* dest,int size); // These functions should act like read but they must use our error code (if needed ;-)
typedef void (*mp_close_func_t)(any_t* ctx); // These are used to close the driver


struct mp_cmd_bind_t {
    int		input[MP_MAX_KEY_DOWN+1];
    const char*	cmd;
};

struct mp_key_name_t {
    int		key;
    const char*	name;
};

struct mp_input_fd_t {
    any_t*		opaque;
    union {
	mp_key_func_t	key_func;
	mp_cmd_func_t	cmd_func;
    }read;
    mp_close_func_t	close_func;
    int			flags;
    // This fields are for the cmd fds
    char*		buffer;
    int			pos,size;
};

struct mp_cmd_filter_t {
    mp_input_cmd_filter	filter;
    any_t*		ctx;
    mp_cmd_filter_t*	next;
};

struct libinput_t : public Opaque {
    libinput_t(const std::map<std::string,std::string>& _envm):envm(_envm) {}
    virtual ~libinput_t() {}

    Opaque		unusable;
    // These are the user defined binds
    mp_cmd_bind_t*	cmd_binds;
    mp_cmd_filter_t*	cmd_filters;

    mp_input_fd_t	key_fds[MP_MAX_KEY_FD];
    unsigned int	num_key_fd;
    mp_input_fd_t	cmd_fds[MP_MAX_CMD_FD];
    unsigned int	num_cmd_fd;
    mp_cmd_t*		cmd_queue[CMD_QUEUE_SIZE];
    unsigned int	cmd_queue_length,cmd_queue_start,cmd_queue_end;
    // this is the key currently down
    int			key_down[MP_MAX_KEY_DOWN];
    unsigned int	num_key_down,last_key_down;
    // Autorepeat stuff
    short		ar_state;
    mp_cmd_t*		ar_cmd;
    unsigned int	last_ar;

    int			in_file_fd;
    int			tim; //for getch2
    char		key_str[12];
    const std::map<std::string,std::string>& envm;
};

struct input_conf_t {
    int		use_joystick,use_lirc,use_lircc;
    unsigned	ar_delay,ar_rate;
    const char*	js_dev;
    const char*	in_file;
    int		print_key_list,print_cmd_list;
};
static input_conf_t libinput_conf = { 1, 1, 1, 100, 8, "/dev/input/js0", NULL, 0, 0 };

/// This array defines all know commands.
/// The first field is an id used to recognize the command without too many strcmp
/// The second is abviously the command name
/// The third is the minimum number of argument this command need
/// Then come the definition of each argument, terminated with and arg of type -1
/// A command can take maximum MP_CMD_MAX_ARGS-1 arguments (-1 because of
/// the terminal one) wich is actually 9

/// For the args, the first field is the type (actually int, float or string), the second
/// is the default value wich is used for optional arguments

static const mp_cmd_t mp_cmds[] = {
  { MP_CMD_SEEK, "seek", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_INCR, "speed_incr", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_MULT, "speed_mult", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_SET, "speed_set", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_SWITCH_AUDIO, "cycle_audio", 0, { {-1,{0}} } },
  { MP_CMD_SWITCH_VIDEO, "cycle_video", 0, { {-1,{0}} } },
  { MP_CMD_SWITCH_SUB, "cycle_subtitles", 0, { {-1,{0}} } },
  { MP_CMD_QUIT, "quit", 0, { {-1,{0}} } },
  { MP_CMD_SOFT_QUIT, "soft_quit", 0, { {-1,{0}} } },
  { MP_CMD_PAUSE, "pause", 0, { {-1,{0}} } },
  { MP_CMD_FRAME_STEP, "frame_step", 0, { {-1,{0}} } },
  { MP_CMD_PLAY_TREE_STEP, "pt_step",1, { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT ,{0}}, {-1,{0}} } },
  { MP_CMD_PLAY_TREE_UP_STEP, "pt_up_step",1,  { { MP_CMD_ARG_INT,{0} }, { MP_CMD_ARG_INT ,{0}}, {-1,{0}} } },
  { MP_CMD_PLAY_ALT_SRC_STEP, "alt_src_step",1, { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_OSD, "osd",0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_VOLUME, "volume", 1, { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_MUTE, "mute", 0, { {-1,{0}} } },
  { MP_CMD_CONTRAST, "contrast",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_BRIGHTNESS, "brightness",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_HUE, "hue",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SATURATION, "saturation",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_FRAMEDROPPING, "frame_drop",0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
  { MP_CMD_SUB_POS, "sub_pos", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
#ifdef USE_TV
  { MP_CMD_TV_STEP_CHANNEL, "tv_step_channel", 1,  { { MP_CMD_ARG_INT ,{0}}, {-1,{0}} }},
  { MP_CMD_TV_STEP_NORM, "tv_step_norm",0, { {-1,{0}} }  },
  { MP_CMD_TV_STEP_CHANNEL_LIST, "tv_step_chanlist", 0, { {-1,{0}} }  },
#endif
  { MP_CMD_VO_FULLSCREEN, "vo_fullscreen", 0, { {-1,{0}} } },
  { MP_CMD_VO_SCREENSHOT, "vo_screenshot", 0, { {-1,{0}} } },
  { MP_CMD_PANSCAN, "panscan",1,  { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },

#ifdef USE_DVDNAV
  { MP_CMD_DVDNAV, "dvdnav", 1, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_DVDNAV_EVENT, "dvdnav_event", 1, { { MP_CMD_ARG_VOID, {0}}, {-1, {0}} } },
#endif
  { MP_CMD_MENU, "menu",1,  { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_SET_MENU, "set_menu",1,  { {MP_CMD_ARG_STRING, {0}},  {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_CHELP, "help", 0, { {-1,{0}} } },
  { MP_CMD_CEXIT, "exit", 0, { {-1,{0}} } },
  { MP_CMD_CHIDE, "hide", 0, { {MP_CMD_ARG_INT,{3000}}, {-1,{0}} } },

  { 0, NULL, 0, {} }
};

/// The names of the key for input.conf
/// If you add some new keys, you also need to add them here

static const mp_key_name_t key_names[] = {
  { ' ', "SPACE" },
  { KEY_ENTER, "ENTER" },
  { KEY_TAB, "TAB" },
  { KEY_CTRL, "CTRL" },
  { KEY_BACKSPACE, "BS" },
  { KEY_DELETE, "DEL" },
  { KEY_INSERT, "INS" },
  { KEY_HOME, "HOME" },
  { KEY_END, "END" },
  { KEY_PAGE_UP, "PGUP" },
  { KEY_PAGE_DOWN, "PGDWN" },
  { KEY_ESC, "ESC" },
  { KEY_RIGHT, "RIGHT" },
  { KEY_LEFT, "LEFT" },
  { KEY_DOWN, "DOWN" },
  { KEY_UP, "UP" },
  { KEY_F+1, "F1" },
  { KEY_F+2, "F2" },
  { KEY_F+3, "F3" },
  { KEY_F+4, "F4" },
  { KEY_F+5, "F5" },
  { KEY_F+6, "F6" },
  { KEY_F+7, "F7" },
  { KEY_F+8, "F8" },
  { KEY_F+9, "F9" },
  { KEY_F+10, "F10" },
  { KEY_KP0, "KP0" },
  { KEY_KP1, "KP1" },
  { KEY_KP2, "KP2" },
  { KEY_KP3, "KP3" },
  { KEY_KP4, "KP4" },
  { KEY_KP5, "KP5" },
  { KEY_KP6, "KP6" },
  { KEY_KP7, "KP7" },
  { KEY_KP8, "KP8" },
  { KEY_KP9, "KP9" },
  { KEY_KPDEL, "KP_DEL" },
  { KEY_KPDEC, "KP_DEL" },
  { KEY_KPINS, "KP0" },
  { KEY_KPENTER, "KP_ENTER" },
  { MOUSE_BTN0, "MOUSE_BTN0" },
  { MOUSE_BTN1, "MOUSE_BTN1" },
  { MOUSE_BTN2, "MOUSE_BTN2" },
  { MOUSE_BTN3, "MOUSE_BTN3" },
  { MOUSE_BTN4, "MOUSE_BTN4" },
  { MOUSE_BTN5, "MOUSE_BTN5" },
  { MOUSE_BTN6, "MOUSE_BTN6" },
  { MOUSE_BTN7, "MOUSE_BTN7" },
  { MOUSE_BTN8, "MOUSE_BTN8" },
  { MOUSE_BTN9, "MOUSE_BTN9" },
  { MOUSE_BTN0_DBL, "MOUSE_BTN0_DBL" },
  { MOUSE_BTN1_DBL, "MOUSE_BTN1_DBL" },
  { MOUSE_BTN2_DBL, "MOUSE_BTN2_DBL" },
  { MOUSE_BTN3_DBL, "MOUSE_BTN3_DBL" },
  { MOUSE_BTN4_DBL, "MOUSE_BTN4_DBL" },
  { MOUSE_BTN5_DBL, "MOUSE_BTN5_DBL" },
  { MOUSE_BTN6_DBL, "MOUSE_BTN6_DBL" },
  { MOUSE_BTN7_DBL, "MOUSE_BTN7_DBL" },
  { MOUSE_BTN8_DBL, "MOUSE_BTN8_DBL" },
  { MOUSE_BTN9_DBL, "MOUSE_BTN9_DBL" },
  { JOY_AXIS1_MINUS, "JOY_UP" },
  { JOY_AXIS1_PLUS, "JOY_DOWN" },
  { JOY_AXIS0_MINUS, "JOY_LEFT" },
  { JOY_AXIS0_PLUS, "JOY_RIGHT" },

  { JOY_AXIS0_PLUS, "JOY_AXIS0_PLUS" },
  { JOY_AXIS0_MINUS, "JOY_AXIS0_MINUS" },
  { JOY_AXIS1_PLUS, "JOY_AXIS1_PLUS" },
  { JOY_AXIS1_MINUS, "JOY_AXIS1_MINUS" },
  { JOY_AXIS2_PLUS, "JOY_AXIS2_PLUS" },
  { JOY_AXIS2_MINUS, "JOY_AXIS2_MINUS" },
  { JOY_AXIS3_PLUS, "JOY_AXIS3_PLUS" },
  { JOY_AXIS3_MINUS, "JOY_AXIS3_MINUS" },
  { JOY_AXIS4_PLUS, "JOY_AXIS4_PLUS" },
  { JOY_AXIS4_MINUS, "JOY_AXIS4_MINUS" },
  { JOY_AXIS5_PLUS, "JOY_AXIS5_PLUS" },
  { JOY_AXIS5_MINUS, "JOY_AXIS5_MINUS" },
  { JOY_AXIS6_PLUS, "JOY_AXIS6_PLUS" },
  { JOY_AXIS6_MINUS, "JOY_AXIS6_MINUS" },
  { JOY_AXIS7_PLUS, "JOY_AXIS7_PLUS" },
  { JOY_AXIS7_MINUS, "JOY_AXIS7_MINUS" },
  { JOY_AXIS8_PLUS, "JOY_AXIS8_PLUS" },
  { JOY_AXIS8_MINUS, "JOY_AXIS8_MINUS" },
  { JOY_AXIS9_PLUS, "JOY_AXIS9_PLUS" },
  { JOY_AXIS9_MINUS, "JOY_AXIS9_MINUS" },

  { JOY_BTN0, "JOY_BTN0" },
  { JOY_BTN1, "JOY_BTN1" },
  { JOY_BTN2, "JOY_BTN2" },
  { JOY_BTN3, "JOY_BTN3" },
  { JOY_BTN4, "JOY_BTN4" },
  { JOY_BTN5, "JOY_BTN5" },
  { JOY_BTN6, "JOY_BTN6" },
  { JOY_BTN7, "JOY_BTN7" },
  { JOY_BTN8, "JOY_BTN8" },
  { JOY_BTN9, "JOY_BTN9" },

  { KEY_XF86_STANDBY, "XF86_STANDBY" },
  { KEY_XF86_POWER, "XF86_POWER" },
  { KEY_XF86_PAUSE, "XF86_PAUSE" },
  { KEY_XF86_STOP, "XF86_STOP" },
  { KEY_XF86_PREV, "XF86_PREV" },
  { KEY_XF86_NEXT, "XF86_NEXT" },
  { KEY_XF86_VOLUME_UP, "XF86_VOLUME_UP" },
  { KEY_XF86_VOLUME_DN, "XF86_VOLUME_DN" },
  { KEY_XF86_MUTE, "XF86_MUTE" },
  { KEY_XF86_EJECT, "XF86_EJECT" },
  { KEY_XF86_MENU, "XF86_MENU" },
  { KEY_XF86_PLAY, "XF86_PLAY" },
  { KEY_XF86_FORWARD, "XF86_FORWARD" },
  { KEY_XF86_REWIND, "XF86_REWIND" },
  { KEY_XF86_BRIGHTNESS, "XF86_BRIGHTNESS" },
  { KEY_XF86_CONTRAST, "XF86_CONTRAST" },
  { KEY_XF86_SATURATION, "XF86_SATURATION" },
  { KEY_XF86_SCREENSAVE, "XF86_SCREENSAVE" },
  { KEY_XF86_REFRESH, "XF86_REFRESH" },

  { 0, NULL }
};

// This is the default binding. The content of input.conf override these ones.
// The first args is a null terminated array of key codes.
// The second is the command

static const mp_cmd_bind_t def_cmd_binds[] = {

  { {  MOUSE_BTN3, 0 }, "seek 10" },
  { {  MOUSE_BTN4, 0 }, "seek -10" },
  { {  MOUSE_BTN5, 0 }, "volume 1" },
  { {  MOUSE_BTN6, 0 }, "volume -1" },

#ifdef USE_DVDNAV
  { { KEY_KP8, 0 }, "dvdnav 1" },   // up
  { { KEY_KP2, 0 }, "dvdnav 2" },   // down
  { { KEY_KP4, 0 }, "dvdnav 3" },   // left
  { { KEY_KP6, 0 }, "dvdnav 4" },   // right
  { { KEY_KP5, 0 }, "dvdnav 5" },   // menu
  { { KEY_KP0, 0 }, "dvdnav 6" },   // select
  { { KEY_KPINS, 0 }, "dvdnav 6" },   // select
#endif
  { { KEY_F+3, 0 }, "menu menu" },
  { { KEY_XF86_MENU, 0 }, "menu menu" },
  { { KEY_F+2, 0 }, "set_menu" },
  { { KEY_F+1, 0 }, "menu help" },
  { { KEY_F+10, 0 }, "menu exit" },
  { { KEY_F+8, 0 }, "menu hide" },

  { { KEY_RIGHT, 0 }, "seek 10" },
  { {  KEY_LEFT, 0 }, "seek -10" },
  { {  KEY_UP, 0 }, "seek 60" },
  { {  KEY_DOWN, 0 }, "seek -60" },
  { {  KEY_PAGE_UP, 0 }, "seek 600" },
  { { KEY_PAGE_DOWN, 0 }, "seek -600" },
  { { '[', 0 }, "speed_mult 0.9091" },
  { { ']', 0 }, "speed_mult 1.1" },
  { { '{', 0 }, "speed_mult 0.5" },
  { { '}', 0 }, "speed_mult 2.0" },
  { { KEY_BACKSPACE, 0 }, "speed_set 1.0" },
  { { 'q', 0 }, "soft_quit" },
  { { KEY_ESC, 0 }, "quit" },
  { { 'p', 0 }, "pause" },
  { { ' ', 0 }, "pause" },
  { { '.', 0 }, "frame_step" },
  { { KEY_HOME, 0 }, "pt_up_step 1" },
  { { KEY_END, 0 }, "pt_up_step -1" },
  { { '>', 0 }, "pt_step 1" },
  { { KEY_ENTER, 0 }, "pt_step 1 1" },
  { { '<', 0 }, "pt_step -1" },
  { { KEY_INS, 0 }, "alt_src_step 1" },
  { { KEY_DEL, 0 }, "alt_src_step -1" },
  { { 'o', 0 }, "osd" },
  { { '9', 0 }, "volume -1" },
  { { '/', 0 }, "volume -1" },
  { { '0', 0 }, "volume 1" },
  { { '*', 0 }, "volume 1" },
  { { 'm', 0 }, "mute" },
  { { '1', 0 }, "contrast -1" },
  { { '2', 0 }, "contrast 1" },
  { { '3', 0 }, "brightness -1" },
  { { '4', 0 }, "brightness 1" },
  { { '5', 0 }, "hue -1" },
  { { '6', 0 }, "hue 1" },
  { { '7', 0 }, "saturation -1" },
  { { '8', 0 }, "saturation 1" },
  { { 'd', 0 }, "frame_drop" },
  { { 'r', 0 }, "sub_pos -1" },
  { { 't', 0 }, "sub_pos +1" },
#ifdef USE_TV
  { { 'h', 0 }, "tv_step_channel 1" },
  { { 'k', 0 }, "tv_step_channel -1" },
  { { 'n', 0 }, "tv_step_norm" },
  { { 'u', 0 }, "tv_step_chanlist" },
#endif
#ifdef HAVE_JOYSTICK
  { { JOY_AXIS0_PLUS, 0 }, "seek 10" },
  { { JOY_AXIS0_MINUS, 0 }, "seek -10" },
  { { JOY_AXIS1_MINUS, 0 }, "seek 60" },
  { { JOY_AXIS1_PLUS, 0 }, "seek -60" },
  { { JOY_BTN0, 0 }, "pause" },
  { { JOY_BTN1, 0 }, "osd" },
  { { JOY_BTN2, 0 }, "volume 1"},
  { { JOY_BTN3, 0 }, "volume -1"},
#endif
  { { 'f', 0 }, "vo_fullscreen" },
  { { 's', 0 }, "vo_screenshot" },
  { { 'w', 0 }, "panscan -0.1" },
  { { 'e', 0 }, "panscan +0.1" },

  { { 'a', 0 }, "cycle_audio" },
  { { 'v', 0 }, "cycle_video" },
  { { 'c', 0 }, "cycle_subtitles" },

  { { KEY_XF86_PLAY, 0 }, "pause" },
  { { KEY_XF86_PAUSE, 0 }, "pause" },
  { { KEY_XF86_STANDBY, 0 }, "pause" },
  { { KEY_XF86_STOP, 0 }, "soft_quit" },
  { { KEY_XF86_POWER, 0 }, "quit" },
  { { KEY_XF86_EJECT, 0 }, "quit" },
  { { KEY_XF86_PREV, 0 }, "pt_step -1" },
  { { KEY_XF86_NEXT, 0 }, "pt_step 1" },
  { { KEY_XF86_REWIND, 0 }, "seek -60" },
  { { KEY_XF86_FORWARD, 0 }, "seek 60" },
  { { KEY_XF86_VOLUME_UP, 0 }, "volume 1"},
  { { KEY_XF86_VOLUME_DN, 0 }, "volume -1"},
  { { KEY_XF86_MUTE, 0 }, "mute" },
  { { KEY_XF86_SCREENSAVE, 0 }, "vo_screenshot" },
  { { KEY_XF86_BRIGHTNESS, 0 }, "brightness 1" },
  { { KEY_XF86_CONTRAST, 0 }, "contrast 1" },
  { { KEY_XF86_SATURATION, 0 }, "saturation 1" },
  { { KEY_XF86_REFRESH, 0 }, "pause" },

  { { 0 }, NULL }
};

static const char* config_file = "input.conf";

// Callback to allow the menu filter to grab the incoming keys
void (*mp_input_key_cb)(int code) = NULL;

static mp_cmd_t* mp_cmd_clone(mp_cmd_t* cmd); // This create a copy of a command (used by the auto repeat stuff)
static int mp_input_print_key_list(libinput_t&);
static int mp_input_print_cmd_list(libinput_t&);

static const mpxp_option_t joystick_conf[] = {
  { "on", &libinput_conf.use_joystick,  CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, "enables using of joystick" },
  { "off", &libinput_conf.use_joystick,  CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, "disables using of joystick" },
  { "dev", &libinput_conf.js_dev, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, "specifies the joystick device" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

extern char *lirc_configfile;
// Our command line options
static const mpxp_option_t input_conf[] = {
  { "conf", (any_t*)&config_file, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, "specifies alternative input.conf" },
  { "ar-delay", (any_t*)&libinput_conf.ar_delay, CONF_TYPE_INT, CONF_GLOBAL, 0, 0, "autorepeate a key delay in milliseconds (0 to disable)" },
  { "ar-rate", (any_t*)&libinput_conf.ar_rate, CONF_TYPE_INT, CONF_GLOBAL, 0, 0, "number of key-presses per second generating on autorepeat" },
  { "keylist", (any_t*)&libinput_conf.print_key_list, CONF_TYPE_INT, CONF_GLOBAL, 0, 0, "prints all keys that can be bound to commands" },
  { "cmdlist", (any_t*)&libinput_conf.print_cmd_list, CONF_TYPE_INT, CONF_GLOBAL, 0, 0, "prints all commands that can be bound to keys" },
  { "file", (any_t*)&libinput_conf.in_file, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, "specifes file with commands (useful for FIFO)" },
#ifdef HAVE_LIRC
  { "lircconf", (any_t*)&lirc_configfile, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, "specifies a config.file for LIRC"},
#endif
  { "lirc", (any_t*)&libinput_conf.use_lirc, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, "enables using of lirc" },
  { "nolirc", (any_t*)&libinput_conf.use_lirc, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, "disables using of lirc" },
  { "lircc", (any_t*)&libinput_conf.use_lircc, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, "enables using of lirc daemon" },
  { "nolircc", (any_t*)&libinput_conf.use_lircc, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, "disables using of lirc daemon" },
  { "joystick", (any_t*)&joystick_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Joystick related options" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const mpxp_option_t mp_input_opts[] = {
  { "input", (any_t*)&input_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Input specific options"},
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static int mp_input_default_key_func(any_t* fd);
static int mp_input_default_cmd_func(any_t* fd,char* buf, int l);
static const char* mp_input_get_key_name(libinput_t&,int key);

static MPXP_Rc mp_input_add_cmd_fd(libinput_t& priv,any_t* opaque, int sel, mp_cmd_func_t read_func, mp_close_func_t close_func) {
    if(priv.num_cmd_fd == MP_MAX_CMD_FD) {
	mpxp_err<<"Too much command fd, unable to register fd"<<std::endl;
	return MPXP_False;
    }

    memset(&priv.cmd_fds[priv.num_cmd_fd],0,sizeof(mp_input_fd_t));
    priv.cmd_fds[priv.num_cmd_fd].opaque = opaque;
    priv.cmd_fds[priv.num_cmd_fd].read.cmd_func = read_func ? read_func : mp_input_default_cmd_func;
    priv.cmd_fds[priv.num_cmd_fd].close_func = close_func;
    if(!sel) priv.cmd_fds[priv.num_cmd_fd].flags = MP_FD_NO_SELECT;
    priv.num_cmd_fd++;

    return MPXP_Ok;
}

static void mp_input_rm_cmd_fd(libinput_t& priv,any_t* fd) {
    unsigned int i;

    for(i = 0; i < priv.num_cmd_fd; i++) {
	if(priv.cmd_fds[i].opaque == fd) break;
    }
    if(i == priv.num_cmd_fd) return;
    if(priv.cmd_fds[i].close_func) priv.cmd_fds[i].close_func(priv.cmd_fds[i].opaque);
    if(priv.cmd_fds[i].buffer)     delete priv.cmd_fds[i].buffer;
    if(i + 1 < priv.num_cmd_fd)    memmove(&priv.cmd_fds[i],&priv.cmd_fds[i+1],(priv.num_cmd_fd-i-1)*sizeof(mp_input_fd_t));
    priv.num_cmd_fd--;
}

static void mp_input_rm_key_fd(libinput_t& priv,any_t* fd) {
    unsigned int i;

    for(i = 0; i < priv.num_key_fd; i++) {
	if(priv.key_fds[i].opaque == fd) break;
    }
    if(i == priv.num_key_fd) return;
    if(priv.key_fds[i].close_func) priv.key_fds[i].close_func(priv.key_fds[i].opaque);
    if(i + 1 < priv.num_key_fd)    memmove(&priv.key_fds[i],&priv.key_fds[i+1],(priv.num_key_fd-i-1)*sizeof(mp_input_fd_t));
    priv.num_key_fd--;
}

static MPXP_Rc mp_input_add_key_fd(libinput_t& priv,any_t* opaque, int sel, mp_key_func_t read_func, mp_close_func_t close_func) {
    if(priv.num_key_fd == MP_MAX_KEY_FD) {
	mpxp_err<<"Too much key fd, unable to register fd"<<std::endl;
	return MPXP_False;
    }

    memset(&priv.key_fds[priv.num_key_fd],0,sizeof(mp_input_fd_t));
    priv.key_fds[priv.num_key_fd].opaque = opaque;
    priv.key_fds[priv.num_key_fd].read.key_func = read_func ? read_func : mp_input_default_key_func;
    priv.key_fds[priv.num_key_fd].close_func = close_func;
    if(!sel) priv.key_fds[priv.num_key_fd].flags |= MP_FD_NO_SELECT;
    priv.num_key_fd++;

    return MPXP_Ok;
}

mp_cmd_t* mp_input_parse_cmd(const std::string& _str) {
    int i,l;
    char *ptr,*e;
    mp_cmd_t *cmd;
    const mp_cmd_t *cmd_def;
    char *str=mp_strdup(_str.c_str());

#ifdef MP_DEBUG
    assert(str != NULL);
#endif
    ptr=str;
    for(; ptr[0] != '\0'  && ptr[0] != '\t' && ptr[0] != ' ' ; ptr++)
    /* NOTHING */;
    if(ptr[0] != '\0') l = ptr-str;
    else  l = strlen(str);

    if(l == 0) { delete str; return NULL; }

    for(i=0; mp_cmds[i].name != NULL; i++) {
	if(strncasecmp(mp_cmds[i].name,str,l) == 0) break;
    }

    if(mp_cmds[i].name == NULL) { delete str; return NULL; }

    cmd_def = &mp_cmds[i];

    cmd = (mp_cmd_t*)mp_malloc(sizeof(mp_cmd_t));
    cmd->id = cmd_def->id;
    cmd->name = mp_strdup(cmd_def->name);

    ptr = str;

    for(i=0; ptr && i < MP_CMD_MAX_ARGS; i++) {
	ptr = strchr(ptr,' ');
	if(!ptr) break;
	while(ptr[0] == ' ' || ptr[0] == '\t') ptr++;
	if(ptr[0] == '\0') break;
	cmd->args[i].type = cmd_def->args[i].type;
	switch(cmd_def->args[i].type) {
	    case MP_CMD_ARG_INT:
		errno = 0;
		cmd->args[i].v.i = atoi(ptr);
		if(errno != 0) {
		    mpxp_err<<"Command: "<<cmd_def->name<<" argument "<<i+1<<" isn't an integer"<<std::endl;
		    ptr = NULL;
		}
	    break;
	    case MP_CMD_ARG_FLOAT:
		errno = 0;
		/* <olo@altkom.com.pl> Use portable C locale for parsing floats: */
#ifdef USE_SETLOCALE
		setlocale(LC_NUMERIC, "C");
#endif
		cmd->args[i].v.f = atof(ptr);
#ifdef USE_SETLOCALE
		setlocale(LC_NUMERIC, "");
#endif
		if(errno != 0) {
		    mpxp_err<<"Command: "<<cmd_def->name<<" argument "<<i+1<<" isn't a float"<<std::endl;
		    ptr = NULL;
		}
		break;
	    case MP_CMD_ARG_STRING: {
		char term;
		char* ptr2 = ptr, *start;
		if(ptr[0] == '\'' || ptr[0] == '"') {
		    term = ptr[0];
		    ptr2++;
		} else
		term = ' ';
		start = ptr2;
		while(1) {
		    e = strchr(ptr2,term);
		    if(!e) break;
		    if(e <= ptr2 || *(e - 1) != '\\') break;
		    ptr2 = e + 1;
		}
		if(term != ' ' && (!e || e[0] == '\0')) {
		    mpxp_err<<"Command: "<<cmd_def->name<<" argument "<<i+1<<" is unterminated"<<std::endl;
		    ptr = NULL;
		    break;
		} else if(!e) e = ptr+strlen(ptr);
		l = e-start;
		cmd->args[i].v.s = (char*)mp_malloc((l+1)*sizeof(char));
		strncpy(cmd->args[i].v.s,start,l);
		cmd->args[i].v.s[l] = '\0';
		ptr2 = start;
		for(e = strchr(ptr2,'\\') ; e ; e = strchr(ptr2,'\\')) {
		    memmove(e,e+1,strlen(e));
		    ptr2 = e + 1;
		}
	    } break;
	    case -1: ptr = NULL;
	    default: mpxp_err<<"Unknown argument "<<i<<std::endl;
	}
    }
    cmd->nargs = i;
    if(cmd_def->nargs > cmd->nargs) {
	mpxp_err<<"Got command '"<<str<<"' but"<<std::endl;
	mpxp_err<<"command "<<cmd_def->name<<" require at least "<<cmd_def->nargs<<" arguments, we found only "<<cmd->nargs<<" so far"<<std::endl;
	mp_cmd_free(cmd);
	delete str;
	return NULL;
    }
    for( ; i < MP_CMD_MAX_ARGS && cmd_def->args[i].type != -1 ; i++) {
	memcpy(&cmd->args[i],&cmd_def->args[i],sizeof(mp_cmd_arg_t));
	if(cmd_def->args[i].type == MP_CMD_ARG_STRING && cmd_def->args[i].v.s != NULL)
	    cmd->args[i].v.s = mp_strdup(cmd_def->args[i].v.s);
    }
    if(i < MP_CMD_MAX_ARGS) cmd->args[i].type = -1;
    delete str;
    return cmd;
}

static int mp_input_default_key_func(any_t* fd) {
    libinput_t& priv = *reinterpret_cast<libinput_t*>(fd);
    int r,code=0;
    unsigned int l;
    l = 0;
    if(priv.in_file_fd == 0) { // stdin is handled by getch2
	code = getch2(priv.tim);
	if(code < 0) code = MP_INPUT_NOTHING;
    } else {
	fcntl(priv.in_file_fd,F_SETFL,fcntl(priv.in_file_fd,F_GETFL)|O_NONBLOCK);
	while(l < sizeof(int)) {
	    r = read(priv.in_file_fd,(&code)+l,sizeof(int)-l);
	    if(r <= 0) break;
	    l +=r;
	}
    }
    return code;
}

static const int MP_CMD_MAX_SIZE=256;
static int mp_input_read_cmd(mp_input_fd_t* mp_fd, char** ret) {
    char* end;
    (*ret) = NULL;

    // Allocate the buffer if it dont exist
    if(!mp_fd->buffer) {
	mp_fd->buffer = (char*)mp_malloc(MP_CMD_MAX_SIZE*sizeof(char));
	mp_fd->pos = 0;
	mp_fd->size = MP_CMD_MAX_SIZE;
    }

    // Get some data if needed/possible
    while( !(mp_fd->flags & MP_FD_GOT_CMD) && !(mp_fd->flags & MP_FD_EOF) && (mp_fd->size-mp_fd->pos>1)) {
	int r = ((mp_cmd_func_t)mp_fd->read.cmd_func)(mp_fd->opaque,mp_fd->buffer+mp_fd->pos,mp_fd->size-1-mp_fd->pos);
	// Error ?
	if(r < 0) {
	    switch(r) {
		case MP_INPUT_ERROR:
		case MP_INPUT_DEAD:
		    mpxp_err<<"Error while reading cmd fd: "<<strerror(errno)<<std::endl;
		case MP_INPUT_NOTHING: return r;
	    }
	    // EOF ?
	} else if(r == 0) {
	    mp_fd->flags |= MP_FD_EOF;
	    break;
	}
	mp_fd->pos += r;
	break;
    }
    // Reset the got_cmd flag
    mp_fd->flags &= ~MP_FD_GOT_CMD;
    while(1) {
	int l = 0;
	// Find the cmd end
	mp_fd->buffer[mp_fd->pos] = '\0';
	end = strchr(mp_fd->buffer,'\n');
	// No cmd end ?
	if(!end) {
	    // If buffer is full we must drop all until the next \n
	    if(mp_fd->size - mp_fd->pos <= 1) {
		mpxp_err<<"Cmd buffer is full: dropping content"<<std::endl;
		mp_fd->pos = 0;
		mp_fd->flags |= MP_FD_DROP;
	    }
	    break;
	}
	// We alredy have a cmd : set the got_cmd flag
	else if((*ret)) {
	    mp_fd->flags |= MP_FD_GOT_CMD;
	    break;
	}
	l = end - mp_fd->buffer;
	// Not dropping : put the cmd in ret
	if( ! (mp_fd->flags & MP_FD_DROP)) {
	    (*ret) = (char*)mp_malloc((l+1)*sizeof(char));
	    strncpy((*ret),mp_fd->buffer,l);
	    (*ret)[l] = '\0';
	} else { // Remove the dropping flag
	    mp_fd->flags &= ~MP_FD_DROP;
	}
	if( mp_fd->pos - (l+1) > 0) memmove(mp_fd->buffer,end+1,mp_fd->pos-(l+1));
	mp_fd->pos -= l+1;
    }
    if(*ret) return 1;
    else     return MP_INPUT_NOTHING;
}

static int mp_input_default_cmd_func(any_t* fd,char* buf, int l) {
    libinput_t& priv=*reinterpret_cast<libinput_t*>(fd);
    fcntl(priv.in_file_fd,F_SETFL,fcntl(priv.in_file_fd,F_GETFL)|O_NONBLOCK);
    while(1) {
	int r = read(priv.in_file_fd,buf,l);
	// Error ?
	if(r < 0) {
	    if(errno == EINTR) continue;
	    else if(errno == EAGAIN) return MP_INPUT_NOTHING;
	    return MP_INPUT_ERROR;
	    // EOF ?
	}
	return r;
    }
}

void mp_input_add_cmd_filter(libinput_t& priv,mp_input_cmd_filter func,any_t* ctx) {
    mp_cmd_filter_t* filter = new(zeromem) mp_cmd_filter_t;

    filter->filter = func;
    filter->ctx = ctx;
    filter->next = priv.cmd_filters;
    priv.cmd_filters = filter;
}

static const char* mp_input_find_bind_for_key(const mp_cmd_bind_t* binds, int n,int* keys) {
    int j;

    for(j = 0; binds[j].cmd != NULL; j++) {
	if(n > 0) {
	    int found = 1,s;
	    for(s = 0; s < n && binds[j].input[s] != 0; s++) {
		if(binds[j].input[s] != keys[s]) {
		    found = 0;
		    break;
		}
	    }
	    if(found && binds[j].input[s] == 0 && s == n) break;
	    else continue;
	} else if(n == 1){
	    if(binds[j].input[0] == keys[0] && binds[j].input[1] == 0) break;
	}
    }
    return binds[j].cmd;
}

mp_cmd_t* mp_input_get_cmd_from_keys(libinput_t& priv,int n,int* keys) {
    const char* cmd = NULL;
    mp_cmd_t* ret;

    if(priv.cmd_binds) cmd = mp_input_find_bind_for_key(priv.cmd_binds,n,keys);
    if(cmd == NULL)    cmd = mp_input_find_bind_for_key(def_cmd_binds,n,keys);
    if(cmd == NULL) {
	mpxp_warn<<"No bind found for key: "<<mp_input_get_key_name(priv,keys[0]);
	if(n > 1) {
	    int s;
	    for(s=1; s < n; s++) mpxp_warn<<std::left<<mp_input_get_key_name(priv,keys[s]);
	}
	mpxp_warn<<std::endl;
	return NULL;
    }
    ret =  mp_input_parse_cmd(cmd);
    if(!ret) {
	mpxp_err<<"Invalid command for binded key: "<<mp_input_get_key_name(priv,priv.key_down[0]);
	if(priv.num_key_down > 1) {
	    unsigned int s;
	    for(s=1; s < priv.num_key_down; s++) mpxp_err<<std::left<<mp_input_get_key_name(priv,priv.key_down[s]);
	}
	mpxp_err<<" : "<<cmd<<std::endl;
    }
    return ret;
}

static int mp_input_read_key_code(libinput_t& priv,int tim) {
    int n=0;
    unsigned i;

    if(priv.num_key_fd == 0) return MP_INPUT_NOTHING;

    // Remove fd marked as dead and build the fd_set
    // n == number of fd's to be select() checked
    for(i = 0; (unsigned int)i < priv.num_key_fd; i++) {
	if( (priv.key_fds[i].flags & MP_FD_DEAD) ) {
	    mp_input_rm_key_fd(priv,priv.key_fds[i].opaque);
	    i--;
	    continue;
	} else if(priv.key_fds[i].flags & MP_FD_NO_SELECT) continue;
	n++;
    }

    if(priv.num_key_fd == 0) return MP_INPUT_NOTHING;
    for(i = 0; i < priv.num_key_fd; i++) {
	int code = -1;
	priv.tim = tim;
	code = ((mp_key_func_t)priv.key_fds[i].read.key_func)(priv.key_fds[i].opaque);
	if(code >= 0) return code;

	if(code == MP_INPUT_ERROR) mpxp_err<<"Error on key input fd"<<std::endl;
	else if(code == MP_INPUT_DEAD) {
	    mpxp_err<<"Dead key input on fd"<<std::endl;
	    mp_input_rm_key_fd(priv,priv.key_fds[i].opaque);
	}
    }
    return MP_INPUT_NOTHING;
}

static mp_cmd_t* mp_input_read_keys(libinput_t& priv,int tim) {
    int code = mp_input_read_key_code(priv,tim);
    unsigned int j;
    mp_cmd_t* ret;

    if(mp_input_key_cb) {
	for( ; code >= 0 ;   code = mp_input_read_key_code(priv,0) ) {
	    if(code & MP_KEY_DOWN) continue;
	    code &= ~(MP_KEY_DOWN|MP_NO_REPEAT_KEY);
	    mp_input_key_cb(code);
	}
	return NULL;
    }

    for( ; code >= 0 ;   code = mp_input_read_key_code(priv,0) ) {
	// key pushed
	if(code & MP_KEY_DOWN) {
	    if(priv.num_key_down > MP_MAX_KEY_DOWN) {
		mpxp_err<<"Too much key down at the same time"<<std::endl;
		continue;
	    }
	    code &= ~MP_KEY_DOWN;
	    // Check if we don't already have this key as pushed
	    for(j = 0; j < priv.num_key_down; j++) {
		if(priv.key_down[j] == code)
		break;
	    }
	    if(j != priv.num_key_down) continue;
	    priv.key_down[priv.num_key_down] = code;
	    priv.num_key_down++;
	    priv.last_key_down = GetTimer();
	    priv.ar_state = 0;
	    continue;
	}
	// key released
	// Check if the key is in the down key, driver which can't send push event
	// send only release event
	for(j = 0; j < priv.num_key_down; j++) {
	    if(priv.key_down[j] == code) break;
	}
	if(j == priv.num_key_down) { // key was not in the down keys : add it
	    if(priv.num_key_down > MP_MAX_KEY_DOWN) {
		mpxp_err<<"Too much key down at the same time"<<std::endl;
		continue;
	    }
	    priv.key_down[priv.num_key_down] = code;
	    priv.num_key_down++;
	    priv.last_key_down = 1;
	}
	// We ignore key from last combination
	ret = priv.last_key_down ? mp_input_get_cmd_from_keys(priv,priv.num_key_down,priv.key_down):NULL;
	// Remove the key
	if(j+1 < priv.num_key_down) memmove(&priv.key_down[j],&priv.key_down[j+1],(priv.num_key_down-(j+1))*sizeof(int));
	priv.num_key_down--;
	priv.last_key_down = 0;
	priv.ar_state = -1;
	if(priv.ar_cmd) {
	    mp_cmd_free(priv.ar_cmd);
	    priv.ar_cmd = NULL;
	}
	if(ret) return ret;
    }

    // No input : autorepeat ?
    if(libinput_conf.ar_rate > 0 && priv.ar_state >=0 && priv.num_key_down > 0 && !(priv.key_down[priv.num_key_down-1]&MP_NO_REPEAT_KEY)) {
	unsigned int t = GetTimer();
	// First time : wait delay
	if(priv.ar_state == 0 && (t - priv.last_key_down) >= libinput_conf.ar_delay*1000) {
	    priv.ar_cmd = mp_input_get_cmd_from_keys(priv,priv.num_key_down,priv.key_down);
	    if(!priv.ar_cmd) {
		priv.ar_state = -1;
		return NULL;
	    }
	    priv.ar_state = 1;
	    priv.last_ar = t;
	    return mp_cmd_clone(priv.ar_cmd);
	    // Then send rate / sec event
	} else if(priv.ar_state == 1 && (t -priv.last_ar) >= 1000000/libinput_conf.ar_rate) {
	    priv.last_ar = t;
	    return mp_cmd_clone(priv.ar_cmd);
	}
    }
    return NULL;
}

static mp_cmd_t* mp_input_read_cmds(libinput_t& priv) {
    int n = 0,got_cmd = 0;
    unsigned i;
    mp_cmd_t* ret;
    static int last_loop = 0;

    if(priv.num_cmd_fd == 0) return NULL;

    for(i = 0; (unsigned int)i < priv.num_cmd_fd ; i++) {
	if(priv.cmd_fds[i].flags&MP_FD_EOF) {
	    mp_input_rm_cmd_fd(priv,priv.cmd_fds[i].opaque);
	    i--;
	    continue;
	} else if(priv.cmd_fds[i].flags & MP_FD_NO_SELECT) continue;
	if(priv.cmd_fds[i].flags & MP_FD_GOT_CMD) got_cmd = 1;
	n++;
    }
    if(priv.num_cmd_fd == 0) return NULL;
    for(i = 0; i < priv.num_cmd_fd; i++) {
	int r = 0;
	char* cmd;
	r = mp_input_read_cmd(&priv.cmd_fds[i],&cmd);
	if(r < 0) {
	    if(r == MP_INPUT_ERROR) mpxp_err<<"Error on cmd fd"<<std::endl;
	    else if(r == MP_INPUT_DEAD) priv.cmd_fds[i].flags |= MP_FD_DEAD;
	    continue;
	}
	ret = mp_input_parse_cmd(cmd?cmd:"");
	delete cmd;
	if(!ret) continue;
	last_loop = i;
	return ret;
    }
    last_loop = 0;
    return NULL;
}

MPXP_Rc mp_input_queue_cmd(libinput_t& priv,mp_cmd_t* cmd) {
    if(priv.cmd_queue_length  >= CMD_QUEUE_SIZE) return MPXP_False;
    priv.cmd_queue[priv.cmd_queue_end] = cmd;
    priv.cmd_queue_end = (priv.cmd_queue_end + 1) % CMD_QUEUE_SIZE;
    priv.cmd_queue_length++;
    return MPXP_Ok;
}

static mp_cmd_t* mp_input_get_queued_cmd(libinput_t& priv,int peek_only) {
    mp_cmd_t* ret;

    if(priv.cmd_queue_length == 0) return NULL;

    ret = priv.cmd_queue[priv.cmd_queue_start];

    if (!peek_only) {
	priv.cmd_queue_length--;
	priv.cmd_queue_start = (priv.cmd_queue_start + 1) % CMD_QUEUE_SIZE;
    }
     return ret;
}

/**
 * \param peek_only when set, the returned command stays in the queue.
 * Do not mp_free the returned cmd whe you set this!
 */
mp_cmd_t* mp_input_get_cmd(libinput_t& priv,int tim, int paused, int peek_only) {
    mp_cmd_t* ret = NULL;
    mp_cmd_filter_t* cf;
    int from_queue;

    from_queue = 1;
    ret = mp_input_get_queued_cmd(priv,peek_only);
    if(!ret) {
	from_queue = 0;
	ret = mp_input_read_keys(priv,tim);
	if(!ret) ret = mp_input_read_cmds(priv);
    }
    if(!ret) return NULL;

    for(cf = priv.cmd_filters ; cf ; cf = cf->next) {
	if(cf->filter(ret,paused,cf->ctx)) return NULL;
    }

    if (!from_queue && peek_only) mp_input_queue_cmd(priv,ret);

    return ret;
}

void mp_cmd_free(mp_cmd_t* cmd) {
    int i;
//#ifdef MP_DEBUG
//  assert(cmd != NULL);
//#endif
    if ( !cmd ) return;

    if(cmd->name) delete cmd->name;

    for(i=0; i < MP_CMD_MAX_ARGS && cmd->args[i].type != -1; i++) {
	if(cmd->args[i].type == MP_CMD_ARG_STRING && cmd->args[i].v.s != NULL)
	    delete cmd->args[i].v.s;
    }
    delete cmd;
}

static mp_cmd_t* mp_cmd_clone(mp_cmd_t* cmd) {
    mp_cmd_t* ret;
    int i;
#ifdef MP_DEBUG
    assert(cmd != NULL);
#endif

    ret = new(zeromem) mp_cmd_t;
    memcpy(ret,cmd,sizeof(mp_cmd_t));
    if(cmd->name) ret->name = mp_strdup(cmd->name);
    for(i = 0;  i < MP_CMD_MAX_ARGS && cmd->args[i].type != -1; i++) {
	if(cmd->args[i].type == MP_CMD_ARG_STRING && cmd->args[i].v.s != NULL)
	    ret->args[i].v.s = mp_strdup(cmd->args[i].v.s);
    }
    return ret;
}

static const char* mp_input_get_key_name(libinput_t& priv,int key) {
    unsigned i;

    for(i = 0; key_names[i].name != NULL; i++) {
	if(key_names[i].key == key) return key_names[i].name;
    }

    if(isascii(key)) {
	snprintf(priv.key_str,12,"%c",(char)key);
	return priv.key_str;
    }

    // Print the hex key code
    snprintf(priv.key_str,12,"%#-8x",key);
    return priv.key_str;
}

static int mp_input_get_key_from_name(const char* name) {
    int i,ret = 0,len = strlen(name);
    if(len == 1) { // Direct key code
	ret = (unsigned char)name[0];
	return ret;
    } else if(len > 2 && strncasecmp("0x",name,2) == 0) return strtol(name,NULL,16);

    for(i = 0; key_names[i].name != NULL; i++) {
	if(strcasecmp(key_names[i].name,name) == 0)
	    return key_names[i].key;
    }
    return -1;
}

static int mp_input_get_input_from_name(char* name,int* keys) {
    char *end;
    char *ptr;
    int n=0;

    ptr = name;
    n = 0;
    for(end = strchr(ptr,'-') ; ptr != NULL ; end = strchr(ptr,'-')) {
	if(end && end[1] != '\0') {
	    if(end[1] == '-') end = &end[1];
	    end[0] = '\0';
	}
	keys[n] = mp_input_get_key_from_name(ptr);
	if(keys[n] < 0) return 0;
	n++;
	if(end && end[1] != '\0' && n < MP_MAX_KEY_DOWN) ptr = &end[1];
	else break;
    }
    keys[n] = 0;
    return 1;
}

static void mp_input_bind_keys(libinput_t& priv,int keys[MP_MAX_KEY_DOWN+1],const char* cmd) {
    int i = 0,j;
    mp_cmd_bind_t* _bind = NULL;

#ifdef MP_DEBUG
    assert(keys != NULL);
    assert(cmd != NULL);
#endif

    if(priv.cmd_binds) {
	for(i = 0; priv.cmd_binds[i].cmd != NULL ; i++) {
	    for(j = 0 ; priv.cmd_binds[i].input[j] == keys[j]  && keys[j] != 0 ; j++) /* NOTHING */;
	    if(keys[j] == 0 && priv.cmd_binds[i].input[j] == 0 ) {
		_bind = &priv.cmd_binds[i];
		break;
	    }
	}
    }

    if(!_bind) {
	priv.cmd_binds = (mp_cmd_bind_t*)mp_realloc(priv.cmd_binds,(i+2)*sizeof(mp_cmd_bind_t));
	memset(&priv.cmd_binds[i],0,2*sizeof(mp_cmd_bind_t));
	_bind = &priv.cmd_binds[i];
    }
    if(_bind->cmd) delete _bind->cmd;
    _bind->cmd = mp_strdup(cmd);
    memcpy(_bind->input,keys,(MP_MAX_KEY_DOWN+1)*sizeof(int));
}

static void mp_input_free_binds(mp_cmd_bind_t* binds) {
    int i;
    if(!binds) return;
    for(i = 0; binds[i].cmd != NULL; i++) delete binds[i].cmd;
    delete binds;
}

static const int BS_MAX=256;
static const char* SPACE_CHAR=" \n\r\t";

static int mp_input_parse_config(libinput_t& priv,const std::string& file) {
    int fd;
    int bs = 0,r,eof = 0,comments = 0;
    char *iter,*end;
    char buffer[BS_MAX];
    int n_binds = 0, keys[MP_MAX_KEY_DOWN+1] = { 0 };
    mp_cmd_bind_t* binds = NULL;

    fd = ::open(file.c_str(),O_RDONLY);

    if(fd < 0) {
	mpxp_err<<"Can't open input config file "<<file<<" : "<<strerror(errno)<<std::endl;
	return 0;
    }

    mpxp_v<<"Parsing input config file "<<file<<std::endl;

    while(1) {
	if(! eof && bs < BS_MAX-1) {
	    if(bs > 0) bs--;
	    r = read(fd,buffer+bs,BS_MAX-1-bs);
	    if(r < 0) {
		if(errno == EINTR) continue;
		mpxp_err<<"Error while reading input config file "<<file<<" : "<<strerror(errno)<<std::endl;
		mp_input_free_binds(binds);
		close(fd);
		return 0;
	    } else if(r == 0) {
		eof = 1;
	    } else {
		bs += r+1;
		buffer[bs-1] = '\0';
	    }
	}
	// Empty buffer : return
	if(bs <= 1) {
	    mpxp_info<<"Input config file "<<file<<" parsed : "<<n_binds<<" binds"<<std::endl;
	    if(binds) priv.cmd_binds = binds;
	    close(fd);
	    return 1;
	}
	iter = buffer;
	if(comments) {
	    for( ; iter[0] != '\0' && iter[0] != '\n' ; iter++)/* NOTHING */;
	    if(iter[0] == '\0') { // Buffer was full of comment
		bs = 0;
		continue;
	    }
	    iter++;
	    r = strlen(iter);
	    if(r) memmove(buffer,iter,r+1);
	    bs = r+1;
	    if(iter[0] != '#') comments = 0;
	    continue;
	}
	// Find the wanted key
	if(keys[0] == 0) {
	    // Jump beginning space
	    for(  ; iter[0] != '\0' && strchr(SPACE_CHAR,iter[0]) != NULL ; iter++)/* NOTHING */;
	    if(iter[0] == '\0') { // Buffer was full of space char
		bs = 0;
		continue;
	    }
	    if(iter[0] == '#') { // Comments
		comments = 1;
		continue;
	    }
	    // Find the end of the key code name
	    for(end = iter; end[0] != '\0' && strchr(SPACE_CHAR,end[0]) == NULL ; end++)/*NOTHING */;
	    if(end[0] == '\0') { // Key name don't fit in the buffer
		if(buffer == iter) {
		    if(eof && (buffer-iter) == bs) mpxp_err<<"Unfinished binding "<<iter<<std::endl;
		    else mpxp_err<<"Buffer is too small for this key name : "<<iter<<std::endl;
		    mp_input_free_binds(binds);
		    return 0;
		}
		memmove(buffer,iter,end-iter);
		bs = end-iter;
		continue;
	    }
	    char name[end-iter+1];
	    strncpy(name,iter,end-iter);
	    name[end-iter] = '\0';
	    if(! mp_input_get_input_from_name(name,keys)) {
		mpxp_err<<"Unknown key '"<<name<<"'"<<std::endl;
		mp_input_free_binds(binds);
		close(fd);
		return 0;
	    }
	    if( bs > (end-buffer)) memmove(buffer,end,bs - (end-buffer));
	    bs -= end-buffer;
	    continue;
	} else { // Get the command
	    while(iter[0] == ' ' || iter[0] == '\t') iter++;
	    // Found new line
	    if(iter[0] == '\n' || iter[0] == '\r') {
		int i;
		mpxp_err<<"No command found for key "<<mp_input_get_key_name(priv,keys[0]);
		for(i = 1; keys[i] != 0 ; i++) mpxp_err<<std::left<<mp_input_get_key_name(priv,keys[i]);
		mpxp_err<<std::endl;
		keys[0] = 0;
		if(iter > buffer) {
		    memmove(buffer,iter,bs- (iter-buffer));
		    bs -= (iter-buffer);
		}
		continue;
	    }
	    for(end = iter ; end[0] != '\n' && end[0] != '\r' && end[0] != '\0' ; end++)/* NOTHING */;
	    if(end[0] == '\0' && ! (eof && ((end+1) - buffer) == bs)) {
		if(iter == buffer) {
		    mpxp_err<<"Buffer is too small for command "<<buffer<<std::endl;
		    mp_input_free_binds(binds);
		    close(fd);
		    return 0;
		}
		memmove(buffer,iter,end - iter);
		bs = end - iter;
		continue;
	    }
	    char cmd[end-iter+1];
	    strncpy(cmd,iter,end-iter);
	    cmd[end-iter] = '\0';
	    //printf("Set bind %d => %s\n",keys[0],cmd);
	    mp_input_bind_keys(priv,keys,cmd);
	    n_binds++;
	    keys[0] = 0;
	    end++;
	    if(bs > (end-buffer)) memmove(buffer,end,bs-(end-buffer));
	    bs -= (end-buffer);
	    buffer[bs-1] = '\0';
	    continue;
	}
    }
    mpxp_err<<"What are we doing here ?"<<std::endl;
    close(fd);
    return 0;
}

static void mp_input_init(libinput_t& priv) {
    std::string file;

    file = config_file[0] != '/' ? get_path(priv.envm,config_file) : config_file;
    if(file.empty()) return;

    if(! mp_input_parse_config(priv,file)) {
	// Try global conf dir
	file = std::string(CONFDIR)+"/input.conf";
	if(! mp_input_parse_config(priv,file)) mpxp_warn<<"Falling back on default (hardcoded) input config"<<std::endl;
    }
#ifdef HAVE_JOYSTICK
    if(libinput_conf.use_joystick) {
	any_t* joystick_fd;
	joystick_fd = mp_input_joystick_open(libinput_conf.js_dev);
	if(!joystick_fd) mpxp_err<<"Can't init input joystick with using: "<<libinput_conf.js_dev<<std::endl;
	else		 mp_input_add_key_fd(priv,joystick_fd,1,mp_input_joystick_read,(mp_close_func_t)mp_input_joystick_close);
    }
#endif

#ifdef HAVE_LIRC
    if(libinput_conf.use_lirc) {
	any_t* lirc_fd = mp_input_lirc_open();
	if(lirc_fd) mp_input_add_cmd_fd(priv,lirc_fd,0,mp_input_lirc_read_cmd,mp_input_lirc_close);
    }
#endif

#ifdef HAVE_LIRCC
    if(libinput_conf.use_lircc) {
	int fd = lircc_init("mplayer", NULL);
	if(fd >= 0) mp_input_add_cmd_fd(fd,1,NULL,(mp_close_func_t)lircc_cleanup);
    }
#endif
    if(libinput_conf.in_file) {
	struct stat st;
	if(stat(libinput_conf.in_file,&st)) mpxp_err<<"Can't stat: "<<libinput_conf.in_file<<" : "<<strerror(errno)<<std::endl;
	else {
	    priv.in_file_fd = open(libinput_conf.in_file,(S_ISFIFO(st.st_mode)?O_RDWR:O_RDONLY)|O_NONBLOCK);
	    if(priv.in_file_fd >= 0) mp_input_add_cmd_fd(priv,&priv,1,NULL,(mp_close_func_t)close);
	    else mpxp_err<<"Can't open: "<<libinput_conf.in_file<<" : "<<strerror(errno)<<std::endl;
	}
    }
    priv.in_file_fd = 0;
    getch2_enable();  // prepare stdin for hotkeys...
    mp_input_add_key_fd(priv,&priv,1,NULL,(mp_close_func_t)close);
}

static void mp_input_uninit(libinput_t& priv) {
    unsigned int i;

    for(i=0; i < priv.num_key_fd; i++) {
	if(priv.key_fds[i].close_func) priv.key_fds[i].close_func(priv.key_fds[i].opaque);
    }

    for(i=0; i < priv.num_cmd_fd; i++) {
	if(priv.cmd_fds[i].close_func) priv.cmd_fds[i].close_func(priv.cmd_fds[i].opaque);
    }

    if(priv.cmd_binds) {
	for(i=0;;i++) {
	    if(priv.cmd_binds[i].cmd != NULL) delete priv.cmd_binds[i].cmd;
	    else break;
	}
	delete priv.cmd_binds;
	priv.cmd_binds=NULL;
    }
    if(priv.in_file_fd==0) getch2_disable();
}

libinput_t& mp_input_open(const std::map<std::string,std::string>& envm) {
    libinput_t& priv=*new(zeromem) libinput_t(envm);
    priv.ar_state=-1;
    priv.in_file_fd=-1;
    mp_input_init(priv);
    if(libinput_conf.print_key_list) mp_input_print_key_list(priv);
    if(libinput_conf.print_cmd_list) mp_input_print_cmd_list(priv);
    return priv;
}

void mp_input_close(libinput_t& handle) {
    mp_input_uninit(handle);
    delete &handle;
}

void mp_input_register_options(M_Config& cfg) {
    cfg.register_options(mp_input_opts);
}

void mp_input_print_keys(libinput_t& handle) {
    unsigned i;
    UNUSED(handle);
    mpxp_info<<"List of available KEYS:"<<std::endl;
    for(i= 0; key_names[i].name != NULL ; i++) mpxp_info<<key_names[i].name<<std::endl;
}

static int mp_input_print_key_list(libinput_t& handle) {
    mp_input_print_keys(handle);
    exit(0);
}

void mp_input_print_binds(libinput_t& handle) {
    unsigned i,j;
    mpxp_info<<"List of available key bindings:"<<std::endl;
    for(i=0; def_cmd_binds[i].cmd != NULL ; i++) {
	for(j=0;def_cmd_binds[i].input[j] != 0;j++) {
	    mpxp_info<<std::left<<"  "<<mp_input_get_key_name(handle,def_cmd_binds[i].input[j]);
	}
	mpxp_info<<" "<<def_cmd_binds[i].cmd<<std::endl;
    }
}

void mp_input_print_cmds(libinput_t& handle) {
    const mp_cmd_t *cmd;
    int i,j;
    const char* type;

    UNUSED(handle);
    mpxp_info<<"List of available input commands:"<<std::endl;
    for(i = 0; (cmd = &mp_cmds[i])->name != NULL ; i++) {
	mpxp_info<<std::left<<"  "<<cmd->name;
	for(j= 0 ; j < MP_CMD_MAX_ARGS && cmd->args[j].type != -1 ; j++) {
	    switch(cmd->args[j].type) {
		case MP_CMD_ARG_INT:
		    type = "Integer";
		    break;
		case MP_CMD_ARG_FLOAT:
		    type = "Float";
		    break;
		case MP_CMD_ARG_STRING:
		    type = "String";
		    break;
		default:
		    type = "??";
	    }
	    if(j+1 > cmd->nargs) mpxp_info<<" ["<<type<<"]";
	    else mpxp_info<<" "<<type;
	}
	mpxp_info<<std::endl;
    }
}

static int mp_input_print_cmd_list(libinput_t& handle) {
    mp_input_print_cmds(handle);
    exit(0);
}

MPXP_Rc mp_input_check_interrupt(libinput_t& handle,int tim) {
    libinput_t& priv = (libinput_t&)handle;
    mp_cmd_t* cmd;
    if((cmd = mp_input_get_cmd(handle,tim,0,1)) == NULL) return MPXP_False;
    switch(cmd->id) {
	case MP_CMD_QUIT:
	case MP_CMD_SOFT_QUIT:
	case MP_CMD_PLAY_TREE_STEP:
	case MP_CMD_PLAY_TREE_UP_STEP:
	case MP_CMD_PLAY_ALT_SRC_STEP:
	    // The cmd will be executed when we are back in the main loop
	    return MPXP_Ok; //<-- memory leaks here
    }
    // remove the cmd from the queue
    cmd = mp_input_get_cmd(priv,tim,0,0);
    mp_cmd_free(cmd);
    return MPXP_False;
}

} // namespace mpxp
