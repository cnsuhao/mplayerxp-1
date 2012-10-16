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
#include "../mplayer.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "sub.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "aspect.h"
#include "osd.h"
#include "dri_vo.h"
#include "../libmpdemux/mrl.h"

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

#ifdef CONFIG_VIDIX
/* Name of VIDIX driver */
static const char *vidix_name = NULL;
#endif
static signed int pre_init_err = -2;
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

#define PRINT_LINENUM MSG_DBG2(" at line %d\n", line_num)

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

typedef struct fb_priv_s {
    FILE *		fp;
    int			line_num;
    char *		line;
    char *		token[MAX_NR_TOKEN];
    uint32_t		srcFourcc,dstFourcc;
/* command line/config file options */
    char *		dev_name;
    char *		mode_cfgfile;
    char *		mode_name;
    char *		monitor_hfreq_str;
    char *		monitor_vfreq_str;
    char *		monitor_dotclock_str;
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
}fb_priv_t;
static fb_priv_t fb;


static int __FASTCALL__ get_token(int num)
{
	static int read_nextline = 1;
	static int line_pos;
	int i;
	char c;

	if (num >= MAX_NR_TOKEN) {
		MSG_ERR("get_token(): max >= MAX_NR_TOKEN!");
		goto out_eof;
	}

	if (read_nextline) {
		if (!fgets(fb.line, MAX_LINE_LEN, fb.fp))
			goto out_eof;
		line_pos = 0;
		++fb.line_num;
		read_nextline = 0;
	}
	for (i = 0; i < num; i++) {
		while (isspace(fb.line[line_pos]))
			++line_pos;
		if (fb.line[line_pos] == '\0' || fb.line[line_pos] == '#') {
			read_nextline = 1;
			goto out_eol;
		}
		fb.token[i] = fb.line + line_pos;
		c = fb.line[line_pos];
		if (c == '"' || c == '\'') {
			fb.token[i]++;
			while (fb.line[++line_pos] != c && fb.line[line_pos])
				/* NOTHING */;
			if (!fb.line[line_pos])
				goto out_eol;
			fb.line[line_pos] = ' ';
		} else {
			for (/* NOTHING */; !isspace(fb.line[line_pos]) &&
					fb.line[line_pos]; line_pos++)
				/* NOTHING */;
		}
		if (!fb.line[line_pos]) {
			read_nextline = 1;
			if (i == num - 1)
				goto out_ok;
			goto out_eol;
		}
		fb.line[line_pos++] = '\0';
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

static int __FASTCALL__ parse_fbmode_cfg(char *cfgfile)
{
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

	if ((fb.fp = fopen(cfgfile, "r")) == NULL) {
		MSG_ERR("can't open '%s': %s\n", cfgfile, strerror(errno));
		return -1;
	}

	if ((fb.line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		MSG_ERR("can't get memory for 'fb.line': %s\n", strerror(errno));
		return -2;
	}

	/*
	 * check if the cfgfile starts with 'mode'
	 */
	while ((tmp = get_token(1)) == RET_EOL)
		/* NOTHING */;
	if (tmp == RET_EOF)
		goto out;
	if (!strcmp(fb.token[0], "mode"))
		goto loop_enter;
	goto err_out_parse_error;

	while ((tmp = get_token(1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(fb.token[0], "mode")) {
			if (in_mode_def) {
				MSG_ERR("'endmode' required");
				goto err_out_print_linenum;
			}
			if (!validate_mode(mode))
				goto err_out_not_valid;
		loop_enter:
		        if (!(fb_modes = (fb_mode_t *) realloc(fb_modes,
				sizeof(fb_mode_t) * (nr_modes + 1)))) {
			    MSG_ERR("can't realloc 'fb_modes' (nr_modes = %d):"
					    " %s\n", nr_modes, strerror(errno));
			    goto err_out;
		        }
			mode=fb_modes + nr_modes;
			++nr_modes;
                        memset(mode,0,sizeof(fb_mode_t));

			if (get_token(1) < 0)
				goto err_out_parse_error;
			for (i = 0; i < nr_modes - 1; i++) {
				if (!strcmp(fb.token[0], fb_modes[i].name)) {
					MSG_ERR("mode name '%s' isn't unique", fb.token[0]);
					goto err_out_print_linenum;
				}
			}
			if (!(mode->name = strdup(fb.token[0]))) {
				MSG_ERR("can't strdup -> 'name': %s\n", strerror(errno));
				goto err_out;
			}
			in_mode_def = 1;
		} else if (!strcmp(fb.token[0], "geometry")) {
			CHECK_IN_MODE_DEF;
			if (get_token(5) < 0)
				goto err_out_parse_error;
			mode->xres = strtoul(fb.token[0], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->yres = strtoul(fb.token[1], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vxres = strtoul(fb.token[2], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vyres = strtoul(fb.token[3], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->depth = strtoul(fb.token[4], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "timings")) {
			CHECK_IN_MODE_DEF;
			if (get_token(7) < 0)
				goto err_out_parse_error;
			mode->pixclock = strtoul(fb.token[0], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->left = strtoul(fb.token[1], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->right = strtoul(fb.token[2], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->upper = strtoul(fb.token[3], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->lower = strtoul(fb.token[4], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->hslen = strtoul(fb.token[5], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
			mode->vslen = strtoul(fb.token[6], &endptr, 0);
			if (*endptr)
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "endmode")) {
			CHECK_IN_MODE_DEF;
			in_mode_def = 0;
		} else if (!strcmp(fb.token[0], "accel")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			/*
			 * it's only used for text acceleration
			 * so we just ignore it.
			 */
		} else if (!strcmp(fb.token[0], "hsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "low"))
				mode->sync &= ~FB_SYNC_HOR_HIGH_ACT;
			else if(!strcmp(fb.token[0], "high"))
				mode->sync |= FB_SYNC_HOR_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "vsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "low"))
				mode->sync &= ~FB_SYNC_VERT_HIGH_ACT;
			else if(!strcmp(fb.token[0], "high"))
				mode->sync |= FB_SYNC_VERT_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "csync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "low"))
				mode->sync &= ~FB_SYNC_COMP_HIGH_ACT;
			else if(!strcmp(fb.token[0], "high"))
				mode->sync |= FB_SYNC_COMP_HIGH_ACT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "extsync")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "false"))
				mode->sync &= ~FB_SYNC_EXT;
			else if(!strcmp(fb.token[0], "true"))
				mode->sync |= FB_SYNC_EXT;
			else
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "laced")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "false"))
				mode->vmode = FB_VMODE_NONINTERLACED;
			else if (!strcmp(fb.token[0], "true"))
				mode->vmode = FB_VMODE_INTERLACED;
			else
				goto err_out_parse_error;
		} else if (!strcmp(fb.token[0], "double")) {
			CHECK_IN_MODE_DEF;
			if (get_token(1) < 0)
				goto err_out_parse_error;
			if (!strcmp(fb.token[0], "false"))
				;
			else if (!strcmp(fb.token[0], "true"))
				mode->vmode = FB_VMODE_DOUBLE;
			else
				goto err_out_parse_error;
		} else
			goto err_out_parse_error;
	}
	if (!validate_mode(mode))
		goto err_out_not_valid;
out:
	MSG_DBG2("%d modes\n", nr_modes);
	free(fb.line);
	fclose(fb.fp);
	return nr_modes;
err_out_parse_error:
	MSG_ERR("parse error");
err_out_print_linenum:
	PRINT_LINENUM;
err_out:
	if (fb_modes) {
		free(fb_modes);
		fb_modes = NULL;
	}
	nr_modes = 0;
	free(fb.line);
	free(fb.fp);
	return -2;
err_out_not_valid:
	MSG_ERR("previous mode is not correct");
	goto err_out_print_linenum;
}

static fb_mode_t * __FASTCALL__ find_mode_by_name(char *name)
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
	if (ret)
		MSG_DBG2(" hsync, vsync, dotclock ok.\n");
	else
		MSG_DBG2("\n");

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
			} else 
				MSG_DBG2("too small.\n");
		} else if (curr->xres == best->xres && curr->yres == best->yres &&
				vsf(curr) > vsf(best)) {
			MSG_DBG2("faster screen refresh.\n");
			best = curr;
		} else if ((curr->xres <= best->xres && curr->yres <= best->yres) &&
				(curr->xres >= xres && curr->yres >= yres)) {
			MSG_DBG2("better than %dx%d, which is too large.\n",
						best->xres, best->yres);
			best = curr;
		} else
		{
			if (curr->xres < xres || curr->yres < yres)
				MSG_DBG2("too small.\n");
			else if (curr->xres > best->xres || curr->yres > best->yres)
				MSG_DBG2("too large.\n");
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

static range_t * __FASTCALL__ str2range(char *s)
{
	float tmp_min, tmp_max;
	char *endptr = s;	// to start the loop
	range_t *r = NULL;
	int i;

	if (!s)
		return NULL;
	for (i = 0; *endptr; i++) {
		if (*s == ',')
			goto out_err;
		if (!(r = (range_t *) realloc(r, sizeof(*r) * (i + 2)))) {
			MSG_ERR("can't realloc 'r'\n");
			return NULL;
		}
		tmp_min = strtod(s, &endptr);
		if (*endptr == 'k' || *endptr == 'K') {
			tmp_min *= 1000.0;
			endptr++;
		} else if (*endptr == 'm' || *endptr == 'M') {
			tmp_min *= 1000000.0;
			endptr++;
		}
		if (*endptr == '-') {
			tmp_max = strtod(endptr + 1, &endptr);
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
		} else
			goto out_err;
		r[i].min = tmp_min;
		r[i].max = tmp_max;
		if (r[i].min < 0 || r[i].max < 0)
			goto out_err;
		s = endptr + 1;
	}
	r[i].min = r[i].max = -1;
	return r;
out_err:
	if (r)
		free(r);
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
  
  red = malloc(cols * sizeof(red[0]));
  if(!red) {
	  MSG_ERR("Can't allocate red palette with %d entries.\n", cols);
	  return NULL;
  }
  for(i=0; i< rcols; i++)
    red[i] = (65535/(rcols-1)) * i;
  
  green = malloc(cols * sizeof(green[0]));
  if(!green) {
	  MSG_ERR("Can't allocate green palette with %d entries.\n", cols);
	  free(red);
	  return NULL;
  }
  for(i=0; i< gcols; i++)
    green[i] = (65535/(gcols-1)) * i;
  
  blue = malloc(cols * sizeof(blue[0]));
  if(!blue) {
	  MSG_ERR("Can't allocate blue palette with %d entries.\n", cols);
	  free(red);
	  free(green);
	  return NULL;
  }
  for(i=0; i< bcols; i++)
    blue[i] = (65535/(bcols-1)) * i;
  
  cmap = malloc(sizeof(struct fb_cmap));
  if(!cmap) {
	  MSG_ERR("Can't allocate color map\n");
	  free(red);
	  free(green);
	  free(blue);
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

static mrl_config_t fbconf[]=
{
	{ "modeconfig", &fb.mode_cfgfile, MRL_TYPE_STRING, 0, 0 },
	{ "hfreq", &fb.monitor_hfreq_str, MRL_TYPE_STRING, 0, 0 },
	{ "vfreq", &fb.monitor_vfreq_str, MRL_TYPE_STRING, 0, 0 },
	{ "dotclock", &fb.monitor_dotclock_str, MRL_TYPE_STRING, 0, 0 },
	{ NULL, NULL, 0, 0, 0 },
};

static uint32_t __FASTCALL__ parseSubDevice(const char *sd)
{
    const char *param;
#ifdef CONFIG_VIDIX
   if(memcmp(sd,"vidix",5) == 0) vidix_name = &sd[5]; /* vidix_name will be valid within init() */
   else
#endif
    {
	param=mrl_parse_line(sd,NULL,NULL,&fb.dev_name,&fb.mode_name);
	mrl_parse_params(param,fbconf);
    }
    return 0;
}

static int fb_preinit(void)
{
	static int fb_preinit_done = 0;
	static int fb_works = 0;

	if (fb_preinit_done)
		return fb_works;

	if (!fb.dev_name && !(fb.dev_name = getenv("FRAMEBUFFER")))
		fb.dev_name = "/dev/fb0";
	MSG_DBG2(FBDEV "using %s\n", fb.dev_name);

	if ((fb.dev_fd = open(fb.dev_name, O_RDWR)) == -1) {
		MSG_ERR(FBDEV "Can't open %s: %s\n", fb.dev_name, strerror(errno));
		goto err_out;
	}
	if (ioctl(fb.dev_fd, FBIOGET_VSCREENINFO, &fb.vinfo)) {
		MSG_ERR(FBDEV "Can't get VSCREENINFO: %s\n", strerror(errno));
		goto err_out_fd;
	}
	fb.orig_vinfo = fb.vinfo;

        if ((fb.tty_fd = open("/dev/tty", O_RDWR)) < 0) {
		MSG_DBG2(FBDEV "notice: Can't open /dev/tty: %s\n", strerror(errno));
        }

	fb.bpp = fb.vinfo.bits_per_pixel;

	if (fb.bpp == 8 && !vo.dbpp) {
		MSG_ERR(FBDEV "8 bpp output is not supported.\n");
		goto err_out_tty_fd;
	}

	/* 16 and 15 bpp is reported as 16 bpp */
	if (fb.bpp == 16)
		fb.bpp = fb.vinfo.red.length + fb.vinfo.green.length +
			fb.vinfo.blue.length;

	if (vo.dbpp) {
		if (vo.dbpp != 15 && vo.dbpp != 16 && vo.dbpp != 24 &&
				vo.dbpp != 32) {
			MSG_ERR(FBDEV "can't switch to %d bpp\n", vo.dbpp);
			goto err_out_fd;
		}
		fb.bpp = vo.dbpp;
	}

	fb_preinit_done = 1;
	fb_works = 1;
	return 1;
err_out_tty_fd:
	close(fb.tty_fd);
	fb.tty_fd = -1;
err_out_fd:
	close(fb.dev_fd);
	fb.dev_fd = -1;
err_out:
	fb_preinit_done = 1;
	fb_works = 0;
	return 0;
}

static void lots_of_printf(void)
{
	MSG_V(FBDEV "var info:\n");
	MSG_V(FBDEV "xres: %u\n", fb.vinfo.xres);
	MSG_V(FBDEV "yres: %u\n", fb.vinfo.yres);
	MSG_V(FBDEV "xres_virtual: %u\n", fb.vinfo.xres_virtual);
	MSG_V(FBDEV "yres_virtual: %u\n", fb.vinfo.yres_virtual);
	MSG_V(FBDEV "xoffset: %u\n", fb.vinfo.xoffset);
	MSG_V(FBDEV "yoffset: %u\n", fb.vinfo.yoffset);
	MSG_V(FBDEV "bits_per_pixel: %u\n", fb.vinfo.bits_per_pixel);
	MSG_V(FBDEV "grayscale: %u\n", fb.vinfo.grayscale);
	MSG_V(FBDEV "red: %lu %lu %lu\n",
			(unsigned long) fb.vinfo.red.offset,
			(unsigned long) fb.vinfo.red.length,
			(unsigned long) fb.vinfo.red.msb_right);
	MSG_V(FBDEV "green: %lu %lu %lu\n",
			(unsigned long) fb.vinfo.green.offset,
			(unsigned long) fb.vinfo.green.length,
			(unsigned long) fb.vinfo.green.msb_right);
	MSG_V(FBDEV "blue: %lu %lu %lu\n",
			(unsigned long) fb.vinfo.blue.offset,
			(unsigned long) fb.vinfo.blue.length,
			(unsigned long) fb.vinfo.blue.msb_right);
	MSG_V(FBDEV "transp: %lu %lu %lu\n",
			(unsigned long) fb.vinfo.transp.offset,
			(unsigned long) fb.vinfo.transp.length,
			(unsigned long) fb.vinfo.transp.msb_right);
	MSG_V(FBDEV "nonstd: %u\n", fb.vinfo.nonstd);
	MSG_DBG2(FBDEV "activate: %u\n", fb.vinfo.activate);
	MSG_DBG2(FBDEV "height: %u\n", fb.vinfo.height);
	MSG_DBG2(FBDEV "width: %u\n", fb.vinfo.width);
	MSG_DBG2(FBDEV "accel_flags: %u\n", fb.vinfo.accel_flags);
	MSG_DBG2(FBDEV "timing:\n");
	MSG_DBG2(FBDEV "pixclock: %u\n", fb.vinfo.pixclock);
	MSG_DBG2(FBDEV "left_margin: %u\n", fb.vinfo.left_margin);
	MSG_DBG2(FBDEV "right_margin: %u\n", fb.vinfo.right_margin);
	MSG_DBG2(FBDEV "upper_margin: %u\n", fb.vinfo.upper_margin);
	MSG_DBG2(FBDEV "lower_margin: %u\n", fb.vinfo.lower_margin);
	MSG_DBG2(FBDEV "hsync_len: %u\n", fb.vinfo.hsync_len);
	MSG_DBG2(FBDEV "vsync_len: %u\n", fb.vinfo.vsync_len);
	MSG_DBG2(FBDEV "sync: %u\n", fb.vinfo.sync);
	MSG_DBG2(FBDEV "vmode: %u\n", fb.vinfo.vmode);
	MSG_V(FBDEV "fix info:\n");
	MSG_V(FBDEV "framebuffer size: %d bytes\n", fb.finfo.smem_len);
	MSG_V(FBDEV "type: %lu\n", (unsigned long) fb.finfo.type);
	MSG_V(FBDEV "type_aux: %lu\n", (unsigned long) fb.finfo.type_aux);
	MSG_V(FBDEV "visual: %lu\n", (unsigned long) fb.finfo.visual);
	MSG_V(FBDEV "line_length: %lu bytes\n", (unsigned long) fb.finfo.line_length);
	MSG_DBG2(FBDEV "id: %.16s\n", fb.finfo.id);
	MSG_DBG2(FBDEV "smem_start: %p\n", (void *) fb.finfo.smem_start);
	MSG_DBG2(FBDEV "xpanstep: %u\n", fb.finfo.xpanstep);
	MSG_DBG2(FBDEV "ypanstep: %u\n", fb.finfo.ypanstep);
	MSG_DBG2(FBDEV "ywrapstep: %u\n", fb.finfo.ywrapstep);
	MSG_DBG2(FBDEV "mmio_start: %p\n", (void *) fb.finfo.mmio_start);
	MSG_DBG2(FBDEV "mmio_len: %u bytes\n", fb.finfo.mmio_len);
	MSG_DBG2(FBDEV "accel: %u\n", fb.finfo.accel);
	MSG_V(FBDEV "fb.bpp: %d\n", fb.bpp);
	MSG_V(FBDEV "fb.real_bpp: %d\n", fb.real_bpp);
	MSG_V(FBDEV "fb.pixel_size: %d bytes\n", fb.pixel_size);
	MSG_V(FBDEV "other:\n");
	MSG_V(FBDEV "fb.in_width: %d\n", fb.in_width);
	MSG_V(FBDEV "fb.in_height: %d\n", fb.in_height);
	MSG_V(FBDEV "fb.out_width: %d\n", fb.out_width);
	MSG_V(FBDEV "fb.out_height: %d\n", fb.out_height);
	MSG_V(FBDEV "fb.last_row: %d\n", fb.last_row);
}

static void __FASTCALL__ vt_set_textarea(int u, int l)
{
	/* how can I determine the font height?
	 * just use 16 for now
	 */
	int urow = ((u + 15) / 16) + 1;
	int lrow = l / 16;

	if (verbose > 1)
		MSG_DBG2(FBDEV "vt_set_textarea(%d,%d): %d,%d\n", u, l, urow, lrow);
	fprintf(fb.vt_fp, "\33[%d;%dr\33[%d;%dH", urow, lrow, lrow, 0);
	fflush(fb.vt_fp);
}

static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format,const vo_tune_info_t *info)
{
	struct fb_cmap *cmap;
	int vm = fullscreen & 0x02;
	int zoom = fullscreen & 0x04?1:0;
	unsigned x_offset,y_offset,i;
	UNUSED(title);
	vo.fs = fullscreen & 0x01;
	vo.flip = fullscreen & 0x08;
	if(fb.fs) zoom++;
	fb.srcFourcc = format;
	if((int)pre_init_err == -2)
	{
	    MSG_ERR(FBDEV "Internal fatal error: init() was called before preinit()\n");
	    return -1;
	}

	if (pre_init_err) return 1;

	if (fb.mode_name && !vm) {
		MSG_ERR(FBDEV "-fbmode can only be used with -vm\n");
		return 1;
	}
	if (vm && (parse_fbmode_cfg(fb.mode_cfgfile) < 0))
			return 1;
	if (d_width && (zoom || vm)) {
		fb.out_width = d_width;
		fb.out_height = d_height;
	} else {
		fb.out_width = width;
		fb.out_height = height;
	}
	fb.in_width = width;
	fb.in_height = height;
	fb.pixel_format = format;

	if (fb.mode_name) {
		if (!(fb.mode = find_mode_by_name(fb.mode_name))) {
			MSG_ERR(FBDEV "can't find requested video mode\n");
			return 1;
		}
		fb_mode2fb_vinfo(fb.mode, &fb.vinfo);
	} else if (vm) {
		fb.monitor_hfreq = str2range(fb.monitor_hfreq_str);
		fb.monitor_vfreq = str2range(fb.monitor_vfreq_str);
		fb.monitor_dotclock = str2range(fb.monitor_dotclock_str);
		if (!fb.monitor_hfreq || !fb.monitor_vfreq || !fb.monitor_dotclock) {
			MSG_ERR(FBDEV "you have to specify the capabilities of"
					" the monitor.\n");
			return 1;
		}
		if (!(fb.mode = find_best_mode(fb.out_width, fb.out_height,
					fb.monitor_hfreq, fb.monitor_vfreq,
					fb.monitor_dotclock))) {
			MSG_ERR(FBDEV "can't find best video mode\n");
			return 1;
		}
		MSG_ERR(FBDEV "using mode %dx%d @ %.1fHz\n", fb.mode->xres,
				fb.mode->yres, vsf(fb.mode));
		fb_mode2fb_vinfo(fb.mode, &fb.vinfo);
	}
	fb.bpp_we_want = fb.bpp;
	set_bpp(&fb.vinfo, fb.bpp);
	fb.vinfo.xres_virtual = fb.vinfo.xres;
	fb.vinfo.yres_virtual = fb.vinfo.yres;

        if (fb.tty_fd >= 0 && ioctl(fb.tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
                    MSG_DBG2(FBDEV "Can't set graphics mode: %s\n", strerror(errno));
                close(fb.tty_fd);
                fb.tty_fd = -1;
        }

	if (ioctl(fb.dev_fd, FBIOPUT_VSCREENINFO, &fb.vinfo)) {
		MSG_ERR(FBDEV "Can't put VSCREENINFO: %s\n", strerror(errno));
                if (fb.tty_fd >= 0 && ioctl(fb.tty_fd, KDSETMODE, KD_TEXT) < 0) {
                        MSG_ERR(FBDEV "Can't restore text mode: %s\n", strerror(errno));
                }
		return 1;
	}

	fb.pixel_size = (fb.vinfo.bits_per_pixel+7) / 8;
	fb.real_bpp = fb.vinfo.red.length + fb.vinfo.green.length +
		fb.vinfo.blue.length;
	fb.bpp = (fb.pixel_size == 4) ? 32 : fb.real_bpp;
	if (fb.bpp_we_want != fb.bpp)
		MSG_ERR(FBDEV "requested %d bpp, got %d bpp!!!\n",
				fb.bpp_we_want, fb.bpp);

	switch (fb.bpp) {
	case 32:
		fb.dstFourcc = IMGFMT_BGR32;
		break;
	case 24:
		fb.dstFourcc = IMGFMT_BGR24;
		break;
	default:
	case 16:
		fb.dstFourcc = IMGFMT_BGR16;
		break;
	case 15:
		fb.dstFourcc = IMGFMT_BGR15;
		break;
	}

	if (vo.flip & ((((fb.pixel_format & 0xff) + 7) / 8) != fb.pixel_size)) {
		MSG_ERR(FBDEV "Flipped output with depth conversion is not "
				"supported\n");
		return 1;
	}

	fb.xres = fb.vinfo.xres;
	fb.yres = fb.vinfo.yres;
	fb.last_row = (fb.xres-fb.out_height) / 2;

	if (ioctl(fb.dev_fd, FBIOGET_FSCREENINFO, &fb.finfo)) {
		MSG_ERR(FBDEV "Can't get FSCREENINFO: %s\n", strerror(errno));
		return 1;
	}

	lots_of_printf();

	if (fb.finfo.type != FB_TYPE_PACKED_PIXELS) {
		MSG_ERR(FBDEV "type %d not supported\n", fb.finfo.type);
		return 1;
	}

	switch (fb.finfo.visual) {
		case FB_VISUAL_TRUECOLOR:
			break;
		case FB_VISUAL_DIRECTCOLOR:
			MSG_DBG2(FBDEV "creating cmap for directcolor\n");
			if (ioctl(fb.dev_fd, FBIOGETCMAP, &fb.oldcmap)) {
				MSG_ERR(FBDEV "can't get cmap: %s\n",
						strerror(errno));
				return 1;
			}
			if (!(cmap = make_directcolor_cmap(&fb.vinfo)))
				return 1;
			if (ioctl(fb.dev_fd, FBIOPUTCMAP, cmap)) {
				MSG_ERR(FBDEV "can't put cmap: %s\n",
						strerror(errno));
				return 1;
			}
			fb.cmap_changed = 1;
			free(cmap->red);
			free(cmap->green);
			free(cmap->blue);
			free(cmap);
			break;
		default:
			MSG_ERR(FBDEV "visual: %d not yet supported\n",
					fb.finfo.visual);
			return 1;
	}

	fb.line_len = fb.finfo.line_length;
	fb.size = fb.finfo.smem_len;
	fb.frame_buffer = NULL;
	memset(fb.next_frame,0,sizeof(fb.next_frame));
	fb.out_width=width;
	fb.out_height=height;
	if(zoom > 1)
	{
	        aspect_save_orig(width,height);
		aspect_save_prescale(d_width,d_height);
		aspect_save_screenres(fb.xres,fb.yres);
		aspect(&fb.out_width,&fb.out_height,A_ZOOM);
	}
	else
	if(fb.fs)
	{
		fb.out_width = fb.xres;
		fb.out_height = fb.yres;
	}
	if(fb.xres > fb.out_width)
	    x_offset = (fb.xres - fb.out_width) / 2;
	else x_offset = 0;
	if(fb.yres > fb.out_height)
	    y_offset = (fb.yres - fb.out_height) / 2;
	else y_offset = 0;

	
#ifdef CONFIG_VIDIX
	if(vidix_name)
	{
		if(vidix_init(width,height,x_offset,y_offset,fb.out_width,
			    fb.out_height,format,fb.bpp,
			    fb.xres,fb.yres,info) != 0)
		{
		    MSG_ERR(FBDEV "Can't initialize VIDIX driver\n");
		    vidix_name = NULL;
		    vidix_term();
		    return -1;
		}
		else MSG_V(FBDEV "Using VIDIX\n");
		if ((fb.frame_buffer = (uint8_t *) mmap(0, fb.size, PROT_READ | PROT_WRITE,
						     MAP_SHARED, fb.dev_fd, 0)) == (uint8_t *) -1) {
		    MSG_ERR(FBDEV "Can't mmap %s: %s\n", fb.dev_name, strerror(errno));
		    return -1;
		}
		memset(fb.frame_buffer, 0, fb.line_len * fb.yres);
		if(vidix_start()!=0) { vidix_term(); return -1; }
	}
	else
#endif
	{
	    if ((fb.frame_buffer = (uint8_t *) mmap(0, fb.size, PROT_READ | PROT_WRITE,
				    MAP_SHARED, fb.dev_fd, 0)) == (uint8_t *) -1) {
		MSG_ERR(FBDEV "Can't mmap %s: %s\n", fb.dev_name, strerror(errno));
		return 1;
	    }
	    if(fb.out_width > fb.xres) fb.out_width=fb.xres;
	    if(fb.out_height > fb.yres) fb.out_width=fb.yres;
	    fb.L123123875 = fb.frame_buffer + x_offset * fb.pixel_size
			+ y_offset * fb.line_len;
	    MSG_DBG2(FBDEV "fb.frame_buffer @ %p\n", fb.frame_buffer);
	    MSG_DBG2(FBDEV "fb.L123123875 @ %p\n", fb.L123123875);
	    MSG_V(FBDEV "pixel per fb.line: %d\n", fb.line_len / fb.pixel_size);

	    fb.total_fr=vo.doublebuffering?vo.da_buffs:1;
	    for(i=0;i<fb.total_fr;i++)
	    if (!(fb.next_frame[i] = (uint8_t *) malloc(fb.out_width * fb.out_height * fb.pixel_size))) {
		MSG_ERR(FBDEV "Can't malloc fb.next_frame: %s\n", strerror(errno));
		return 1;
	    }
	}
	if (fb.vt_doit && (fb.vt_fd = open("/dev/tty", O_WRONLY)) == -1) {
		MSG_ERR(FBDEV "can't open /dev/tty: %s\n", strerror(errno));
		fb.vt_doit = 0;
	}
	if (fb.vt_doit && !(fb.vt_fp = fdopen(fb.vt_fd, "w"))) {
		MSG_ERR(FBDEV "can't fdopen /dev/tty: %s\n", strerror(errno));
		fb.vt_doit = 0;
	}

	if (fb.vt_doit)
		vt_set_textarea(fb.last_row, fb.yres);

	return 0;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t * format)
{
    switch(format->fourcc)
    {
	case IMGFMT_BGR15: return fb.bpp == 15;
	case IMGFMT_BGR16: return fb.bpp == 16;
	case IMGFMT_BGR24: return fb.bpp == 24;
	case IMGFMT_BGR32: return fb.bpp == 32;
	default: break;
    }
    return 0;
}

static const vo_info_t *get_info(void)
{
	return &vo_info;
}

static void __FASTCALL__ change_frame(unsigned idx)
{
	unsigned i, out_offset = 0, in_offset = 0;

	for (i = 0; i < fb.out_height; i++) {
		memcpy(fb.L123123875 + out_offset, fb.next_frame[idx] + in_offset,
				fb.out_width * fb.pixel_size);
		out_offset += fb.line_len;
		in_offset += fb.out_width * fb.pixel_size;
	}
}

static void uninit(void)
{
	unsigned i;
	MSG_V(FBDEV "uninit\n");
	if (fb.cmap_changed) {
		if (ioctl(fb.dev_fd, FBIOPUTCMAP, &fb.oldcmap))
			MSG_ERR(FBDEV "Can't restore original cmap\n");
		fb.cmap_changed = 0;
	}
	for(i=0;i<fb.total_fr;i++) free(fb.next_frame[i]);
	if (ioctl(fb.dev_fd, FBIOGET_VSCREENINFO, &fb.vinfo))
		MSG_ERR(FBDEV "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
	fb.orig_vinfo.xoffset = fb.vinfo.xoffset;
	fb.orig_vinfo.yoffset = fb.vinfo.yoffset;
	if (ioctl(fb.dev_fd, FBIOPUT_VSCREENINFO, &fb.orig_vinfo))
		MSG_ERR(FBDEV "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
        if (fb.tty_fd >= 0) {
                if (ioctl(fb.tty_fd, KDSETMODE, KD_TEXT) < 0)
                        MSG_ERR(FBDEV "Can't restore text mode: %s\n", strerror(errno));
        }
	if (fb.vt_doit)
		vt_set_textarea(0, fb.orig_vinfo.yres);
        close(fb.tty_fd);
	close(fb.dev_fd);
	if(fb.frame_buffer) munmap(fb.frame_buffer, fb.size);
#ifdef CONFIG_VIDIX
	if(vidix_name) vidix_term();
#endif
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    memset(&fb,0,sizeof(fb_priv_t));
    fb.mode_cfgfile = "/etc/fb.modes";
    fb.vt_doit = 1;
    pre_init_err = 0;
    if(arg) parseSubDevice(arg);
#ifdef CONFIG_VIDIX
    if(vidix_name) pre_init_err = vidix_preinit(vidix_name,&video_out_fbdev);
    MSG_DBG2("vo_subdevice: initialization returns: %i\n",pre_init_err);
#endif
    if(!pre_init_err) return (pre_init_err=(fb_preinit()?0:-1));
    return pre_init_err;
}

static void __FASTCALL__ fbdev_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = fb.dstFourcc;
    caps->width=fb.out_width;
    caps->height=fb.out_height;
    caps->x=0;
    caps->y=0;
    caps->w=fb.out_width;
    caps->h=fb.out_height;
    caps->strides[0] = (fb.out_width)*((fb.bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ fbdev_dri_get_surface(dri_surface_t *surf)
{
    surf->planes[0] = fb.next_frame[surf->idx];
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static uint32_t __FASTCALL__ control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format((vo_query_fourcc_t*)data);
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = fb.total_fr;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	fbdev_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE: 
	fbdev_dri_get_surface(data);
	return VO_TRUE;
  }
  return VO_NOTIMPL;
}
