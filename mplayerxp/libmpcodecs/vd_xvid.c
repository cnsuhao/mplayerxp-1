/*
   Xvid codec - is successor of odivx
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "mp_config.h"
#include "help_mp.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd_internal.h"
#include "codecs_ld.h"
#include "libvo/video_out.h"

static const vd_info_t info = {
	"XviD MPEG4 codec ",
	"xvid",
	"Nickols_K",
	"http://www.xvid.org <(C) Christoph Lampert (gruel@web.de)>",
	"native codecs"
};

typedef struct {
	int cs;
	unsigned char img_type;
	void* hdl;
	mp_image_t* mpi;
	int vo_initialized;
	int pp_level;
	int brightness;
	int resync;
} priv_t;

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(xvid)

#include "interface/xvid.h"

static int (*xvid_decore_ptr)(void* handle,
			int dec_opt,
			void *param1,
			void *param2);
static int (*xvid_global_ptr)(void *handle, int opt, void *param1, void *param2);
static void *dll_handle;


static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,"http://www.xvid.org"))) return 0;
  xvid_decore_ptr = ld_sym(dll_handle,"xvid_decore");
  xvid_global_ptr = ld_sym(dll_handle,"xvid_global");
  return xvid_decore_ptr != NULL && xvid_global_ptr!=NULL;
}

/* Returns DAR value according to VOL's informations contained in stats
 * param */
static float stats2aspect(xvid_dec_stats_t *stats)
{
	if (stats->type == XVID_TYPE_VOL) {
		float wpar;
		float hpar;
		float dar;
	
		/* MPEG4 strem stores PAR (Pixel Aspect Ratio), mplayer uses
		 * DAR (Display Aspect Ratio)
		 * 
		 * Both are related thanks to the equation:
		 *            width 
		 *      DAR = ----- x PAR
		 *            height
		 *
		 * As MPEG4 is so well designed (*cough*), VOL header carries
		 * both informations together -- lucky eh ? */

		switch (stats->data.vol.par) {
		case XVID_PAR_11_VGA: /* 1:1 vga (square), default if supplied PAR is not a valid value */
			wpar = hpar = 1.0f;
			break;
		case XVID_PAR_43_PAL: /* 4:3 pal (12:11 625-line) */
			wpar = 12;
			hpar = 11;
			break;
		case XVID_PAR_43_NTSC: /* 4:3 ntsc (10:11 525-line) */
			wpar = 10;
			hpar = 11;
			break;
		case XVID_PAR_169_PAL: /* 16:9 pal (16:11 625-line) */
			wpar = 16;
			hpar = 11;
			break;
		case XVID_PAR_169_NTSC: /* 16:9 ntsc (40:33 525-line) */
			wpar = 40;
			hpar = 33;
			break;
		case XVID_PAR_EXT: /* extended par; use par_width, par_height */
			wpar = stats->data.vol.par_width;
			hpar = stats->data.vol.par_height;
			break;
		default:
			wpar = hpar = 1.0f;
			break;
		}

		dar  = ((float)stats->data.vol.width*wpar);
		dar /= ((float)stats->data.vol.height*hpar);

		return(dar);
	}

	return(0.0f);
}

// init driver
static int init(sh_video_t *sh){
	xvid_gbl_info_t xvid_gbl_info;
	xvid_gbl_init_t xvid_ini;
	xvid_dec_create_t dec_p;
	priv_t* p;
	int cs;

    if(!load_lib("libxvidcore"SLIBSUFFIX)) return 0;

	memset(&xvid_gbl_info, 0, sizeof(xvid_gbl_info_t));
	xvid_gbl_info.version = XVID_VERSION;
	
	memset(&xvid_ini, 0, sizeof(xvid_gbl_init_t));
	xvid_ini.version = XVID_VERSION;
	
	memset(&dec_p, 0, sizeof(xvid_dec_create_t));
	dec_p.version = XVID_VERSION;

	switch(sh->codec->outfmt[sh->outfmtidx]){
	case IMGFMT_YUY2:
		cs = XVID_CSP_YUY2;
		break;
	case IMGFMT_UYVY:
		cs = XVID_CSP_UYVY;
		break;
	case IMGFMT_YVYU:
		cs = XVID_CSP_YVYU;
		break;
	case IMGFMT_YV12:
		/* We will use our own buffers, this speeds decoding avoiding
		 * frame memcpy's overhead */
		cs = XVID_CSP_YV12;
		break;
	case IMGFMT_I420: 
	case IMGFMT_IYUV:
		cs = XVID_CSP_I420;
		break;
	case IMGFMT_BGR15: 
		cs = XVID_CSP_RGB555;
		break;
	case IMGFMT_BGR16: 
		cs = XVID_CSP_RGB565;
		break;
	case IMGFMT_BGR24: 
		cs = XVID_CSP_BGR;
		break;
	case IMGFMT_BGR32:
		cs = XVID_CSP_BGRA;
		break;
	default:
		MSG_ERR("Unsupported out_fmt: 0x%X\n",
		       sh->codec->outfmt[sh->outfmtidx]);
		return 0;
	}

	/* Gather some information about the host library */
	if((*xvid_global_ptr)(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL) < 0) {
		MSG_INFO("xvid: could not get information about the library\n");
	} else {
		MSG_INFO("xvid: using library version %d.%d.%d (build %s)\n",
		       XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		       xvid_gbl_info.build);
	}
	if(xvid_gbl_info.actual_version < XVID_VERSION) {
		MSG_ERR("xvid: please upgrade xvid library from %d.%d.%d to %d.%d.%d\n",
		       XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		       XVID_VERSION_MAJOR(XVID_VERSION),
		       XVID_VERSION_MINOR(XVID_VERSION),
		       XVID_VERSION_PATCH(XVID_VERSION));
		return 0;
	}
	
	xvid_ini.cpu_flags = 0; /* autodetect */
	xvid_ini.debug = 0;
	/* Initialize the xvidcore library */
	if((*xvid_global_ptr)(NULL, XVID_GBL_INIT, &xvid_ini, NULL))
		return(0);

	/* We use 0 width and height so xvidcore will resize its buffers
	 * if required. That allows this vd plugin to do resize on first
	 * VOL encountered (don't trust containers' width and height) */
	dec_p.width =
	dec_p.height = 0;

	/* Get a decoder instance */
	if((*xvid_decore_ptr)(0, XVID_DEC_CREATE, &dec_p, NULL)<0) {
		MSG_ERR("XviD init failed\n");
		return 0;
	}

	if((*xvid_global_ptr)(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL))
		return(0);
	MSG_INFO("xvid: using %u cpus/threads. Flags: %08X\n"
		,xvid_gbl_info.num_threads
		,xvid_gbl_info.cpu_flags);

	p = malloc(sizeof(priv_t));
	p->cs = cs;
	p->hdl = dec_p.handle;
	p->vo_initialized = 0;
	sh->context = p;

	switch(cs) {
	case XVID_CSP_INTERNAL:
		p->img_type = MP_IMGTYPE_EXPORT;
		break;
	case XVID_CSP_USER:
		p->img_type = MP_IMGTYPE_STATIC;
		break;
	default:
		p->img_type = MP_IMGTYPE_TEMP;
		break;
	}
	return mpcodecs_config_vo(sh, sh->disp_w, sh->disp_h, NULL);
}

// uninit driver
static void uninit(sh_video_t *sh){
    priv_t* p = sh->context;
    if(!p) return;
    (*xvid_decore_ptr)(p->hdl,XVID_DEC_DESTROY, NULL, NULL);
    free(p);
    dlclose(dll_handle);
}


// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
	xvid_dec_frame_t dec;
	xvid_dec_stats_t stats;
	mp_image_t* mpi = NULL;
	int consumed;
	priv_t* p = sh->context;


	if(!data || len <= 0) return NULL;

	memset(&dec,0,sizeof(xvid_dec_frame_t));
	memset(&stats, 0, sizeof(xvid_dec_stats_t));
	dec.version = XVID_VERSION;
	stats.version = XVID_VERSION;

	dec.bitstream = data;
	dec.length = len;
	dec.general = 0;
	dec.brightness = p->brightness;
	if(!p->pp_level)	dec.general |= XVID_LOWDELAY | XVID_DEC_FAST;
	if(p->pp_level>0)	dec.general = 0;
	if(p->pp_level>1)	dec.general |= XVID_DEBLOCKY;
	if(p->pp_level>2)	dec.general |= XVID_DEBLOCKUV;
	if(p->pp_level>3)	dec.general |= XVID_DERINGY;
	if(p->pp_level>4)	dec.general |= XVID_DERINGUV;
	if(p->resync)		{ dec.general |= XVID_DISCONTINUITY; p->resync=0; }

	if(flags&3) dec.general |= XVID_DEC_DROP;
	dec.output.csp = p->cs;
	mpi = mpcodecs_get_image(sh, p->img_type,  MP_IMGFLAG_ACCEPT_STRIDE,
				 sh->disp_w, sh->disp_h);
	if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
	if(p->cs != XVID_CSP_INTERNAL) {
	    dec.output.plane[0] = mpi->planes[0];
	    dec.output.plane[1] = mpi->planes[1];
	    dec.output.plane[2] = mpi->planes[2];
	    dec.output.stride[0] = mpi->stride[0];
	    dec.output.stride[1] = mpi->stride[1];
	    dec.output.stride[2] = mpi->stride[2];
	}
	while(dec.length > 0) {

		/* Decode data */
		consumed = (*xvid_decore_ptr)(p->hdl, XVID_DEC_DECODE, &dec, &stats);
		if (consumed < 0) {
			MSG_ERR("Decoding error\n");
			return NULL;
		}
		if(!(stats.type == XVID_TYPE_VOL || stats.type == XVID_TYPE_NOTHING)) break;
		dec.bitstream += consumed;
		dec.length -= consumed;
	}
	if (mpi != NULL && p->cs == XVID_CSP_INTERNAL) {
	    mpi->planes[0] = dec.output.plane[0];
	    mpi->planes[1] = dec.output.plane[1];
	    mpi->planes[2] = dec.output.plane[2];

	    mpi->stride[0] = dec.output.stride[0];
	    mpi->stride[1] = dec.output.stride[1];
	    mpi->stride[2] = dec.output.stride[2];
	}
	return (stats.type == XVID_TYPE_NOTHING)?NULL:mpi;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    priv_t* p = sh->context;
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    return 5; // for xvid_linux
	case VDCTRL_SET_PP_LEVEL: {
	    int quality=*((int*)arg);
	    if(quality<0 || quality>5) quality=5;
	    p->pp_level=quality;
	    return CONTROL_OK;
	}
	case VDCTRL_SET_EQUALIZER: {
	    va_list ap;
	    int value;
	    va_start(ap, arg);
	    value=va_arg(ap, int);
	    va_end(ap);

	    if(strcmp(arg,VO_EC_BRIGHTNESS)!=0) return CONTROL_FALSE;
	
	    p->brightness = (value * 256) / 100;
	    return CONTROL_OK;
	}
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY || 
		*((int*)arg) == IMGFMT_YVYU || 
		*((int*)arg) == IMGFMT_YV12 || 
		*((int*)arg) == IMGFMT_I420 || 
		*((int*)arg) == IMGFMT_IYUV || 
		*((int*)arg) == IMGFMT_BGR15 || 
		*((int*)arg) == IMGFMT_BGR16 || 
		*((int*)arg) == IMGFMT_BGR24 || 
		*((int*)arg) == IMGFMT_BGR32)
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
	case VDCTRL_RESYNC_STREAM:
	    p->resync=1;
	    return CONTROL_TRUE;
	default: break;
    }
    return CONTROL_UNKNOWN;
}
