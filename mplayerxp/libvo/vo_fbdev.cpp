/*
 * Video driver for Framebuffer device
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * Some idea and code borrowed from Chris Lawrence's ppmtofb-0.27
 */

#define FBDEV "fbdev: "

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

#include "mp_config.h"
#include "mplayerxp.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "sub.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "aspect.h"
#include "osd.h"
#include "dri_vo.h"
#include "libmpstream/mrl.h"
#include "vo_msg.h"

LIBVO_EXTERN(fbdev)

static vo_info_t vo_info = {
    "Framebuffer Device"
#ifdef CONFIG_VIDIX
    " (with fbdev:vidix subdevice)"
#endif
    ,
    "fbdev",
    "Szabolcs Berecz <szabi@inf.elte.hu>",
    ""
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

#define PRINT_LINENUM //MSG_DBG2(" at line %d\n", line_num)

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

static int __FASTCALL__ validate_mode(fb_mode_t *m)
{
    if (!m->xres) {
	MSG_V("needs geometry ");
	return 0;
    }
    if (!m->pixclock) {
	MSG_V("needs timings ");
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

typedef struct priv_s {
    FILE *		fp;
    int			line_num;
    char *		line;
    char *		token[MAX_NR_TOKEN];
    uint32_t		srcFourcc,dstFourcc;
/* fb.modes related variables */
    range_t *		monitor_hfreq;
    range_t *		monitor_vfreq;
    range_t *		monitor_dotclock;
    fb_mode_t *		mode;
/* vt related variables */
    int			vt_fd;
    FILE *		vt_fp;
    int			vt_doit;
/* vo_fbdev related variables */
    int			dev_fd;
    int			tty_fd;
    size_t		size;
    uint8_t *		frame_buffer;
    uint8_t *		L123123875;	/* thx .so :) */
    struct fb_fix_screeninfo	finfo;
    struct fb_var_screeninfo	orig_vinfo;
    struct fb_var_screeninfo	vinfo;
    struct fb_cmap		oldcmap;
    int			cmap_changed;
    unsigned		pixel_size;	// 32:  4  24:  3  16:  2  15:  2
    uint32_t		pixel_format;
    unsigned		real_bpp;	// 32: 24  24: 24  16: 16  15: 15
    unsigned		bpp;		// 32: 32  24: 24  16: 16  15: 15
    unsigned		bpp_we_want;	// 32: 32  24: 24  16: 16  15: 15
    unsigned		line_len;
    unsigned		xres;
    unsigned		yres;

    uint8_t *		next_frame[MAX_DRI_BUFFERS];
    unsigned		total_fr;
    int			in_width;
    int			in_height;
    unsigned		out_width;
    unsigned		out_height;
    int			last_row;
    int			fs;
    MPXP_Rc		pre_init_err;
#ifdef CONFIG_VIDIX
/* Name of VIDIX driver */
    const char*		vidix_name;
    vidix_server_t*	vidix_server;
#endif
}priv_t;

typedef struct priv_conf_s {
/* command line/config file options */
    char*		dev_name;
    const char*		mode_cfgfile;
    char*		mode_name;
    const char*		monitor_hfreq_str;
    const char*		monitor_vfreq_str;
    const char*		monitor_dotclock_str;
}priv_conf_t;
static priv_conf_t priv_conf;

static int __FASTCALL__ get_token(vo_data_t*vo,int num)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    static int read_nextline = 1;
    static int line_pos;
    int i;
    char c;

    if (num >= MAX_NR_TOKEN) {
	MSG_ERR("get_token(): max >= MAX_NR_TOKEN!");
	goto out_eof;
    }

    if (read_nextline) {
	if (!fgets(priv->line, MAX_LINE_LEN, priv->fp)) goto out_eof;
	line_pos = 0;
	++priv->line_num;
	read_nextline = 0;
    }
    for (i = 0; i < num; i++) {
	while (isspace(priv->line[line_pos])) ++line_pos;
	if (priv->line[line_pos] == '\0' || priv->line[line_pos] == '#') {
	    read_nextline = 1;
	    goto out_eol;
	}
	priv->token[i] = priv->line + line_pos;
	c = priv->line[line_pos];
	if (c == '"' || c == '\'') {
	    priv->token[i]++;
	    while (priv->line[++line_pos] != c && priv->line[line_pos]) /* NOTHING */;
	    if (!priv->line[line_pos]) goto out_eol;
	    priv->line[line_pos] = ' ';
	} else {
	    for (/* NOTHING */; !isspace(priv->line[line_pos]) && priv->line[line_pos]; line_pos++) /* NOTHING */;
	}
	if (!priv->line[line_pos]) {
	    read_nextline = 1;
	    if (i == num - 1) goto out_ok;
	    goto out_eol;
	}
	priv->line[line_pos++] = '\0';
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

static int __FASTCALL__ parse_fbmode_cfg(vo_data_t*vo,const char *cfgfile)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
#define CHECK_IN_MODE_DEF\
	do {\
	if (!in_mode_def) {\
		MSG_DBG2("'needs 'mode' first");\
		goto err_out_print_linenum;\
	}\
	} while (0)

    fb_mode_t *mode = NULL;
    char *endptr;	// strtoul()...
    int in_mode_def = 0;
    int tmp, i;

    MSG_DBG2("Reading %s: ", cfgfile);

    if ((priv->fp = fopen(cfgfile, "r")) == NULL) {
	MSG_ERR("can't open '%s': %s\n", cfgfile, strerror(errno));
	return -1;
    }

    if ((priv->line = (char *) mp_malloc(MAX_LINE_LEN + 1)) == NULL) {
	MSG_ERR("can't get memory for 'priv->line': %s\n", strerror(errno));
	return -2;
    }

    /*
     * check if the cfgfile starts with 'mode'
     */
    while ((tmp = get_token(vo,1)) == RET_EOL) /* NOTHING */;
    if (tmp == RET_EOF) goto out;
    if (!strcmp(priv->token[0], "mode")) goto loop_enter;
    goto err_out_parse_error;

    while ((tmp = get_token(vo,1)) != RET_EOF) {
	if (tmp == RET_EOL) continue;
	if (!strcmp(priv->token[0], "mode")) {
	    if (in_mode_def) {
		MSG_ERR("'endmode' required");
		goto err_out_print_linenum;
	    }
	    if (!validate_mode(mode)) goto err_out_not_valid;
	    loop_enter:
	    if (!(fb_modes = (fb_mode_t *) mp_realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
		MSG_ERR("can't mp_realloc 'fb_modes' (nr_modes = %d):"
			" %s\n", nr_modes, strerror(errno));
		goto err_out;
	    }
	    mode=fb_modes + nr_modes;
	    ++nr_modes;
	    memset(mode,0,sizeof(fb_mode_t));

	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    for (i = 0; i < nr_modes - 1; i++) {
		if (!strcmp(priv->token[0], fb_modes[i].name)) {
		    MSG_ERR("mode name '%s' isn't unique", priv->token[0]);
		    goto err_out_print_linenum;
		}
	    }
	    if (!(mode->name = mp_strdup(priv->token[0]))) {
		MSG_ERR("can't mp_strdup -> 'name': %s\n", strerror(errno));
		goto err_out;
	    }
	    in_mode_def = 1;
	} else if (!strcmp(priv->token[0], "geometry")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,5) < 0) goto err_out_parse_error;
	    mode->xres = strtoul(priv->token[0], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->yres = strtoul(priv->token[1], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->vxres = strtoul(priv->token[2], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->vyres = strtoul(priv->token[3], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->depth = strtoul(priv->token[4], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "timings")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,7) < 0) goto err_out_parse_error;
	    mode->pixclock = strtoul(priv->token[0], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->left = strtoul(priv->token[1], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->right = strtoul(priv->token[2], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->upper = strtoul(priv->token[3], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->lower = strtoul(priv->token[4], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->hslen = strtoul(priv->token[5], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	    mode->vslen = strtoul(priv->token[6], &endptr, 0);
	    if (*endptr) goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "endmode")) {
	    CHECK_IN_MODE_DEF;
	    in_mode_def = 0;
	} else if (!strcmp(priv->token[0], "accel")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    /*
	     * it's only used for text acceleration
	     * so we just ignore it.
	     */
	} else if (!strcmp(priv->token[0], "hsync")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "low")) mode->sync &= ~FB_SYNC_HOR_HIGH_ACT;
	    else if(!strcmp(priv->token[0], "high")) mode->sync |= FB_SYNC_HOR_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "vsync")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "low")) mode->sync &= ~FB_SYNC_VERT_HIGH_ACT;
	    else if(!strcmp(priv->token[0], "high")) mode->sync |= FB_SYNC_VERT_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "csync")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "low")) mode->sync &= ~FB_SYNC_COMP_HIGH_ACT;
	    else if(!strcmp(priv->token[0], "high")) mode->sync |= FB_SYNC_COMP_HIGH_ACT;
	    else goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "extsync")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "false")) mode->sync &= ~FB_SYNC_EXT;
	    else if(!strcmp(priv->token[0], "true")) mode->sync |= FB_SYNC_EXT;
	    else goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "laced")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "false")) mode->vmode = FB_VMODE_NONINTERLACED;
	    else if (!strcmp(priv->token[0], "true")) mode->vmode = FB_VMODE_INTERLACED;
	    else goto err_out_parse_error;
	} else if (!strcmp(priv->token[0], "double")) {
	    CHECK_IN_MODE_DEF;
	    if (get_token(vo,1) < 0) goto err_out_parse_error;
	    if (!strcmp(priv->token[0], "false")) ;
	    else if (!strcmp(priv->token[0], "true")) mode->vmode = FB_VMODE_DOUBLE;
	    else goto err_out_parse_error;
	} else goto err_out_parse_error;
    }
    if (!validate_mode(mode)) goto err_out_not_valid;
out:
    MSG_DBG2("%d modes\n", nr_modes);
    delete priv->line;
    fclose(priv->fp);
    return nr_modes;
err_out_parse_error:
    MSG_ERR("parse error");
err_out_print_linenum:
    PRINT_LINENUM;
err_out:
    if (fb_modes) {
	delete fb_modes;
	fb_modes = NULL;
    }
    nr_modes = 0;
    delete priv->line;
    delete priv->fp;
    return -2;
err_out_not_valid:
    MSG_ERR("previous mode is not correct");
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

    MSG_DBG2(FBDEV "mode %dx%d:", m->xres, m->yres);
    if (!in_range(hfreq, h)) {
	ret = 0;
	MSG_DBG2(" hsync out of range.");
    }
    if (!in_range(vfreq, v)) {
	ret = 0;
	MSG_DBG2(" vsync out of range.");
    }
    if (!in_range(dotclock, d)) {
	ret = 0;
	MSG_DBG2(" dotclock out of range.");
    }
    if (ret)	MSG_DBG2(" hsync, vsync, dotclock ok.\n");
    else	MSG_DBG2("\n");
    return ret;
}

static fb_mode_t * __FASTCALL__ find_best_mode(unsigned xres, unsigned yres, range_t *hfreq,
		range_t *vfreq, range_t *dotclock)
{
    int i;
    fb_mode_t *best = fb_modes;
    fb_mode_t *curr;

    MSG_DBG2(FBDEV "Searching for first working mode\n");

    for (i = 0; i < nr_modes; i++, best++)
	if (mode_works(best, hfreq, vfreq, dotclock))
	    break;

    if (i == nr_modes)
	return NULL;
    if (i == nr_modes - 1)
	return best;

    MSG_DBG2(FBDEV "First working mode: %dx%d\n", best->xres, best->yres);
    MSG_DBG2(FBDEV "Searching for better modes\n");

    for (curr = best + 1; i < nr_modes - 1; i++, curr++) {
	if (!mode_works(curr, hfreq, vfreq, dotclock))
	continue;

	MSG_DBG2(FBDEV);

	if (best->xres < xres || best->yres < yres) {
	    if (curr->xres > best->xres || curr->yres > best->yres) {
		MSG_DBG2("better than %dx%d, which is too small.\n",
			best->xres, best->yres);
		best = curr;
	    } else MSG_DBG2("too small.\n");
	} else if (curr->xres == best->xres && curr->yres == best->yres &&
			vsf(curr) > vsf(best)) {
	    MSG_DBG2("faster screen refresh.\n");
	    best = curr;
	} else if ((curr->xres <= best->xres && curr->yres <= best->yres) &&
				(curr->xres >= xres && curr->yres >= yres)) {
	    MSG_DBG2("better than %dx%d, which is too large.\n",
			best->xres, best->yres);
	    best = curr;
	} else {
	    if (curr->xres < xres || curr->yres < yres) MSG_DBG2("too small.\n");
	    else if (curr->xres > best->xres || curr->yres > best->yres) MSG_DBG2("too large.\n");
	    else MSG_DBG2("it's worse, don't know why.\n");
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
	    MSG_ERR("can't mp_realloc 'r'\n");
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
	MSG_ERR("Can't allocate red palette with %d entries.\n", cols);
	return NULL;
    }
    for(i=0; i< rcols; i++)
	red[i] = (65535/(rcols-1)) * i;

    green = new uint16_t[cols];
    if(!green) {
	MSG_ERR("Can't allocate green palette with %d entries.\n", cols);
	delete red;
	return NULL;
    }
    for(i=0; i< gcols; i++)
	green[i] = (65535/(gcols-1)) * i;

    blue = new uint16_t[cols];
    if(!blue) {
	MSG_ERR("Can't allocate blue palette with %d entries.\n", cols);
	delete red;
	delete green;
	return NULL;
    }
    for(i=0; i< bcols; i++)
	blue[i] = (65535/(bcols-1)) * i;

    cmap = new struct fb_cmap;
    if(!cmap) {
	MSG_ERR("Can't allocate color map\n");
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

static const mrl_config_t fbconf[]=
{
    { "modeconfig", &priv_conf.mode_cfgfile, MRL_TYPE_STRING, 0, 0 },
    { "hfreq", &priv_conf.monitor_hfreq_str, MRL_TYPE_STRING, 0, 0 },
    { "vfreq", &priv_conf.monitor_vfreq_str, MRL_TYPE_STRING, 0, 0 },
    { "dotclock", &priv_conf.monitor_dotclock_str, MRL_TYPE_STRING, 0, 0 },
    { NULL, NULL, 0, 0, 0 },
};

static uint32_t __FASTCALL__ parseSubDevice(priv_t* priv,const char *sd)
{
    const char *param;
#ifdef CONFIG_VIDIX
    if(memcmp(sd,"vidix",5) == 0) priv->vidix_name = &sd[5]; /* vidix_name will be valid within init() */
    else
#endif
    {
	param=mrl_parse_line(sd,NULL,NULL,&priv_conf.dev_name,&priv_conf.mode_name);
	mrl_parse_params(param,fbconf);
    }
    return 0;
}

static MPXP_Rc fb_preinit(vo_data_t*vo)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    static int fb_preinit_done = 0;
    static MPXP_Rc fb_works = MPXP_Ok;

    if (fb_preinit_done) return fb_works;

    if (!priv_conf.dev_name && !(priv_conf.dev_name = getenv("FRAMEBUFFER")))
    priv_conf.dev_name = "/dev/fb0";
    MSG_DBG2(FBDEV "using %s\n", priv_conf.dev_name);

    if ((priv->dev_fd = open(priv_conf.dev_name, O_RDWR)) == -1) {
	MSG_ERR(FBDEV "Can't open %s: %s\n", priv_conf.dev_name, strerror(errno));
	goto err_out;
    }
    if (ioctl(priv->dev_fd, FBIOGET_VSCREENINFO, &priv->vinfo)) {
	MSG_ERR(FBDEV "Can't get VSCREENINFO: %s\n", strerror(errno));
	goto err_out_fd;
    }
    priv->orig_vinfo = priv->vinfo;

    if ((priv->tty_fd = open("/dev/tty", O_RDWR)) < 0) {
	MSG_DBG2(FBDEV "notice: Can't open /dev/tty: %s\n", strerror(errno));
    }

    priv->bpp = priv->vinfo.bits_per_pixel;

    if (priv->bpp == 8 && !vo_conf.dbpp) {
	MSG_ERR(FBDEV "8 bpp output is not supported.\n");
	goto err_out_tty_fd;
    }

    /* 16 and 15 bpp is reported as 16 bpp */
    if (priv->bpp == 16)
	priv->bpp = priv->vinfo.red.length + priv->vinfo.green.length + priv->vinfo.blue.length;

    if (vo_conf.dbpp) {
	if (vo_conf.dbpp != 15 && vo_conf.dbpp != 16 && vo_conf.dbpp != 24 && vo_conf.dbpp != 32) {
		MSG_ERR(FBDEV "can't switch to %d bpp\n", vo_conf.dbpp);
		goto err_out_fd;
	}
	priv->bpp = vo_conf.dbpp;
    }

    fb_preinit_done = 1;
    fb_works = MPXP_Ok;
    return MPXP_Ok;
err_out_tty_fd:
    close(priv->tty_fd);
    priv->tty_fd = -1;
err_out_fd:
    close(priv->dev_fd);
    priv->dev_fd = -1;
err_out:
    fb_preinit_done = 1;
    fb_works = MPXP_False;
    return MPXP_False;
}

static void lots_of_printf(vo_data_t*vo)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    MSG_V(FBDEV "var info:\n");
    MSG_V(FBDEV "xres: %u\n", priv->vinfo.xres);
    MSG_V(FBDEV "yres: %u\n", priv->vinfo.yres);
    MSG_V(FBDEV "xres_virtual: %u\n", priv->vinfo.xres_virtual);
    MSG_V(FBDEV "yres_virtual: %u\n", priv->vinfo.yres_virtual);
    MSG_V(FBDEV "xoffset: %u\n", priv->vinfo.xoffset);
    MSG_V(FBDEV "yoffset: %u\n", priv->vinfo.yoffset);
    MSG_V(FBDEV "bits_per_pixel: %u\n", priv->vinfo.bits_per_pixel);
    MSG_V(FBDEV "grayscale: %u\n", priv->vinfo.grayscale);
    MSG_V(FBDEV "red: %lu %lu %lu\n",
		(unsigned long) priv->vinfo.red.offset,
		(unsigned long) priv->vinfo.red.length,
		(unsigned long) priv->vinfo.red.msb_right);
    MSG_V(FBDEV "green: %lu %lu %lu\n",
		(unsigned long) priv->vinfo.green.offset,
		(unsigned long) priv->vinfo.green.length,
		(unsigned long) priv->vinfo.green.msb_right);
    MSG_V(FBDEV "blue: %lu %lu %lu\n",
		(unsigned long) priv->vinfo.blue.offset,
		(unsigned long) priv->vinfo.blue.length,
		(unsigned long) priv->vinfo.blue.msb_right);
    MSG_V(FBDEV "transp: %lu %lu %lu\n",
		(unsigned long) priv->vinfo.transp.offset,
		(unsigned long) priv->vinfo.transp.length,
		(unsigned long) priv->vinfo.transp.msb_right);
    MSG_V(FBDEV "nonstd: %u\n", priv->vinfo.nonstd);
    MSG_DBG2(FBDEV "activate: %u\n", priv->vinfo.activate);
    MSG_DBG2(FBDEV "height: %u\n", priv->vinfo.height);
    MSG_DBG2(FBDEV "width: %u\n", priv->vinfo.width);
    MSG_DBG2(FBDEV "accel_flags: %u\n", priv->vinfo.accel_flags);
    MSG_DBG2(FBDEV "timing:\n");
    MSG_DBG2(FBDEV "pixclock: %u\n", priv->vinfo.pixclock);
    MSG_DBG2(FBDEV "left_margin: %u\n", priv->vinfo.left_margin);
    MSG_DBG2(FBDEV "right_margin: %u\n", priv->vinfo.right_margin);
    MSG_DBG2(FBDEV "upper_margin: %u\n", priv->vinfo.upper_margin);
    MSG_DBG2(FBDEV "lower_margin: %u\n", priv->vinfo.lower_margin);
    MSG_DBG2(FBDEV "hsync_len: %u\n", priv->vinfo.hsync_len);
    MSG_DBG2(FBDEV "vsync_len: %u\n", priv->vinfo.vsync_len);
    MSG_DBG2(FBDEV "sync: %u\n", priv->vinfo.sync);
    MSG_DBG2(FBDEV "vmode: %u\n", priv->vinfo.vmode);
    MSG_V(FBDEV "fix info:\n");
    MSG_V(FBDEV "framebuffer size: %d bytes\n", priv->finfo.smem_len);
    MSG_V(FBDEV "type: %lu\n", (unsigned long) priv->finfo.type);
    MSG_V(FBDEV "type_aux: %lu\n", (unsigned long) priv->finfo.type_aux);
    MSG_V(FBDEV "visual: %lu\n", (unsigned long) priv->finfo.visual);
    MSG_V(FBDEV "line_length: %lu bytes\n", (unsigned long) priv->finfo.line_length);
    MSG_DBG2(FBDEV "id: %.16s\n", priv->finfo.id);
    MSG_DBG2(FBDEV "smem_start: %p\n", (any_t*) priv->finfo.smem_start);
    MSG_DBG2(FBDEV "xpanstep: %u\n", priv->finfo.xpanstep);
    MSG_DBG2(FBDEV "ypanstep: %u\n", priv->finfo.ypanstep);
    MSG_DBG2(FBDEV "ywrapstep: %u\n", priv->finfo.ywrapstep);
    MSG_DBG2(FBDEV "mmio_start: %p\n", (any_t*) priv->finfo.mmio_start);
    MSG_DBG2(FBDEV "mmio_len: %u bytes\n", priv->finfo.mmio_len);
    MSG_DBG2(FBDEV "accel: %u\n", priv->finfo.accel);
    MSG_V(FBDEV "priv->bpp: %d\n", priv->bpp);
    MSG_V(FBDEV "priv->real_bpp: %d\n", priv->real_bpp);
    MSG_V(FBDEV "priv->pixel_size: %d bytes\n", priv->pixel_size);
    MSG_V(FBDEV "other:\n");
    MSG_V(FBDEV "priv->in_width: %d\n", priv->in_width);
    MSG_V(FBDEV "priv->in_height: %d\n", priv->in_height);
    MSG_V(FBDEV "priv->out_width: %d\n", priv->out_width);
    MSG_V(FBDEV "priv->out_height: %d\n", priv->out_height);
    MSG_V(FBDEV "priv->last_row: %d\n", priv->last_row);
}

static void __FASTCALL__ vt_set_textarea(vo_data_t*vo,int u, int l)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    /* how can I determine the font height?
     * just use 16 for now
     */
    int urow = ((u + 15) / 16) + 1;
    int lrow = l / 16;

    if (mp_conf.verbose > 1)
	MSG_DBG2(FBDEV "vt_set_textarea(%d,%d): %d,%d\n", u, l, urow, lrow);
    fprintf(priv->vt_fp, "\33[%d;%dr\33[%d;%dH", urow, lrow, lrow, 0);
    fflush(priv->vt_fp);
}

static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    struct fb_cmap *cmap;
    unsigned x_offset,y_offset,i;

    UNUSED(title);
    UNUSED(fullscreen);
    priv->srcFourcc = format;
    if((int)priv->pre_init_err == MPXP_Error) {
	MSG_ERR(FBDEV "Internal fatal error: init() was called before preinit()\n");
	return MPXP_False;
    }
    if (priv->pre_init_err!=MPXP_Ok) return MPXP_False;

    if (priv_conf.mode_name && !vo_VM(vo)) {
	MSG_ERR(FBDEV "-fbmode can only be used with -vm\n");
	return MPXP_False;
    }
    if (vo_VM(vo) && (parse_fbmode_cfg(vo,priv_conf.mode_cfgfile) < 0)) return MPXP_False;
    if (d_width && (vo_ZOOM(vo) || vo_VM(vo))) {
	priv->out_width = d_width;
	priv->out_height = d_height;
    } else {
	priv->out_width = width;
	priv->out_height = height;
    }
    priv->in_width = width;
    priv->in_height = height;
    priv->pixel_format = format;

    if (priv_conf.mode_name) {
	if (!(priv->mode = find_mode_by_name(priv_conf.mode_name))) {
	    MSG_ERR(FBDEV "can't find requested video mode\n");
	    return MPXP_False;
	}
	fb_mode2fb_vinfo(priv->mode, &priv->vinfo);
    } else if (vo_VM(vo)) {
	priv->monitor_hfreq = str2range(priv_conf.monitor_hfreq_str);
	priv->monitor_vfreq = str2range(priv_conf.monitor_vfreq_str);
	priv->monitor_dotclock = str2range(priv_conf.monitor_dotclock_str);
	if (!priv->monitor_hfreq || !priv->monitor_vfreq || !priv->monitor_dotclock) {
	    MSG_ERR(FBDEV "you have to specify the capabilities of"
			" the monitor.\n");
	    return MPXP_False;
	}
	if (!(priv->mode = find_best_mode(priv->out_width, priv->out_height,
					priv->monitor_hfreq, priv->monitor_vfreq,
					priv->monitor_dotclock))) {
	    MSG_ERR(FBDEV "can't find best video mode\n");
	    return MPXP_False;
	}
	MSG_ERR(FBDEV "using mode %dx%d @ %.1fHz\n", priv->mode->xres,
		priv->mode->yres, vsf(priv->mode));
	fb_mode2fb_vinfo(priv->mode, &priv->vinfo);
    }
    priv->bpp_we_want = priv->bpp;
    set_bpp(&priv->vinfo, priv->bpp);
    priv->vinfo.xres_virtual = priv->vinfo.xres;
    priv->vinfo.yres_virtual = priv->vinfo.yres;

    if (priv->tty_fd >= 0 && ioctl(priv->tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
	MSG_DBG2(FBDEV "Can't set graphics mode: %s\n", strerror(errno));
	close(priv->tty_fd);
	priv->tty_fd = -1;
    }

    if (ioctl(priv->dev_fd, FBIOPUT_VSCREENINFO, &priv->vinfo)) {
	MSG_ERR(FBDEV "Can't put VSCREENINFO: %s\n", strerror(errno));
	if (priv->tty_fd >= 0 && ioctl(priv->tty_fd, KDSETMODE, KD_TEXT) < 0) {
	    MSG_ERR(FBDEV "Can't restore text mode: %s\n", strerror(errno));
	}
	return MPXP_False;
    }

    priv->pixel_size = (priv->vinfo.bits_per_pixel+7) / 8;
    priv->real_bpp = priv->vinfo.red.length + priv->vinfo.green.length +
    priv->vinfo.blue.length;
    priv->bpp = (priv->pixel_size == 4) ? 32 : priv->real_bpp;
    if (priv->bpp_we_want != priv->bpp)
	MSG_ERR(FBDEV "requested %d bpp, got %d bpp!!!\n",
		priv->bpp_we_want, priv->bpp);

    switch (priv->bpp) {
	case 32:
	    priv->dstFourcc = IMGFMT_BGR32;
	    break;
	case 24:
	    priv->dstFourcc = IMGFMT_BGR24;
	    break;
	default:
	case 16:
	    priv->dstFourcc = IMGFMT_BGR16;
	    break;
	case 15:
	    priv->dstFourcc = IMGFMT_BGR15;
	    break;
    }

    if (vo_FLIP(vo) && ((((priv->pixel_format & 0xff) + 7) / 8) != priv->pixel_size)) {
	MSG_ERR(FBDEV "Flipped output with depth conversion is not "
			"supported\n");
	return MPXP_False;
    }

    priv->xres = priv->vinfo.xres;
    priv->yres = priv->vinfo.yres;
    priv->last_row = (priv->xres-priv->out_height) / 2;

    if (ioctl(priv->dev_fd, FBIOGET_FSCREENINFO, &priv->finfo)) {
	MSG_ERR(FBDEV "Can't get FSCREENINFO: %s\n", strerror(errno));
	return MPXP_False;
    }

    lots_of_printf(vo);

    if (priv->finfo.type != FB_TYPE_PACKED_PIXELS) {
	MSG_ERR(FBDEV "type %d not supported\n", priv->finfo.type);
	return MPXP_False;
    }

    switch (priv->finfo.visual) {
	case FB_VISUAL_TRUECOLOR: break;
	case FB_VISUAL_DIRECTCOLOR:
	    MSG_DBG2(FBDEV "creating cmap for directcolor\n");
	    if (ioctl(priv->dev_fd, FBIOGETCMAP, &priv->oldcmap)) {
		MSG_ERR(FBDEV "can't get cmap: %s\n",strerror(errno));
		return MPXP_False;
	    }
	    if (!(cmap = make_directcolor_cmap(&priv->vinfo))) return MPXP_False;
	    if (ioctl(priv->dev_fd, FBIOPUTCMAP, cmap)) {
		MSG_ERR(FBDEV "can't put cmap: %s\n",strerror(errno));
		return MPXP_False;
	    }
	    priv->cmap_changed = 1;
	    delete cmap->red;
	    delete cmap->green;
	    delete cmap->blue;
	    delete cmap;
	    break;
	default:
	    MSG_ERR(FBDEV "visual: %d not yet supported\n",priv->finfo.visual);
	    return MPXP_False;
    }

    priv->line_len = priv->finfo.line_length;
    priv->size = priv->finfo.smem_len;
    priv->frame_buffer = NULL;
    memset(priv->next_frame,0,sizeof(priv->next_frame));
    priv->out_width=width;
    priv->out_height=height;
    if(vo_ZOOM(vo)) {
	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(priv->xres,priv->yres);
	aspect(&priv->out_width,&priv->out_height,A_ZOOM);
    } else if(vo_FS(vo)) {
	priv->out_width = priv->xres;
	priv->out_height = priv->yres;
    }
    if(priv->xres > priv->out_width)
	x_offset = (priv->xres - priv->out_width) / 2;
    else x_offset = 0;
    if(priv->yres > priv->out_height)
	y_offset = (priv->yres - priv->out_height) / 2;
    else y_offset = 0;

#ifdef CONFIG_VIDIX
    if(priv->vidix_name) {
	if(vidix_init(vo,width,height,x_offset,y_offset,priv->out_width,
		    priv->out_height,format,priv->bpp,
		    priv->xres,priv->yres) != MPXP_Ok) {
			MSG_ERR(FBDEV "Can't initialize VIDIX driver\n");
			priv->vidix_name = NULL;
			vidix_term(vo);
			return MPXP_False;
	} else MSG_V(FBDEV "Using VIDIX\n");
	if ((priv->frame_buffer = (uint8_t *) mmap(0, priv->size, PROT_READ | PROT_WRITE,
						     MAP_SHARED, priv->dev_fd, 0)) == (uint8_t *) -1) {
	    MSG_ERR(FBDEV "Can't mmap %s: %s\n", priv_conf.dev_name, strerror(errno));
	    return MPXP_False;
	}
	memset(priv->frame_buffer, 0, priv->line_len * priv->yres);
	if(vidix_start(vo)!=0) { vidix_term(vo); return MPXP_False; }
    } else
#endif
    {
	if ((priv->frame_buffer = (uint8_t *) mmap(0, priv->size, PROT_READ | PROT_WRITE,
				    MAP_SHARED, priv->dev_fd, 0)) == (uint8_t *) -1) {
	    MSG_ERR(FBDEV "Can't mmap %s: %s\n", priv_conf.dev_name, strerror(errno));
	    return MPXP_False;
	}
	if(priv->out_width > priv->xres) priv->out_width=priv->xres;
	if(priv->out_height > priv->yres) priv->out_width=priv->yres;
	priv->L123123875 = priv->frame_buffer + x_offset * priv->pixel_size + y_offset * priv->line_len;
	MSG_DBG2(FBDEV "priv->frame_buffer @ %p\n", priv->frame_buffer);
	MSG_DBG2(FBDEV "priv->L123123875 @ %p\n", priv->L123123875);
	MSG_V(FBDEV "pixel per priv->line: %d\n", priv->line_len / priv->pixel_size);

	priv->total_fr=vo_conf.xp_buffs;
	for(i=0;i<priv->total_fr;i++)
	    if (!(priv->next_frame[i] = (uint8_t *) mp_malloc(priv->out_width * priv->out_height * priv->pixel_size))) {
		MSG_ERR(FBDEV "Can't mp_malloc priv->next_frame: %s\n", strerror(errno));
		return MPXP_False;
	    }
    }
    if (priv->vt_doit && (priv->vt_fd = open("/dev/tty", O_WRONLY)) == -1) {
	MSG_ERR(FBDEV "can't open /dev/tty: %s\n", strerror(errno));
	priv->vt_doit = 0;
    }
    if (priv->vt_doit && !(priv->vt_fp = fdopen(priv->vt_fd, "w"))) {
	MSG_ERR(FBDEV "can't fdopen /dev/tty: %s\n", strerror(errno));
	priv->vt_doit = 0;
    }

    if (priv->vt_doit)
	vt_set_textarea(vo,priv->last_row, priv->yres);

    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ query_format(vo_data_t*vo,vo_query_fourcc_t * format)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    format->flags=VOCAP_NA;
    switch(format->fourcc) {
	case IMGFMT_BGR15: if(priv->bpp == 15) format->flags=VOCAP_SUPPORTED; break;
	case IMGFMT_BGR16: if(priv->bpp == 16) format->flags=VOCAP_SUPPORTED; break;
	case IMGFMT_BGR24: if(priv->bpp == 24) format->flags=VOCAP_SUPPORTED; break;
	case IMGFMT_BGR32: if(priv->bpp == 32) format->flags=VOCAP_SUPPORTED; break;
	default: break;
    }
    return MPXP_Ok;
}

static const vo_info_t *get_info(const vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}

static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
#ifdef CONFIG_VIDIX
    if(priv->vidix_server) {
	priv->vidix_server->select_frame(vo,idx);
	return;
    }
#endif
    unsigned i, out_offset = 0, in_offset = 0;

    for (i = 0; i < priv->out_height; i++) {
	memcpy( priv->L123123875 + out_offset, priv->next_frame[idx] + in_offset,
		priv->out_width * priv->pixel_size);
	out_offset += priv->line_len;
	in_offset += priv->out_width * priv->pixel_size;
    }
}

static void uninit(vo_data_t*vo)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    unsigned i;
    MSG_V(FBDEV "uninit\n");
    if (priv->cmap_changed) {
	if (ioctl(priv->dev_fd, FBIOPUTCMAP, &priv->oldcmap))
		MSG_ERR(FBDEV "Can't restore original cmap\n");
	priv->cmap_changed = 0;
    }
    for(i=0;i<priv->total_fr;i++) delete priv->next_frame[i];
    if (ioctl(priv->dev_fd, FBIOGET_VSCREENINFO, &priv->vinfo))
	MSG_ERR(FBDEV "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
    priv->orig_vinfo.xoffset = priv->vinfo.xoffset;
    priv->orig_vinfo.yoffset = priv->vinfo.yoffset;
    if (ioctl(priv->dev_fd, FBIOPUT_VSCREENINFO, &priv->orig_vinfo))
	MSG_ERR(FBDEV "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
    if (priv->tty_fd >= 0) {
		if (ioctl(priv->tty_fd, KDSETMODE, KD_TEXT) < 0)
		    MSG_ERR(FBDEV "Can't restore text mode: %s\n", strerror(errno));
    }
    if (priv->vt_doit) vt_set_textarea(vo,0, priv->orig_vinfo.yres);
    close(priv->tty_fd);
    close(priv->dev_fd);
    if(priv->frame_buffer) munmap(priv->frame_buffer, priv->size);
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) vidix_term(vo);
    delete priv->vidix_server;
#endif
    delete vo->priv;
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    priv_t*priv;
    priv=new(zeromem) priv_t;
    vo->priv=priv;
    priv_conf.mode_cfgfile = "/etc/priv->modes";
    priv->vt_doit = 1;
    priv->pre_init_err = MPXP_Ok;
    if(arg) parseSubDevice(priv,arg);
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) {
	if(!(priv->vidix_server=vidix_preinit(vo,priv->vidix_name,&video_out_fbdev)))
	    priv->pre_init_err=MPXP_False;
    }
    MSG_DBG2("vo_subdevice: initialization returns: %i\n",priv->pre_init_err);
#endif
    if(priv->pre_init_err) priv->pre_init_err=fb_preinit(vo);
    return priv->pre_init_err;
}

static void __FASTCALL__ fbdev_dri_get_surface_caps(vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = priv->dstFourcc;
    caps->width=priv->out_width;
    caps->height=priv->out_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv->out_width;
    caps->h=priv->out_height;
    caps->strides[0] = (priv->out_width)*((priv->bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ fbdev_dri_get_surface(vo_data_t*vo,dri_surface_t *surf)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
    surf->planes[0] = priv->next_frame[surf->idx];
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t*priv=reinterpret_cast<priv_t*>(vo->priv);
#ifdef CONFIG_VIDIX
    if(priv->vidix_server)
	if(priv->vidix_server->control(vo,request,data)==MPXP_Ok) return MPXP_Ok;
#endif
    switch (request) {
	case VOCTRL_QUERY_FORMAT:
	    return query_format(vo,(vo_query_fourcc_t*)data);
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = priv->total_fr;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    fbdev_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    fbdev_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	    return MPXP_True;
    }
    return MPXP_NA;
}
