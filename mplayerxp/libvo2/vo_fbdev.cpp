#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 */
#include <iostream>
#include <fstream>
#include <stdexcept>

static const char* FBDEV= "fbdev: ";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <linux/fb.h>

#include "mplayerxp.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "osdep/fastmemcpy.h"
#include "sub.h"
#ifdef CONFIG_VIDIX
#include "vidix_system.h"
#endif
#include "aspect.h"
#include "dri_vo.h"
#include "libmpstream2/mrl.h"
#include "vo_msg.h"

namespace	usr {
struct priv_conf_t {
    priv_conf_t();
    ~priv_conf_t() {}
/* command line/config file options */
    char*		dev_name;
    const char*		mode_cfgfile;
    char*		mode_name;
    const char*		monitor_hfreq_str;
    const char*		monitor_vfreq_str;
    const char*		monitor_dotclock_str;
};
static priv_conf_t priv_conf;
static const int MAX_NR_TOKEN=16;
static const int MAX_LINE_LEN=1000;
static const int RET_EOF=-1;
static const int RET_EOL=-2;
priv_conf_t::priv_conf_t() {
    mode_cfgfile = "/etc/priv.modes";
}
inline void PRINT_LINENUM(int line_num) { mpxp_dbg2<<" at line "<<line_num<<std::endl; }

static const mrl_config_t fbconf[]=
{
    { "modeconfig", &priv_conf.mode_cfgfile, MRL_TYPE_STRING, 0, 0 },
    { "hfreq", &priv_conf.monitor_hfreq_str, MRL_TYPE_STRING, 0, 0 },
    { "vfreq", &priv_conf.monitor_vfreq_str, MRL_TYPE_STRING, 0, 0 },
    { "dotclock", &priv_conf.monitor_dotclock_str, MRL_TYPE_STRING, 0, 0 },
    { NULL, NULL, 0, 0, 0 },
};

/******************************
*	fb.modes support      *
******************************/

typedef struct {
    char *name;
    uint32_t xres, yres, vxres, vyres, depth;
    uint32_t pixclock, left, right, upper, lower, hslen, vslen;
    uint32_t sync;
    uint32_t vmode;
} fb_mode_t;

static int __FASTCALL__ validate_mode(fb_mode_t *m)
{
    if (!m->xres) {
	mpxp_v<<"needs geometry ";
	return 0;
    }
    if (!m->pixclock) {
	mpxp_v<<"needs timings ";
	return 0;
    }
    return 1;
}

/******************************
*	    vo_fbdev	      *
******************************/
typedef struct {
    float min;
    float max;
} range_t;


class FBDev_VO_Interface : public VO_Interface {
    public:
	FBDev_VO_Interface(const std::string& args);
	virtual ~FBDev_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const std::string& title,
				uint32_t format);
	virtual MPXP_Rc	select_frame(unsigned idx);
	virtual MPXP_Rc	flush_page(unsigned idx);
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const;
	virtual void	get_surface(dri_surface_t *surf);
	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	MPXP_Rc		fb_preinit();
	std::string	parse_sub_device(const std::string& sd);
	int		parse_fbmode_cfg(const std::string& cfgfile);
	int		get_token(std::ifstream&,int num);
	void		vt_set_textarea(int u, int l);
	void		lots_of_printf() const;

	LocalPtr<Aspect>aspect;
	int		line_num;
	char *		line;
	char *		token[MAX_NR_TOKEN];
	uint32_t	srcFourcc,dstFourcc;
	unsigned	flags;
/* fb.modes related variables */
	range_t *	monitor_hfreq;
	range_t *	monitor_vfreq;
	range_t *	monitor_dotclock;
	fb_mode_t *	mode;
/* vt related variables */
	std::ofstream	vt_fp;
	int		vt_doit;
/* vo_fbdev related variables */
	int		dev_fd;
	int		tty_fd;
	size_t		size;
	uint8_t *	frame_buffer;
	uint8_t *	L123123875;	/* thx .so :) */
	struct fb_fix_screeninfo	finfo;
	struct fb_var_screeninfo	orig_vinfo;
	struct fb_var_screeninfo	vinfo;
	struct fb_cmap	oldcmap;
	int		cmap_changed;
	unsigned	pixel_size;	// 32:  4  24:  3  16:  2  15:  2
	uint32_t	pixel_format;
	unsigned	real_bpp;	// 32: 24  24: 24  16: 16  15: 15
	unsigned	bpp;		// 32: 32  24: 24  16: 16  15: 15
	unsigned	bpp_we_want;	// 32: 32  24: 24  16: 16  15: 15
	unsigned	line_len;
	unsigned	xres;
	unsigned	yres;

	uint8_t *	next_frame[MAX_DRI_BUFFERS];
	unsigned	total_fr;
	int		in_width;
	int		in_height;
	unsigned	out_width;
	unsigned	out_height;
	int		last_row;
	MPXP_Rc		pre_init_err;
#ifdef CONFIG_VIDIX
/* Name of VIDIX driver */
	Vidix_System*	vidix;
#endif
	int		fb_preinit_done;
	MPXP_Rc		fb_works;
	std::string	dev_name;
};

std::string FBDev_VO_Interface::parse_sub_device(const std::string& sd)
{
    const char *param;
#ifdef CONFIG_VIDIX
    if(sd.substr(0,5)=="vidix") return &sd[5]; /* vidix_name will be valid within init() */
    else
#endif
    {
	param=mrl_parse_line(sd,NULL,NULL,&priv_conf.dev_name,&priv_conf.mode_name);
	mrl_parse_params(param,fbconf);
    }
    return NULL;
}

MPXP_Rc FBDev_VO_Interface::fb_preinit()
{
    fb_preinit_done = 0;
    fb_works = MPXP_Ok;
    vt_doit = 1;

    if (fb_preinit_done) return fb_works;

    dev_name="";
    const std::map<std::string,std::string>& envm=mpxp_get_environment();
    std::map<std::string,std::string>::const_iterator it;
    it = envm.find("FRAMEBUFFER");
    if(it!=envm.end()) dev_name = (*it).second;

    if(dev_name.empty()) dev_name = "/dev/fb0";
    priv_conf.dev_name = const_cast<char*>(dev_name.c_str());
    mpxp_dbg2<<FBDEV<< "using "<<priv_conf.dev_name<<std::endl;

    if ((dev_fd = ::open(priv_conf.dev_name, O_RDWR)) == -1) {
	mpxp_err<<FBDEV<< "Can't open "<<priv_conf.dev_name<<": "<<strerror(errno)<<std::endl;
	goto err_out;
    }
    if (::ioctl(dev_fd, FBIOGET_VSCREENINFO, &vinfo)) {
	mpxp_err<<FBDEV<< "Can't get VSCREENINFO: "<<strerror(errno)<<std::endl;
	goto err_out_fd;
    }
    orig_vinfo = vinfo;

    if ((tty_fd = ::open("/dev/tty", O_RDWR)) < 0) {
	mpxp_dbg2<<FBDEV<< "notice: Can't open /dev/tty: "<<strerror(errno)<<std::endl;
    }

    bpp = vinfo.bits_per_pixel;

    if (bpp == 8 && !vo_conf.dbpp) {
	mpxp_err<<FBDEV<< "8 bpp output is not supported."<<std::endl;
	goto err_out_tty_fd;
    }

    /* 16 and 15 bpp is reported as 16 bpp */
    if (bpp == 16)
	bpp = vinfo.red.length + vinfo.green.length + vinfo.blue.length;

    if (vo_conf.dbpp) {
	if (vo_conf.dbpp != 15 && vo_conf.dbpp != 16 && vo_conf.dbpp != 24 && vo_conf.dbpp != 32) {
		mpxp_err<<FBDEV<< "can't switch to "<<vo_conf.dbpp<<" bpp"<<std::endl;
		goto err_out_fd;
	}
	bpp = vo_conf.dbpp;
    }

    fb_preinit_done = 1;
    pre_init_err= MPXP_Ok;
    fb_works = MPXP_Ok;
    return MPXP_Ok;
err_out_tty_fd:
    ::close(tty_fd);
    tty_fd = -1;
err_out_fd:
    ::close(dev_fd);
    dev_fd = -1;
err_out:
    fb_preinit_done = 1;
    fb_works = MPXP_False;
    pre_init_err= MPXP_Error;
    return MPXP_False;
}

FBDev_VO_Interface::~FBDev_VO_Interface()
{
    unsigned i;
    mpxp_v<<FBDEV<< "uninit"<<std::endl;
    if (cmap_changed) {
	if (::ioctl(dev_fd, FBIOPUTCMAP, &oldcmap))
	    mpxp_err<<FBDEV<< "Can't restore original cmap"<<std::endl;
	cmap_changed = 0;
    }
    for(i=0;i<total_fr;i++) delete next_frame[i];
    if (::ioctl(dev_fd, FBIOGET_VSCREENINFO, &vinfo))
	mpxp_err<<FBDEV<< "ioctl FBIOGET_VSCREENINFO: "<<strerror(errno)<<std::endl;
    orig_vinfo.xoffset = vinfo.xoffset;
    orig_vinfo.yoffset = vinfo.yoffset;
    if (::ioctl(dev_fd, FBIOPUT_VSCREENINFO, &orig_vinfo))
	mpxp_err<<FBDEV<< "Can't reset original fb_var_screeninfo: "<<strerror(errno)<<std::endl;
    if (tty_fd >= 0) {
		if (::ioctl(tty_fd, KDSETMODE, KD_TEXT) < 0)
		    mpxp_err<<FBDEV<< "Can't restore text mode: "<<strerror(errno)<<std::endl;
    }
    if (vt_doit) vt_set_textarea(0, orig_vinfo.yres);
    ::close(tty_fd);
    ::close(dev_fd);
    if(frame_buffer) ::munmap(frame_buffer,size);
#ifdef CONFIG_VIDIX
    if(vidix) delete vidix;
#endif
}

FBDev_VO_Interface::FBDev_VO_Interface(const std::string& arg)
		    :VO_Interface(arg),
		    aspect(new(zeromem) Aspect(mp_conf.monitor_pixel_aspect))
{
    std::string vidix_name;
    if(!arg.empty()) vidix_name=parse_sub_device(arg);
#ifdef CONFIG_VIDIX
    if(!vidix_name.empty()) {
	if(!(vidix=new(zeromem) Vidix_System(vidix_name))) {
	    mpxp_err<<"Cannot initialze vidix with '"<<vidix_name<<"' argument"<<std::endl;
	    throw std::runtime_error("Vidix error");
	}
    }
#endif
    if(fb_preinit()!=MPXP_Ok) throw std::runtime_error("FBDev preinit");
}

int FBDev_VO_Interface::get_token(std::ifstream& fp,int num)
{
    static int read_nextline = 1;
    static int line_pos;
    int i;
    char c;

    if (num >= MAX_NR_TOKEN) {
	mpxp_err<<"get_token(): max >= MAX_NR_TOKEN!"<<std::endl;
	goto out_eof;
    }

    if (read_nextline) {
	fp.getline(line, MAX_LINE_LEN);
	if (!fp.good()) goto out_eof;
	line_pos = 0;
	++line_num;
	read_nextline = 0;
    }
    for (i = 0; i < num; i++) {
	while (isspace(line[line_pos])) ++line_pos;
	if (line[line_pos] == '\0' || line[line_pos] == '#') {
	    read_nextline = 1;
	    goto out_eol;
	}
	token[i] = line + line_pos;
	c = line[line_pos];
	if (c == '"' || c == '\'') {
	    token[i]++;
	    while (line[++line_pos] != c && line[line_pos]) /* NOTHING */;
	    if (!line[line_pos]) goto out_eol;
	    line[line_pos] = ' ';
	} else {
	    for (/* NOTHING */; !isspace(line[line_pos]) && line[line_pos]; line_pos++) /* NOTHING */;
	}
	if (!line[line_pos]) {
	    read_nextline = 1;
	    if (i == num - 1) goto out_ok;
	    goto out_eol;
	}
	line[line_pos++] = '\0';
    }
out_ok:
    return i;
out_eof:
    return RET_EOF;
out_eol:
    return RET_EOL;
}

static fb_mode_t *fb_modes = NULL;
static int nr_modes = 0;

static void check_in_mode_def(int in_mode_def) { if (!in_mode_def) mpxp_dbg2<<"'needs 'mode' first"<<std::endl; }

int FBDev_VO_Interface::parse_fbmode_cfg(const std::string& cfgfile)
{
    fb_mode_t *_mode = NULL;
    char *endptr;	// strtoul()...
    int in_mode_def = 0;
    int tmp, i;
    std::ifstream fp;

    mpxp_dbg2<<"Reading "<<cfgfile.c_str()<<":";

    fp.open(cfgfile.c_str(),std::ios_base::in);
    if (!fp.is_open()) {
	mpxp_err<<"can't open '"<<cfgfile<<"': "<<strerror(errno)<<std::endl;
	return -1;
    }

    if ((line = new char[MAX_LINE_LEN + 1]) == NULL) {
	mpxp_err<<"can't get memory for 'line': "<<strerror(errno)<<std::endl;
	return -2;
    }

    /*
     * check if the cfgfile starts with 'mode'
     */
    while ((tmp = get_token(fp,1)) == RET_EOL) /* NOTHING */;
    if (tmp == RET_EOF) goto out;
    if (!strcmp(token[0], "mode")) goto loop_enter;
    goto err_out_parse_error;

    while ((tmp = get_token(fp,1)) != RET_EOF) {
	if (tmp == RET_EOL) continue;
	if (!strcmp(token[0], "mode")) {
	    if (in_mode_def) {
		mpxp_err<<"'endmode' required"<<std::endl;
		goto err_out_print_linenum;
	    }
	    if (!validate_mode(_mode)) goto err_out_not_valid;
	    loop_enter:
	    if (!(fb_modes = (fb_mode_t *) mp_realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
		mpxp_err<<"can't mp_realloc 'fb_modes' (nr_modes = "<<nr_modes<<")"<<strerror(errno)<<std::endl;
		goto err_out;
	    }
	    _mode=fb_modes + nr_modes;
	    ++nr_modes;
	    memset(_mode,0,sizeof(fb_mode_t));

	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    for (i = 0; i < nr_modes - 1; i++) {
		if (!strcmp(token[0], fb_modes[i].name)) {
		    mpxp_err<<"mode name '"<<token[0]<<"' isn't unique"<<std::endl;
		    goto err_out_print_linenum;
		}
	    }
	    if (!(_mode->name = mp_strdup(token[0]))) {
		mpxp_err<<"can't mp_strdup -> 'name': "<<strerror(errno)<<std::endl;
		goto err_out;
	    }
	    in_mode_def = 1;
	} else if (!strcmp(token[0], "geometry")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,5) < 0) goto err_out_parse_error;
	    _mode->xres = strtoul(token[0], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->yres = strtoul(token[1], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->vxres = strtoul(token[2], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->vyres = strtoul(token[3], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->depth = strtoul(token[4], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	} else if (!strcmp(token[0], "timings")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,7) < 0) goto err_out_parse_error;
	    _mode->pixclock = strtoul(token[0], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->left = strtoul(token[1], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->right = strtoul(token[2], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->upper = strtoul(token[3], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->lower = strtoul(token[4], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->hslen = strtoul(token[5], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    _mode->vslen = strtoul(token[6], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	} else if (!strcmp(token[0], "endmode")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    in_mode_def = 0;
	} else if (!strcmp(token[0], "accel")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    /*
	     * it's only used for text acceleration
	     * so we just ignore it.
	     */
	} else if (!strcmp(token[0], "hsync")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "low")) _mode->sync &= ~FB_SYNC_HOR_HIGH_ACT;
	    else if(!strcmp(token[0], "high")) _mode->sync |= FB_SYNC_HOR_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(token[0], "vsync")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "low")) _mode->sync &= ~FB_SYNC_VERT_HIGH_ACT;
	    else if(!strcmp(token[0], "high")) _mode->sync |= FB_SYNC_VERT_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(token[0], "csync")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "low")) _mode->sync &= ~FB_SYNC_COMP_HIGH_ACT;
	    else if(!strcmp(token[0], "high")) _mode->sync |= FB_SYNC_COMP_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(token[0], "extsync")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "false")) _mode->sync &= ~FB_SYNC_EXT;
	    else if(!strcmp(token[0], "true")) _mode->sync |= FB_SYNC_EXT;
	    else goto err_out_parse_error;
	} else if (!strcmp(token[0], "laced")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "false")) _mode->vmode = FB_VMODE_NONINTERLACED;
	    else if (!strcmp(token[0], "true")) _mode->vmode = FB_VMODE_INTERLACED;
	    else goto err_out_parse_error;
	} else if (!strcmp(token[0], "double")) {
	    check_in_mode_def(in_mode_def); goto err_out_print_linenum;
	    if (get_token(fp,1) < 0) goto err_out_parse_error;
	    if (!strcmp(token[0], "false")) ;
	    else if (!strcmp(token[0], "true")) _mode->vmode = FB_VMODE_DOUBLE;
	    else goto err_out_parse_error;
	} else goto err_out_parse_error;
    }
    if (!validate_mode(_mode)) goto err_out_not_valid;
out:
    mpxp_dbg2<<nr_modes<<"modes"<<std::endl;
    delete line;
    fp.close();
    return nr_modes;
err_out_parse_error:
    mpxp_err<<"parse error";
err_out_print_linenum:
    PRINT_LINENUM(line_num);
err_out:
    if (fb_modes) {
	delete fb_modes;
	fb_modes = NULL;
    }
    nr_modes = 0;
    delete line;
    return -2;
err_out_not_valid:
    mpxp_err<<"previous mode is not correct"<<std::endl;
    goto err_out_print_linenum;
}

static fb_mode_t * __FASTCALL__ find_mode_by_name(const char *name)
{
    int i;

    for (i = 0; i < nr_modes; i++)
	if (!strcmp(name, fb_modes[i].name))
	    return fb_modes + i;
    return NULL;
}

static float __FASTCALL__ dcf(fb_mode_t *m)	//driving clock frequency
{
    return 1e12f / m->pixclock;
}

static float __FASTCALL__ hsf(fb_mode_t *m)	//horizontal scan frequency
{
    int htotal = m->left + m->xres + m->right + m->hslen;
    return dcf(m) / htotal;
}

static float __FASTCALL__ vsf(fb_mode_t *m)	//vertical scan frequency
{
    int vtotal = m->upper + m->yres + m->lower + m->vslen;
    return hsf(m) / vtotal;
}

static int __FASTCALL__ in_range(range_t *r, float f)
{
    for (/* NOTHING */; (r->min != -1 && r->max != -1); r++)
	if (f >= r->min && f <= r->max)
	    return 1;
    return 0;
}

static int __FASTCALL__ mode_works(fb_mode_t *m, range_t *hfreq, range_t *vfreq,
		range_t *dotclock)
{
    float h = hsf(m);
    float v = vsf(m);
    float d = dcf(m);
    int ret = 1;

    mpxp_dbg2<<FBDEV<< "mode "<<m->xres<<"x"<<m->yres<<":";
    if (!in_range(hfreq, h)) {
	ret = 0;
	mpxp_dbg2<<" hsync out of range.";
    }
    if (!in_range(vfreq, v)) {
	ret = 0;
	mpxp_dbg2<<" vsync out of range.";
    }
    if (!in_range(dotclock, d)) {
	ret = 0;
	mpxp_dbg2<<" dotclock out of range.";
    }
    if (ret)	mpxp_dbg2<<" hsync, vsync, dotclock ok."<<std::endl;
    else	mpxp_dbg2<<std::endl;
    return ret;
}

static fb_mode_t * __FASTCALL__ find_best_mode(unsigned xres, unsigned yres, range_t *hfreq,
		range_t *vfreq, range_t *dotclock)
{
    int i;
    fb_mode_t *best = fb_modes;
    fb_mode_t *curr;

    mpxp_dbg2<<FBDEV<< "Searching for first working mode"<<std::endl;

    for (i = 0; i < nr_modes; i++, best++)
	if (mode_works(best, hfreq, vfreq, dotclock))
	    break;

    if (i == nr_modes)
	return NULL;
    if (i == nr_modes - 1)
	return best;

    mpxp_dbg2<<FBDEV<< "First working mode: "<<best->xres<<"x"<<best->yres<<std::endl;
    mpxp_dbg2<<FBDEV<< "Searching for better modes"<<std::endl;

    for (curr = best + 1; i < nr_modes - 1; i++, curr++) {
	if (!mode_works(curr, hfreq, vfreq, dotclock))
	continue;

	mpxp_dbg2<<FBDEV;

	if (best->xres < xres || best->yres < yres) {
	    if (curr->xres > best->xres || curr->yres > best->yres) {
		mpxp_dbg2<<"better than "<<best->xres<<"x"<<best->yres<<", which is too small."<<std::endl;
		best = curr;
	    } else mpxp_dbg2<<"too small."<<std::endl;
	} else if (curr->xres == best->xres && curr->yres == best->yres &&
			vsf(curr) > vsf(best)) {
	    mpxp_dbg2<<"faster screen refresh."<<std::endl;
	    best = curr;
	} else if ((curr->xres <= best->xres && curr->yres <= best->yres) &&
				(curr->xres >= xres && curr->yres >= yres)) {
	    mpxp_dbg2<<"better than "<<best->xres<<"x"<<best->yres<<", which is too large."<<std::endl;
	    best = curr;
	} else {
	    if (curr->xres < xres || curr->yres < yres) mpxp_dbg2<<"too small."<<std::endl;
	    else if (curr->xres > best->xres || curr->yres > best->yres) mpxp_dbg2<<"too large."<<std::endl;
	    else mpxp_dbg2<<"it's worse, don't know why."<<std::endl;
	}
    }
    return best;
}

static void __FASTCALL__ set_bpp(struct fb_var_screeninfo *p, unsigned bpp)
{
    p->bits_per_pixel = (bpp + 1) & ~1;
    p->red.msb_right = p->green.msb_right = p->blue.msb_right = p->transp.msb_right = 0;
    p->transp.offset = p->transp.length = 0;
    p->blue.offset = 0;
    switch (bpp) {
	case 32:
	    p->transp.offset = 24;
	    p->transp.length = 8;
	case 24:
	    p->red.offset = 16;
	    p->red.length = 8;
	    p->green.offset = 8;
	    p->green.length = 8;
	    p->blue.length = 8;
	    break;
	case 16:
	    p->red.offset = 11;
	    p->green.length = 6;
	    p->red.length = 5;
	    p->green.offset = 5;
	    p->blue.length = 5;
	    break;
	case 15:
	    p->red.offset = 10;
	    p->green.length = 5;
	    p->red.length = 5;
	    p->green.offset = 5;
	    p->blue.length = 5;
	    break;
    }
}

static void __FASTCALL__ fb_mode2fb_vinfo(fb_mode_t *m, struct fb_var_screeninfo *v)
{
    v->xres = m->xres;
    v->yres = m->yres;
    v->xres_virtual = m->vxres;
    v->yres_virtual = m->vyres;
    set_bpp(v, m->depth);
    v->pixclock = m->pixclock;
    v->left_margin = m->left;
    v->right_margin = m->right;
    v->upper_margin = m->upper;
    v->lower_margin = m->lower;
    v->hsync_len = m->hslen;
    v->vsync_len = m->vslen;
    v->sync = m->sync;
    v->vmode = m->vmode;
}

static range_t * __FASTCALL__ str2range(const char *s)
{
    float tmp_min, tmp_max;
    const char *endptr = s;	// to start the loop
    range_t *r = NULL;
    int i;

    if (!s) return NULL;
    for (i = 0; *endptr; i++) {
	if (*s == ',') goto out_err;
	if (!(r = (range_t *) mp_realloc(r, sizeof(*r) * (i + 2)))) {
	    mpxp_err<<"can't mp_realloc 'r'"<<std::endl;
	    return NULL;
	}
	tmp_min = strtod(s, const_cast<char**>(&endptr));
	if (*endptr == 'k' || *endptr == 'K') {
	    tmp_min *= 1000.0;
	    endptr++;
	} else if (*endptr == 'm' || *endptr == 'M') {
	    tmp_min *= 1000000.0;
	    endptr++;
	}
	if (*endptr == '-') {
	    tmp_max = strtod(endptr + 1, const_cast<char**>(&endptr));
	    if (*endptr == 'k' || *endptr == 'K') {
		tmp_max *= 1000.0;
		endptr++;
	    } else if (*endptr == 'm' || *endptr == 'M') {
		tmp_max *= 1000000.0;
		endptr++;
	    }
	    if (*endptr != ',' && *endptr)
	    goto out_err;
	} else if (*endptr == ',' || !*endptr) {
	    tmp_max = tmp_min;
	} else goto out_err;
	r[i].min = tmp_min;
	r[i].max = tmp_max;
	if (r[i].min < 0 || r[i].max < 0) goto out_err;
	s = endptr + 1;
    }
    r[i].min = r[i].max = -1;
    return r;
out_err:
    if (r) delete r;
    return NULL;
}

/*
 * Note: this function is completely cut'n'pasted from
 * Chris Lawrence's code.
 * (modified a bit to fit in my code...)
 */
static struct fb_cmap * __FASTCALL__ make_directcolor_cmap(struct fb_var_screeninfo *var)
{
  /* Hopefully any DIRECTCOLOR device will have a big enough palette
   * to handle mapping the full color depth.
   * e.g. 8 bpp -> 256 entry palette
   *
   * We could handle some sort of gamma here
   */
    int i, cols, rcols, gcols, bcols;
    uint16_t *red, *green, *blue;
    struct fb_cmap *cmap;

    rcols = 1 << var->red.length;
    gcols = 1 << var->green.length;
    bcols = 1 << var->blue.length;

    /* Make our palette the length of the deepest color */
    cols = (rcols > gcols ? rcols : gcols);
    cols = (cols > bcols ? cols : bcols);

    red = new uint16_t [cols];
    if(!red) {
	mpxp_err<<"Can't allocate red palette with "<<cols<<" entries"<<std::endl;
	return NULL;
    }
    for(i=0; i< rcols; i++)
	red[i] = (65535/(rcols-1)) * i;

    green = new uint16_t[cols];
    if(!green) {
	mpxp_err<<"Can't allocate green palette with "<<cols<<" entries"<<std::endl;
	delete red;
	return NULL;
    }
    for(i=0; i< gcols; i++)
	green[i] = (65535/(gcols-1)) * i;

    blue = new uint16_t[cols];
    if(!blue) {
	mpxp_err<<"Can't allocate blue palette with "<<cols<<" entries"<<std::endl;
	delete red;
	delete green;
	return NULL;
    }
    for(i=0; i< bcols; i++)
	blue[i] = (65535/(bcols-1)) * i;

    cmap = new struct fb_cmap;
    if(!cmap) {
	mpxp_err<<"Can't allocate color map"<<std::endl;
	delete red;
	delete green;
	delete blue;
	return NULL;
    }
    cmap->start = 0;
    cmap->transp = 0;
    cmap->len = cols;
    cmap->red = red;
    cmap->blue = blue;
    cmap->green = green;
    cmap->transp = NULL;

    return cmap;
}

void FBDev_VO_Interface::lots_of_printf() const
{
    mpxp_v<<FBDEV<< "var info:"<<std::endl;
    mpxp_v<<FBDEV<< "xres: "<<vinfo.xres<<std::endl;
    mpxp_v<<FBDEV<< "yres: "<<vinfo.yres<<std::endl;
    mpxp_v<<FBDEV<< "xres_virtual: "<<vinfo.xres_virtual<<std::endl;
    mpxp_v<<FBDEV<< "yres_virtual: "<<vinfo.yres_virtual<<std::endl;
    mpxp_v<<FBDEV<< "xoffset: "<<vinfo.xoffset<<std::endl;
    mpxp_v<<FBDEV<< "yoffset: "<<vinfo.yoffset<<std::endl;
    mpxp_v<<FBDEV<< "bits_per_pixel: "<<vinfo.bits_per_pixel<<std::endl;
    mpxp_v<<FBDEV<< "grayscale: "<<vinfo.grayscale<<std::endl;
    mpxp_v<<FBDEV<< "red: "<<vinfo.red.offset<<" "<<vinfo.red.length<<" "<<vinfo.red.msb_right<<std::endl;
    mpxp_v<<FBDEV<< "green: "<<vinfo.green.offset<<" "<<vinfo.green.length<<" "<<vinfo.green.msb_right<<std::endl;
    mpxp_v<<FBDEV<< "blue: "<<vinfo.blue.offset<<" "<<vinfo.blue.length<<" "<<vinfo.blue.msb_right<<std::endl;
    mpxp_v<<FBDEV<< "transp: "<<vinfo.transp.offset<<" "<<vinfo.transp.length<<" "<<vinfo.transp.msb_right<<std::endl;
    mpxp_v<<FBDEV<< "nonstd: "<<vinfo.nonstd<<std::endl;
    mpxp_dbg2<<FBDEV<< "activate: "<<vinfo.activate<<std::endl;
    mpxp_dbg2<<FBDEV<< "height: "<<vinfo.height<<std::endl;
    mpxp_dbg2<<FBDEV<< "width: "<<vinfo.width<<std::endl;
    mpxp_dbg2<<FBDEV<< "accel_flags: "<<vinfo.accel_flags<<std::endl;
    mpxp_dbg2<<FBDEV<< "timing:"<<std::endl;
    mpxp_dbg2<<FBDEV<< "pixclock: "<<vinfo.pixclock<<std::endl;
    mpxp_dbg2<<FBDEV<< "left_margin: "<<vinfo.left_margin<<std::endl;
    mpxp_dbg2<<FBDEV<< "right_margin: "<<vinfo.right_margin<<std::endl;
    mpxp_dbg2<<FBDEV<< "upper_margin: "<<vinfo.upper_margin<<std::endl;
    mpxp_dbg2<<FBDEV<< "lower_margin: "<<vinfo.lower_margin<<std::endl;
    mpxp_dbg2<<FBDEV<< "hsync_len: "<<vinfo.hsync_len<<std::endl;
    mpxp_dbg2<<FBDEV<< "vsync_len: "<<vinfo.vsync_len<<std::endl;
    mpxp_dbg2<<FBDEV<< "sync: "<<vinfo.sync<<std::endl;
    mpxp_dbg2<<FBDEV<< "vmode: "<<vinfo.vmode<<std::endl;
    mpxp_v<<FBDEV<< "fix info:"<<std::endl;
    mpxp_v<<FBDEV<< "framebuffer size: "<<finfo.smem_len<<" bytes"<<std::endl;
    mpxp_v<<FBDEV<< "type: "<<finfo.type<<std::endl;
    mpxp_v<<FBDEV<< "type_aux: "<<finfo.type_aux<<std::endl;
    mpxp_v<<FBDEV<< "visual: "<<finfo.visual<<std::endl;
    mpxp_v<<FBDEV<< "line_length: "<<finfo.line_length<<" bytes"<<std::endl;
    mpxp_dbg2<<FBDEV<< "id: "<<finfo.id<<std::endl;
    mpxp_dbg2<<FBDEV<< "smem_start: "<<std::hex<<reinterpret_cast<any_t*>(finfo.smem_start)<<std::endl;
    mpxp_dbg2<<FBDEV<< "xpanstep: "<<finfo.xpanstep<<std::endl;
    mpxp_dbg2<<FBDEV<< "ypanstep: "<<finfo.ypanstep<<std::endl;
    mpxp_dbg2<<FBDEV<< "ywrapstep: "<<finfo.ywrapstep<<std::endl;
    mpxp_dbg2<<FBDEV<< "mmio_start: "<<std::hex<<reinterpret_cast<any_t*>(finfo.mmio_start)<<std::endl;
    mpxp_dbg2<<FBDEV<< "mmio_len: "<<finfo.mmio_len<<" bytes"<<std::endl;
    mpxp_dbg2<<FBDEV<< "accel: "<<finfo.accel<<std::endl;
    mpxp_v<<FBDEV<< "priv.bpp: "<<bpp<<std::endl;
    mpxp_v<<FBDEV<< "priv.real_bpp: "<<real_bpp<<std::endl;
    mpxp_v<<FBDEV<< "priv.pixel_size: "<<pixel_size<<" bytes"<<std::endl;
    mpxp_v<<FBDEV<< "other:"<<std::endl;
    mpxp_v<<FBDEV<< "priv.in_width: "<<in_width<<std::endl;
    mpxp_v<<FBDEV<< "priv.in_height: "<<in_height<<std::endl;
    mpxp_v<<FBDEV<< "priv.out_width: "<<out_width<<std::endl;
    mpxp_v<<FBDEV<< "priv.out_height: "<<out_height<<std::endl;
    mpxp_v<<FBDEV<< "priv.last_row: "<<last_row<<std::endl;
}

void FBDev_VO_Interface::vt_set_textarea(int u, int l)
{
    /* how can I determine the font height?
     * just use 16 for now
     */
    int urow = ((u + 15) / 16) + 1;
    int lrow = l / 16;

    if (mp_conf.verbose > 1)
	mpxp_dbg2<<FBDEV<< "vt_set_textarea("<<u<<","<<l<<"): "<<urow<<","<<lrow<<std::endl;
    vt_fp<<"\33["<<urow<<";"<<lrow<<"r\33["<<lrow<<";0H";
    vt_fp.flush();
}

MPXP_Rc FBDev_VO_Interface::configure(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, unsigned _flags, const std::string& title,
		uint32_t format)
{
    struct fb_cmap *cmap;
    unsigned x_offset,y_offset,i;
    flags=_flags;

    UNUSED(title);
    srcFourcc = format;
    if((int)pre_init_err == MPXP_Error) {
	mpxp_err<<FBDEV<< "Internal fatal error: init() was called before preinit()"<<std::endl;
	return MPXP_False;
    }
    if (pre_init_err!=MPXP_Ok) {
	mpxp_err<<FBDEV<<"fb_prenit() was failed"<<std::endl;
	return MPXP_False;
    }

    if (priv_conf.mode_name && !flags&VOFLAG_MODESWITCHING) {
	mpxp_err<<FBDEV<< "-fbmode can only be used with -vm"<<std::endl;
	return MPXP_False;
    }
    if ((flags&VOFLAG_MODESWITCHING) && (parse_fbmode_cfg(priv_conf.mode_cfgfile) < 0)) return MPXP_False;
    if (d_width && ((flags&VOFLAG_SWSCALE) || (flags&VOFLAG_MODESWITCHING))) {
	out_width = d_width;
	out_height = d_height;
    } else {
	out_width = width;
	out_height = height;
    }
    in_width = width;
    in_height = height;
    pixel_format = format;

    if (priv_conf.mode_name) {
	if (!(mode = find_mode_by_name(priv_conf.mode_name))) {
	    mpxp_err<<FBDEV<< "can't find requested video mode"<<std::endl;
	    return MPXP_False;
	}
	fb_mode2fb_vinfo(mode, &vinfo);
    } else if (flags&VOFLAG_MODESWITCHING) {
	monitor_hfreq = str2range(priv_conf.monitor_hfreq_str);
	monitor_vfreq = str2range(priv_conf.monitor_vfreq_str);
	monitor_dotclock = str2range(priv_conf.monitor_dotclock_str);
	if (!monitor_hfreq || !monitor_vfreq || !monitor_dotclock) {
	    mpxp_err<<FBDEV<< "you have to specify the capabilities of the monitor."<<std::endl;
	    return MPXP_False;
	}
	if (!(mode = find_best_mode(out_width, out_height,
					monitor_hfreq, monitor_vfreq,
					monitor_dotclock))) {
	    mpxp_err<<FBDEV<< "can't find best video mode"<<std::endl;
	    return MPXP_False;
	}
	mpxp_err<<FBDEV<< "using mode "<<mode->xres<<"x"<<mode->yres<<" @ "<<vsf(mode)<<"Hz"<<std::endl;
	fb_mode2fb_vinfo(mode, &vinfo);
    }
    bpp_we_want = bpp;
    set_bpp(&vinfo, bpp);
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres;

    if (tty_fd >= 0 && ::ioctl(tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
	mpxp_dbg2<<FBDEV<< "Can't set graphics mode: "<<strerror(errno)<<std::endl;
	::close(tty_fd);
	tty_fd = -1;
    }

    if (::ioctl(dev_fd, FBIOPUT_VSCREENINFO, &vinfo)) {
	mpxp_err<<FBDEV<< "Can't put VSCREENINFO: "<<strerror(errno)<<std::endl;
	if (tty_fd >= 0 && ::ioctl(tty_fd, KDSETMODE, KD_TEXT) < 0) {
	    mpxp_err<<FBDEV<< "Can't restore text mode: "<<strerror(errno)<<std::endl;
	}
	return MPXP_False;
    }

    pixel_size = (vinfo.bits_per_pixel+7) / 8;
    real_bpp = vinfo.red.length + vinfo.green.length +
    vinfo.blue.length;
    bpp = (pixel_size == 4) ? 32 : real_bpp;
    if (bpp_we_want != bpp)
	mpxp_err<<FBDEV<< "requested "<<bpp_we_want<<" bpp, got "<<bpp<<" bpp!!!"<<std::endl;

    switch (bpp) {
	case 32:
	    dstFourcc = IMGFMT_BGR32;
	    break;
	case 24:
	    dstFourcc = IMGFMT_BGR24;
	    break;
	default:
	case 16:
	    dstFourcc = IMGFMT_BGR16;
	    break;
	case 15:
	    dstFourcc = IMGFMT_BGR15;
	    break;
    }

    if ((flags&VOFLAG_FLIPPING) && ((((pixel_format & 0xff) + 7) / 8) != pixel_size)) {
	mpxp_err<<FBDEV<< "Flipped output with depth conversion is not supported"<<std::endl;
	return MPXP_False;
    }

    xres = vinfo.xres;
    yres = vinfo.yres;
    last_row = (xres-out_height) / 2;

    if (::ioctl(dev_fd, FBIOGET_FSCREENINFO, &finfo)) {
	mpxp_err<<FBDEV<< "Can't get FSCREENINFO: "<<strerror(errno)<<std::endl;
	return MPXP_False;
    }

    lots_of_printf();

    if (finfo.type != FB_TYPE_PACKED_PIXELS) {
	mpxp_err<<FBDEV<< "type "<<finfo.type<<" not supported"<<std::endl;
	return MPXP_False;
    }

    switch (finfo.visual) {
	case FB_VISUAL_TRUECOLOR: break;
	case FB_VISUAL_DIRECTCOLOR:
	    mpxp_dbg2<<FBDEV<< "creating cmap for directcolor"<<std::endl;
	    if (::ioctl(dev_fd, FBIOGETCMAP, &oldcmap)) {
		mpxp_err<<FBDEV<< "can't get cmap: "<<strerror(errno)<<std::endl;
		return MPXP_False;
	    }
	    if (!(cmap = make_directcolor_cmap(&vinfo))) return MPXP_False;
	    if (::ioctl(dev_fd, FBIOPUTCMAP, cmap)) {
		mpxp_err<<FBDEV<< "can't put cmap: "<<strerror(errno)<<std::endl;
		return MPXP_False;
	    }
	    cmap_changed = 1;
	    delete cmap->red;
	    delete cmap->green;
	    delete cmap->blue;
	    delete cmap;
	    break;
	default:
	    mpxp_err<<FBDEV<< "visual: "<<finfo.visual<<" not yet supported"<<std::endl;
	    return MPXP_False;
    }

    line_len = finfo.line_length;
    size = finfo.smem_len;
    frame_buffer = NULL;
    memset(next_frame,0,sizeof(next_frame));
    out_width=width;
    out_height=height;
    if(flags&VOFLAG_SWSCALE) {
	aspect->save(width,height,d_width,d_height,xres,yres);
	aspect->calc(out_width,out_height,flags&VOFLAG_FULLSCREEN?Aspect::ZOOM:Aspect::NOZOOM);
    } else if(flags&VOFLAG_FULLSCREEN) {
	out_width = xres;
	out_height = yres;
    }
    if(xres > out_width)
	x_offset = (xres - out_width) / 2;
    else x_offset = 0;
    if(yres > out_height)
	y_offset = (yres - out_height) / 2;
    else y_offset = 0;

#ifdef CONFIG_VIDIX
    if(vidix) {
	if(vidix->configure(width,height,x_offset,y_offset,out_width,
		    out_height,format,bpp,
		    xres,yres) != MPXP_Ok) {
			mpxp_err<<FBDEV<< "Can't initialize VIDIX driver"<<std::endl;
			return MPXP_False;
	} else mpxp_v<<FBDEV<< "Using VIDIX"<<std::endl;
	if ((frame_buffer = (uint8_t *) ::mmap(0, size, PROT_READ | PROT_WRITE,
						     MAP_SHARED, dev_fd, 0)) == (uint8_t *) -1) {
	    mpxp_err<<FBDEV<< "Can't mmap "<<priv_conf.dev_name<<": "<<strerror(errno)<<std::endl;
	    return MPXP_False;
	}
	memset(frame_buffer, 0, line_len * yres);
	if(vidix->start()!=0) return MPXP_False;
    } else
#endif
    {
	if ((frame_buffer = (uint8_t *) ::mmap(0, size, PROT_READ | PROT_WRITE,
				    MAP_SHARED, dev_fd, 0)) == (uint8_t *) -1) {
	    mpxp_err<<FBDEV<< "Can't mmap "<<priv_conf.dev_name<<": "<<strerror(errno)<<std::endl;
	    return MPXP_False;
	}
	if(out_width > xres) out_width=xres;
	if(out_height > yres) out_width=yres;
	L123123875 = frame_buffer + x_offset * pixel_size + y_offset * line_len;
	mpxp_dbg2<<FBDEV<< "frame_buffer @ "<<std::hex<<reinterpret_cast<long>(frame_buffer)<<std::endl;
	mpxp_dbg2<<FBDEV<< "L123123875 @ "<<std::hex<<reinterpret_cast<long>(L123123875)<<std::endl;
	mpxp_v<<FBDEV<< "pixel per line: "<<(line_len/pixel_size)<<std::endl;

	total_fr=vo_conf.xp_buffs;
	for(i=0;i<total_fr;i++)
	    if (!(next_frame[i] = (uint8_t *) mp_malloc(out_width * out_height * pixel_size))) {
		mpxp_err<<FBDEV<< "Can't mp_malloc next_frame: "<<strerror(errno)<<std::endl;
		return MPXP_False;
	    }
    }
    vt_fp.open("/dev/tty", std::ios_base::out);
    if (vt_doit && !vt_fp.is_open()) {
	mpxp_err<<FBDEV<< "can't fdopen /dev/tty: "<<strerror(errno)<<std::endl;
	vt_doit = 0;
    }

    if (vt_doit)
	vt_set_textarea(last_row, yres);

    return MPXP_Ok;
}

MPXP_Rc FBDev_VO_Interface::query_format(vo_query_fourcc_t * format) const
{
    MPXP_Rc rc=MPXP_False;
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->query_fourcc(format);
#endif
    format->flags=VOCAP_NA;
    switch(format->fourcc) {
	case IMGFMT_BGR15: if(bpp == 15) format->flags=VOCAP_SUPPORTED; rc=MPXP_Ok; break;
	case IMGFMT_BGR16: if(bpp == 16) format->flags=VOCAP_SUPPORTED; rc=MPXP_Ok; break;
	case IMGFMT_BGR24: if(bpp == 24) format->flags=VOCAP_SUPPORTED; rc=MPXP_Ok; break;
	case IMGFMT_BGR32: if(bpp == 32) format->flags=VOCAP_SUPPORTED; rc=MPXP_Ok; break;
	default: break;
    }
    return rc;
}

MPXP_Rc FBDev_VO_Interface::select_frame(unsigned idx)
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->select_frame(idx);
#endif

    size_t src_stride=out_width*pixel_size;
    size_t dst_stride=line_len;
    stream_copy_pic(L123123875,next_frame[idx],src_stride,out_height,dst_stride,src_stride);

    return MPXP_Ok;
}

void FBDev_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface_caps(caps);
#endif
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = dstFourcc;
    caps->width=out_width;
    caps->height=out_height;
    caps->x=0;
    caps->y=0;
    caps->w=out_width;
    caps->h=out_height;
    caps->strides[0] = (out_width)*((bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

void FBDev_VO_Interface::get_surface(dri_surface_t *surf)
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface(surf);
#endif
    surf->planes[0] = next_frame[surf->idx];
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

unsigned FBDev_VO_Interface::get_num_frames() const {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_num_frames();
#endif
    return total_fr;
}

MPXP_Rc FBDev_VO_Interface::flush_page(unsigned idx) {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->flush_page(idx);
#endif
    return MPXP_False;
}

uint32_t FBDev_VO_Interface::check_events(const vo_resize_t* vr) {
    UNUSED(vr);
    return 0;
}

MPXP_Rc FBDev_VO_Interface::ctrl(uint32_t request, any_t*data) {
#ifdef CONFIG_VIDIX
    switch (request) {
	case VOCTRL_SET_EQUALIZER:
	    if(!vidix->set_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    return MPXP_False;
	case VOCTRL_GET_EQUALIZER:
	    if(vidix->get_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    return MPXP_False;
    }
#endif
    return MPXP_NA;
}

static VO_Interface* query_interface(const std::string& args) { return new(zeromem) FBDev_VO_Interface(args); }
extern const vo_info_t fbdev_vo_info = {
    "Framebuffer Device"
#ifdef CONFIG_VIDIX
    " (with fbdev:vidix subdevice)"
#endif
    ,
    "fbdev",
    "Szabolcs Berecz <szabi@inf.elte.hu>",
    "",
    query_interface
};
} //namespace
