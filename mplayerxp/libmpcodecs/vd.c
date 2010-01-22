#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_config.h"
#include "help_mp.h"

#ifdef HAVE_MALLOC
#include <malloc.h>
#endif

#include "codec-cfg.h"

#include "../libvo/img_format.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "vd.h"
#include "../postproc/vf.h"
#include "vd_msg.h"

extern const vd_functions_t mpcodecs_vd_null;
extern const vd_functions_t mpcodecs_vd_ffmpeg;
extern const vd_functions_t mpcodecs_vd_dshow;
extern const vd_functions_t mpcodecs_vd_vfw;
extern const vd_functions_t mpcodecs_vd_vfwex;
extern const vd_functions_t mpcodecs_vd_divx4;
extern const vd_functions_t mpcodecs_vd_raw;
extern const vd_functions_t mpcodecs_vd_libdv;
extern const vd_functions_t mpcodecs_vd_xanim;
extern const vd_functions_t mpcodecs_vd_fli;
extern const vd_functions_t mpcodecs_vd_nuv;
extern const vd_functions_t mpcodecs_vd_mpng;
extern const vd_functions_t mpcodecs_vd_ijpg;
extern const vd_functions_t mpcodecs_vd_libmpeg2;
extern const vd_functions_t mpcodecs_vd_xvid;
extern const vd_functions_t mpcodecs_vd_mpegpes;
extern const vd_functions_t mpcodecs_vd_huffyuv;
extern const vd_functions_t mpcodecs_vd_xanim;
extern const vd_functions_t mpcodecs_vd_real;
extern const vd_functions_t mpcodecs_vd_dmo;
extern const vd_functions_t mpcodecs_vd_qtvideo;
extern const vd_functions_t mpcodecs_vd_theora;

const vd_functions_t* mpcodecs_vd_drivers[] = {
	&mpcodecs_vd_null,
	&mpcodecs_vd_ffmpeg,
#ifdef HAVE_WIN32LOADER
	&mpcodecs_vd_dshow,
	&mpcodecs_vd_vfw,
	&mpcodecs_vd_vfwex,
	&mpcodecs_vd_dmo,
	&mpcodecs_vd_qtvideo,
#endif
	&mpcodecs_vd_divx4,
	&mpcodecs_vd_raw,
	&mpcodecs_vd_nuv,
	&mpcodecs_vd_libmpeg2,
	&mpcodecs_vd_xvid,
	&mpcodecs_vd_mpegpes,
	&mpcodecs_vd_huffyuv,
	&mpcodecs_vd_xanim,
	&mpcodecs_vd_real,
#ifdef HAVE_LIBTHEORA
	&mpcodecs_vd_theora,
#endif
#ifdef HAVE_LIBDV
	&mpcodecs_vd_libdv,
#endif
	NULL
};
static unsigned int nddrivers=sizeof(mpcodecs_vd_drivers)/sizeof(vd_functions_t*);

void libmpcodecs_vd_register_options(m_config_t* cfg)
{
  unsigned i;
  for(i=0;i<nddrivers;i++)
  {
    if(mpcodecs_vd_drivers[i])
	if(mpcodecs_vd_drivers[i]->options)
	    m_config_register_options(cfg,mpcodecs_vd_drivers[i]->options);
  }
}

void vfm_help(void) {
  unsigned i;
  MSG_INFO("Available video codec families/drivers:\n");
  for(i=0;i<nddrivers;i++) {
    if(mpcodecs_vd_drivers[i])
	if(mpcodecs_vd_drivers[i]->options)
	    MSG_INFO("\t%-10s %s\n",mpcodecs_vd_drivers[i]->info->driver_name,mpcodecs_vd_drivers[i]->info->descr);
  }
  MSG_INFO("\n");
}

#include "libvo/video_out.h"

extern const vd_functions_t* mpvdec; // FIXME!

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, void *tune){
    int i,j;
    unsigned int out_fmt=0;
    int screen_size_x=0;//SCREEN_SIZE_X;
    int screen_size_y=0;//SCREEN_SIZE_Y;
    vf_instance_t* vf=sh->vfilter,*sc=NULL;
    int palette=0;

    if(!(sh->disp_w && sh->disp_h))
        MSG_WARN(
            "VDec: driver %s didn't set sh->disp_w and sh->disp_h, trying to workaround!\n"
	    ,sh->codec->codec_name);
    /* XXX: HACK, if sh->disp_* aren't set,
     * but we have w and h, set them :: atmos */
    if(!sh->disp_w && w)
        sh->disp_w=w;
    if(!sh->disp_h && h)
        sh->disp_h=h;

    MSG_V("VDec: vo config request - %d x %d\n",
	w,h);

csp_again:
    // check if libvo and codec has common outfmt (no conversion):
    j=-1;
    for(i=0;i<CODECS_MAX_OUTFMT;i++){
	int flags;
	out_fmt=sh->codec->outfmt[i];
	if(out_fmt==0xFFFFFFFF) continue;
	flags=vf_query_format(vf,out_fmt,w,h);
	MSG_DBG2("vo_debug[step i=%d]: query(%s %ix%i) returned 0x%X for:\n",i,vo_format_name(out_fmt),w,h,flags);
	if(verbose>1) if(verbose) vf_showlist(vf);
	if((flags&VFCAP_CSP_SUPPORTED_BY_HW) || ((flags&VFCAP_CSP_SUPPORTED) && j<0)){
	    // check (query) if codec really support this outfmt...
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)==CONTROL_FALSE) {
		MSG_DBG2("vo_debug: codec[%s] query_format(%s) returned FALSE\n",mpvdec->info->driver_name,vo_format_name(out_fmt));
		continue;
	    }
	    j=i; vo_flags=flags;
	    if(flags&VFCAP_CSP_SUPPORTED_BY_HW) break;
	} else
	if(!palette && !(vo_flags&3) && (out_fmt==IMGFMT_RGB8||out_fmt==IMGFMT_BGR8)){
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)!=CONTROL_FALSE)
		palette=1;
	}
    }
    if(j<0){
	// TODO: no match - we should use conversion...
	if(strcmp(vf->info->name,"fmtcvt") && palette!=1){
	    int ind;
	    MSG_WARN("Can't find colorspace for: ");
	    for(ind=0;ind<CODECS_MAX_OUTFMT;ind++) {
		if(sh->codec->outfmt[ind]==0xFFFFFFFF) break;
		MSG_WARN("'%s' ",vo_format_name(sh->codec->outfmt[ind]));
	    }
	    MSG_WARN("Trying -vf fmtcvt\n");
	    sc=vf=vf_open_filter(vf,sh,"fmtcvt",NULL);
	    goto csp_again;
	} else
	if(palette==1){
	    MSG_V("vd: Trying -vf palette...\n");
	    palette=-1;
	    vf=vf_open_filter(vf,sh,"palette",NULL);
	    goto csp_again;
	} else 
	{ // sws failed, if the last filter (vf_vo) support MPEGPES try to append vf_lavc
	     vf_instance_t* vo, *vp = NULL, *ve;
	     // Remove the scale filter if we added it ourself
	     if(vf == sc) {
	       ve = vf;
	       vf = vf->next;
	       vf_uninit_filter(ve);
	     }
	     // Find the last filter (vf_vo)
	     for(vo = vf ; vo->next ; vo = vo->next)
	       vp = vo;
	}
	MSG_WARN(MSGTR_VOincompCodec);
	sh->vfilter_inited=-1;
	return 0;	// failed
    }
    out_fmt=sh->codec->outfmt[j];
    sh->outfmtidx=j;
    sh->vfilter=vf;

    // autodetect flipping
    if(flip==-1){
	flip=0;
	if(sh->codec->outflags[j]&CODECS_FLAG_FLIP)
	    if(!(sh->codec->outflags[j]&CODECS_FLAG_NOFLIP))
		flip=1;
    }
    if(vo_flags&VFCAP_FLIPPED) flip^=1;
    if(flip && !(vo_flags&VFCAP_FLIP)){
	// we need to flip, but no flipping filter avail.
	sh->vfilter=vf=vf_open_filter(vf,sh,"mirror","x");
    }

    // time to do aspect ratio corrections...

  if(movie_aspect>-1.0) sh->aspect = movie_aspect; // cmdline overrides autodetect
  if(opt_screen_size_x||opt_screen_size_y){
    screen_size_x = opt_screen_size_x;
    screen_size_y = opt_screen_size_y;
    if(!vidmode){
     if(!screen_size_x) screen_size_x=SCREEN_SIZE_X;
     if(!screen_size_y) screen_size_y=SCREEN_SIZE_Y;
     if(screen_size_x<=8) screen_size_x*=sh->disp_w;
     if(screen_size_y<=8) screen_size_y*=sh->disp_h;
    }
  } else {
    // check source format aspect, calculate prescale ::atmos
    screen_size_x=sh->disp_w;
    screen_size_y=sh->disp_h;
    if(screen_size_xy>=0.001){
     if(screen_size_xy<=8){
       // -xy means x+y scale
       screen_size_x*=screen_size_xy;
       screen_size_y*=screen_size_xy;
     } else {
       // -xy means forced width while keeping correct aspect
       screen_size_x=screen_size_xy;
       screen_size_y=screen_size_xy*sh->disp_h/sh->disp_w;
     }
    }
    if(sh->aspect>0.01){
      int w;
      MSG_V("Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",
             sh->aspect);
      w=(int)((float)screen_size_y*sh->aspect); w+=w%2; // round
      // we don't like horizontal downscale || user forced width:
      if(w<screen_size_x || screen_size_xy>8){
        screen_size_y=(int)((float)screen_size_x*(1.0/sh->aspect));
        screen_size_y+=screen_size_y%2; // round
        if(screen_size_y<sh->disp_h) // Do not downscale verticaly
            screen_size_y=sh->disp_h;
      } else screen_size_x=w; // keep new width
    } else {
      MSG_V("Movie-Aspect is undefined - no prescaling applied.\n");
    }
  }

    MSG_V("video_out->init(%dx%d->%dx%d,flags=%d,'%s',%s)\n",
                      sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      "MPlayerXP",vo_format_name(out_fmt));

    MSG_DBG2("vf configuring: %s\n",vf->info->name);
    if(vf->config(vf,sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      out_fmt,tune)==0){
	MSG_WARN(MSGTR_CannotInitVO);
	sh->vfilter_inited=-1;
	return 0;
    }
    MSG_DBG2("vf->config(%dx%d->%dx%d,flags=%d,'%s',%p)\n",
                      sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      vo_format_name(out_fmt),tune);
    return 1;
}

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag,int w, int h){
  MSG_DBG2("mpcodecs_get_image(vf_%s,%i,%i,%i,%i) was called\n",((vf_instance_t *)(sh->vfilter))->info->name,mp_imgtype,mp_imgflag,w,h);
  mp_image_t* mpi=vf_get_image(sh->vfilter,sh->codec->outfmt[sh->outfmtidx],mp_imgtype,mp_imgflag,w,h);
  mpi->x=mpi->y=0;
  if(mpi->xp_idx==XP_IDX_INVALID)
    MSG_WARN("[mpcodecs_get_image] Incorrect mpi->xp_idx. Be ready for segfault!\n");
  return mpi;
}

void mpcodecs_draw_slice(sh_video_t *sh, mp_image_t*mpi) {
  struct vf_instance_s* vf = sh->vfilter;
  vf->put_slice(vf,mpi);
}
