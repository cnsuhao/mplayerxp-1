#ifndef INPUT_H_INCLUDED
#define INPUT_H_ICLUDED 1

// All commands id
enum {
    MP_CMD_SEEK			=0,
    MP_CMD_RESERVED_0		=1,
    MP_CMD_QUIT			=2,
    MP_CMD_PAUSE		=3,
    MP_CMD_SOFT_QUIT		=4,
    MP_CMD_PLAY_TREE_STEP	=5,
    MP_CMD_PLAY_TREE_UP_STEP	=6,
    MP_CMD_PLAY_ALT_SRC_STEP	=7,
    MP_CMD_RESERVED_1		=8,
    MP_CMD_OSD			=9,
    MP_CMD_VOLUME		=10,
    MP_CMD_RESERVED_2		=11,
    MP_CMD_CONTRAST		=12,
    MP_CMD_BRIGHTNESS		=13,
    MP_CMD_HUE			=14,
    MP_CMD_SATURATION		=15,
    MP_CMD_FRAMEDROPPING	=16,
    MP_CMD_TV_STEP_CHANNEL	=17,
    MP_CMD_TV_STEP_NORM		=18,
    MP_CMD_TV_STEP_CHANNEL_LIST	=19,
    MP_CMD_VO_FULLSCREEN	=20,
    MP_CMD_SUB_POS		=21,
    MP_CMD_DVDNAV		=22,
    MP_CMD_VO_SCREENSHOT	=23,
    MP_CMD_PANSCAN		=24,
    MP_CMD_MUTE			=25,
    MP_CMD_LOADFILE		=26,
    MP_CMD_LOADLIST		=27,
    MP_CMD_VF_CHANGE_RECTANGLE	=28,
    MP_CMD_GAMMA		=29,
    MP_CMD_SUB_VISIBILITY	=30,
    MP_CMD_VOBSUB_LANG		=31,
    MP_CMD_MENU			=32,
    MP_CMD_SET_MENU		=33,
    MP_CMD_GET_TIME_LENGTH	=34,
    MP_CMD_GET_PERCENT_POS	=35,
    MP_CMD_SUB_STEP		=36,
    MP_CMD_TV_SET_CHANNEL	=37,
#ifdef USE_EDL
    MP_CMD_EDL_MARK		=38,
#endif
    MP_CMD_SUB_ALIGNMENT	=39,
    MP_CMD_TV_LAST_CHANNEL	=40,
    MP_CMD_OSD_SHOW_TEXT	=41,
    MP_CMD_TV_STEP_CHANNEL_UP	=42,
    MP_CMD_TV_STEP_CHANNEL_DOWN	=43,

    MP_CMD_SPEED_INCR		=44,
    MP_CMD_SPEED_MULT		=45,
    MP_CMD_SPEED_SET		=46,

    MP_CMD_SWITCH_AUDIO		=47,
    MP_CMD_SWITCH_VIDEO		=48,
    MP_CMD_SWITCH_SUB		=49,

    MP_CMD_FRAME_STEP		=56,

    MP_CMD_DVDNAV_EVENT		=6000,
/// Console command
    MP_CMD_CHELP		=7000,
    MP_CMD_CEXIT		=7001,
    MP_CMD_CHIDE		=7002,
    MP_CMD_CRUN			=7003
};

enum {
    MP_CMD_DVDNAV_UP	=1,
    MP_CMD_DVDNAV_DOWN	=2,
    MP_CMD_DVDNAV_LEFT	=3,
    MP_CMD_DVDNAV_RIGHT	=4,
    MP_CMD_DVDNAV_MENU	=5,
    MP_CMD_DVDNAV_SELECT=6,
};
enum {
// The args types
    MP_CMD_ARG_INT	=0,
    MP_CMD_ARG_FLOAT	=1,
    MP_CMD_ARG_STRING	=2,
    MP_CMD_ARG_VOID	=3,

#ifndef MP_CMD_MAX_ARGS
    MP_CMD_MAX_ARGS	=10
#endif
};
// Error codes for the drivers

enum {
    MP_INPUT_ERROR	=-1,// An error occured but we can continue
    MP_INPUT_DEAD	=-2,// A fatal error occured, this driver should be removed
    MP_INPUT_NOTHING	=-3 // No input were avaible
};
// For the keys drivers, if possible you can send key up and key down
// events. Key up is the default, to send a key down you must or the key
// code with MP_KEY_DOWN
enum {
    MP_KEY_DOWN		=(1<<29),
    MP_NO_REPEAT_KEY	=(1<<28), // Use this when the key shouldn't be auto-repeated (like mouse buttons)
    MP_MAX_KEY_DOWN	=32
};

typedef union mp_cmd_arg_value {
  int i;
  float f;
  char* s;
  any_t* v;
} mp_cmd_arg_value_t;

typedef struct mp_cmd_arg {
  int type;
  mp_cmd_arg_value_t v;
} mp_cmd_arg_t;

typedef struct mp_cmd {
  int id;
  char* name;
  int nargs;
  mp_cmd_arg_t args[MP_CMD_MAX_ARGS];
} mp_cmd_t;


typedef struct mp_cmd_bind {
  int input[MP_MAX_KEY_DOWN+1];
  char* cmd;
} mp_cmd_bind_t;

typedef struct mp_key_name {
  int key;
  char* name;
} mp_key_name_t;

// These typedefs are for the drivers. They are the functions used to retrive
// the next key code or command.

// These functions should return the key code or one of the error code
typedef int (*mp_key_func_t)(int fd);
// These functions should act like read but they must use our error code (if needed ;-)
typedef int (*mp_cmd_func_t)(int fd,char* dest,int size);
// These are used to close the driver
typedef void (*mp_close_func_t)(int fd);

// Set this to grab all incoming key code 
extern void (*mp_input_key_cb)(int code);
// Should return 1 if the command was processed
typedef int (*mp_input_cmd_filter)(mp_cmd_t* cmd, int paused, any_t* ctx);

// This function add a new key driver.
// The first arg is a file descriptor (use a negative value if you don't use any fd)
// The second arg tell if we use select on the fd to know if something is avaible.
// The third arg is optional. If null a default function wich read an int from the
// fd will be used.
// The last arg can be NULL if nothing is needed to close the driver. The close
// function can be used
int
mp_input_add_cmd_fd(int fd, int select, mp_cmd_func_t read_func, mp_close_func_t close_func);

// This remove a cmd driver, you usally don't need to use it
void
mp_input_rm_cmd_fd(int fd);

// The args are the sames as for the keys drivers. If you don't use any valid fd you MUST
// give a read_func.
int
mp_input_add_key_fd(int fd, int select, mp_key_func_t read_func, mp_close_func_t close_func);

// As for the cmd one you usally don't need this function
void
mp_input_rm_key_fd(int fd);

// This function can be used to reput a command in the system. It's used by libmpdemux
// when it perform a blocking operation to resend the command it received to the main
// loop.
int
mp_input_queue_cmd(mp_cmd_t* cmd);

// This function retrive the next avaible command waiting no more than time msec.
// If pause is true, the next input will always return a pause command.
mp_cmd_t*
mp_input_get_cmd(int time, int paused, int peek_only);

mp_cmd_t*
mp_input_parse_cmd(char* str);

/// These filter allow you to process the command before mplayer
/// If a filter return a true value mp_input_get_cmd will return NULL
void
mp_input_add_cmd_filter(mp_input_cmd_filter, any_t* ctx);

// After getting a command from mp_input_get_cmd you need to mp_free it using this
// function
void
mp_cmd_free(mp_cmd_t* cmd);

// This create a copy of a command (used by the auto repeat stuff)
mp_cmd_t*
mp_cmd_clone(mp_cmd_t* cmd);

// When you create a new driver you should add it in this 2 functions.
void
mp_input_init(void);

void
mp_input_uninit(void);

// Interruptible usleep:  (used by libmpdemux)
int
mp_input_check_interrupt(int time);

void mp_input_print_keys(void);
void mp_input_print_cmds(void);
void mp_input_print_binds(void);
#endif
