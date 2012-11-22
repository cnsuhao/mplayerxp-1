#include "mp_config.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mplayerxp.h"
#include "help_mp.h"

#include "osdep/timer.h"
#include "osdep/shmem.h"
#include "osdep/mplib.h"

#include "libmpstream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/parse_es.h"
#include "libmpdemux/stheader.h"
#include "xmpcore/sig_hand.h"

#include "libmpconf/codec-cfg.h"

#include "libvo/video_out.h"
#include "postproc/vf.h"
#include "vd.h"

#include "xmpcore/xmp_core.h"
#include "dec_video.h"
#include "libmpsub/spudec.h"
#include "libmpsub/vobsub.h"

#include "mp_conf_lavc.h"
#include "osdep/cpudetect.h"
#include "vd_msg.h"

using namespace mpxp;

// ===================================================================
vf_cfg_t vf_cfg; // Configuration for audio filters

extern int v_bright;
extern int v_cont;
extern int v_hue;
extern int v_saturation;

typedef struct priv_s {
    sh_video_t* parent;
    const vd_functions_t* mpvdec;
}priv_t;

MPXP_Rc mpcv_get_quality_max(any_t *opaque,unsigned *quality){
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    if(priv->mpvdec){
	MPXP_Rc ret=priv->mpvdec->control(sh_video,VDCTRL_QUERY_MAX_PP_LEVEL,quality);
	if(ret>=MPXP_Ok) return ret;
    }
    return MPXP_False;
}

MPXP_Rc mpcv_set_quality(any_t *opaque,int quality){
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    if(priv->mpvdec)
	return priv->mpvdec->control(sh_video,VDCTRL_SET_PP_LEVEL, (any_t*)(&quality));
    return MPXP_False;
}

MPXP_Rc mpcv_set_colors(any_t *opaque,const char *item,int value)
{
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    vf_instance_t* vf=sh_video->vfilter;
    vf_equalizer_t eq;
    eq.item=item;
    eq.value=value*10;
    if(vf->control(vf,VFCTRL_SET_EQUALIZER,&eq)!=MPXP_True) {
	if(priv->mpvdec) return priv->mpvdec->control(sh_video,VDCTRL_SET_EQUALIZER,(any_t*)item,(int)value);
    }
    return MPXP_False;
}

void mpcv_uninit(any_t *opaque){
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    if(!sh_video->inited) { delete priv; return; }
    MSG_V("uninit video ...\n");
    if(sh_video->vfilter && sh_video->vfilter_inited==1) vf_uninit_filter_chain(sh_video->vfilter);
    priv->mpvdec->uninit(sh_video);
    sh_video->inited=0;
    delete priv;
}

#include "libvo/video_out.h"
extern vo_data_t*vo_data;
#define MPDEC_THREAD_COND (VF_FLAGS_THREADS|VF_FLAGS_SLICES)
static unsigned smp_num_cpus=1;
static unsigned use_vf_threads=0;

static void mpcv_print_codec_info(const priv_t *priv) {
    sh_video_t* sh_video = priv->parent;
    MSG_OK("[VC] %s decoder: [%s] drv:%s.%s (%dx%d (aspect %g) %4.2ffps\n"
	,mp_conf.video_codec?"Forcing":"Selected"
	,sh_video->codec->codec_name
	,priv->mpvdec->info->driver_name
	,sh_video->codec->dll_name
	,sh_video->src_w
	,sh_video->src_h
	,sh_video->aspect
	,sh_video->fps);
    // Yeah! We got it!
    sh_video->inited=1;
    sh_video->vf_flags=vf_query_flags(sh_video->vfilter);
#ifdef _OPENMP
    if(mp_conf.gomp) {
	smp_num_cpus=omp_get_num_procs();
	use_vf_threads=0;
	if(((sh_video->vf_flags&MPDEC_THREAD_COND)==MPDEC_THREAD_COND) && (smp_num_cpus>1)) use_vf_threads=1;
	if(use_vf_threads)
	    MSG_STATUS("[mpdec] will perform parallel video-filter on %u CPUs\n",smp_num_cpus);
    }
#else
    MSG_V("[mpdec] GOMP was not compiled-in! Using single threaded video filtering!\n");
#endif
}

any_t * mpcv_ffmpeg_init(sh_video_t* sh_video,any_t* libinput) {
    priv_t* priv = new(zeromem) priv_t;
    priv->parent=sh_video;
    sh_video->decoder=priv;
    /* Use ffmpeg's drivers  as last hope */
    priv->mpvdec=vfm_find_driver("ffmpeg");
    if(priv->mpvdec) {
	if(priv->mpvdec->init(sh_video,libinput)!=MPXP_Ok){
	    MSG_ERR(MSGTR_CODEC_CANT_INITV);
	    return NULL;
	}
    } else {
	MSG_ERR("Cannot find ffmpeg video decoder\n");
	return NULL;
    }
    mpcv_print_codec_info(priv);
    return priv;
}

any_t * mpcv_init(sh_video_t *sh_video,const char* codecname,const char * vfm,int status,any_t*libinput){
    int done=0;
    const video_probe_t* vprobe;
    sh_video->codec=NULL;
    priv_t* priv = new(zeromem) priv_t;
    priv->parent=sh_video;
    sh_video->decoder=priv;
    if(vfm) {
	priv->mpvdec=vfm_find_driver(vfm);
	if(priv->mpvdec) vprobe=priv->mpvdec->probe(sh_video,sh_video->fourcc);
    }
    else vprobe = vfm_driver_probe(sh_video);
    if(vprobe) {
	vfm=vprobe->driver;
	/* fake struct codecs_st*/
	sh_video->codec=new(zeromem) struct codecs_st;
	strcpy(sh_video->codec->dll_name,vprobe->codec_dll);
	strcpy(sh_video->codec->driver_name,vprobe->driver);
	strcpy(sh_video->codec->codec_name,sh_video->codec->dll_name);
	memcpy(sh_video->codec->outfmt,vprobe->pix_fmt,sizeof(vprobe->pix_fmt));
	priv->mpvdec=vfm_find_driver(vfm);
    }
    if(sh_video->codec) {
	if(priv->mpvdec->init(sh_video,libinput)!=MPXP_Ok){
	    MSG_ERR(MSGTR_CODEC_CANT_INITV);
		delete sh_video->codec;
		sh_video->codec=NULL;
	} else done=1;
    }
#ifdef ENABLE_WIN32LOADER
    if(sh_video->codec) {
	done=0;
	MSG_DBG3("mpcv_init(%p, %s, %s, %i)\n",sh_video,codecname,vfm,status);
	while((sh_video->codec=find_codec(sh_video->fourcc,
		sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,
		sh_video->codec,0) )){
	    // ok we found one codec
	    if(sh_video->codec->flags&CODECS_FLAG_SELECTED) {
		MSG_DBG3("mpcv_init: %s already tried and failed\n",sh_video->codec->codec_name);
		continue;
	    }
	    if(codecname && strcmp(sh_video->codec->codec_name,codecname)) {
		MSG_DBG3("mpcv_init: %s != %s [-vc]\n",sh_video->codec->codec_name,codecname);
		continue;
	    }
	    if(vfm && strcmp(sh_video->codec->driver_name,vfm)!=0) {
		MSG_DBG3("mpcv_init: vfm doesn't match %s != %s\n",vfm,sh_video->codec->driver_name);
		continue; // vfm doesn't match
	    }
	    if(sh_video->codec->status<status) {
		MSG_DBG3("mpcv_init: %s too unstable\n",sh_video->codec->codec_name);
		continue;
	    }
	    sh_video->codec->flags|=CODECS_FLAG_SELECTED; // tagging it
	    // ok, it matches all rules, let's find the driver!
	    if(!(priv->mpvdec=vfm_find_driver(sh_video->codec->driver_name))) continue;
	    else    MSG_DBG3("mpcv_init: mpcodecs_vd_drivers[%s]->mpvdec==0\n",priv->mpvdec->info->driver_name);
	    // it's available, let's try to init!
	    if(priv->mpvdec->init(sh_video,libinput)!=MPXP_Ok){
		MSG_ERR(MSGTR_CODEC_CANT_INITV);
		continue; // try next...
	    }
	    done=1;
	    break;
	}
    }
#endif
    if(done) {
	mpcv_print_codec_info(priv);
// memory leak here
//	if(vprobe) { delete sh_video->codec; sh_video->codec=NULL; }
	return priv;
    }
    return NULL;
}

void mpcodecs_draw_image(sh_video_t* sh,mp_image_t *mpi)
{
  vf_instance_t* vf;
  const unsigned h_step=16;
  unsigned num_slices = mpi->h/h_step;
  vf=sh->vfilter;

  if(!(mpi->flags&(MP_IMGFLAG_DRAW_CALLBACK))){
    if(mpi->h%h_step) num_slices++;
    if(sh->vf_flags&VF_FLAGS_SLICES)
    {
	unsigned j,i,y;
	mp_image_t *ampi[num_slices];
	static int hello_printed=0;
	if(!hello_printed) {
		MSG_OK("[VC] using %u threads for video filters\n",smp_num_cpus);
		hello_printed=1;
	}
	y=0;
	for(i=0;i<num_slices;i++) {
	    ampi[i]=new_mp_image(mpi->w,y,h_step);
	    mpi_fake_slice(ampi[i],mpi,y,h_step);
	    y+=h_step;
	}
#ifdef _OPENMP
	if(use_vf_threads && (num_slices>smp_num_cpus)) {
	    for(j=0;j<num_slices;j+=smp_num_cpus) {
#pragma omp parallel for shared(vf) private(i)
		for(i=j;i<smp_num_cpus;i++) {
		    MSG_DBG2("parallel: dec_video.put_slice[%ux%u] %i %i %i %i\n",ampi[i]->width,ampi[i]->height,ampi[i]->x,ampi[i]->y,ampi[i]->w,ampi[i]->h);
		    vf->put_slice(vf,ampi[i]);
		    free_mp_image(ampi[i]);
		}
	    }
	    for(;j<num_slices;j++) {
		MSG_DBG2("par_tail: dec_video.put_slice[%ux%u] %i %i %i %i\n",ampi[i]->width,ampi[i]->height,ampi[i]->x,ampi[i]->y,ampi[i]->w,ampi[i]->h);
		vf->put_slice(vf,ampi[j]);
		free_mp_image(ampi[j]);
	    }
	}
	else
#endif
	{
	    /* execute slices instead of whole frame make faster multiple filters */
	    for(i=0;i<num_slices;i++) {
		MSG_DBG2("dec_video.put_slice[%ux%u] %i %i %i %i -> [%i]\n",ampi[i]->width,ampi[i]->height,ampi[i]->x,ampi[i]->y,ampi[i]->w,ampi[i]->h,ampi[i]->xp_idx);
		vf->put_slice(vf,ampi[i]);
		free_mp_image(ampi[i]);
	    }
	}
    } else {
	MSG_DBG2("Put whole frame[%ux%u]\n",mpi->width,mpi->height);
	vf->put_slice(vf,mpi);
    }
    free_mp_image(mpi);
  }
}

extern vo_data_t* vo_data;
static void update_subtitle(sh_video_t *sh_video,float v_pts,unsigned idx);
int mpcv_decode(any_t *opaque,const enc_frame_t* frame){
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    vf_instance_t* vf;
    mp_image_t *mpi=NULL;
    unsigned int t;
    unsigned int t2;
    double tt;

    vf=sh_video->vfilter;

    t=GetTimer();
    vf->control(vf,VFCTRL_START_FRAME,NULL);

    sh_video->active_slices=0;
    mpi=priv->mpvdec->decode(sh_video, frame);
    MSG_DBG2("decvideo: decoding video %u bytes\n",frame->len);
    while(sh_video->active_slices!=0) usleep(0);
/* ------------------------ frame decoded. -------------------- */

    if(!mpi) return 0; // error / skipped frame
    mpcodecs_draw_image(sh_video,mpi);

    t2=GetTimer();t=t2-t;
    tt = t*0.000001f;
    MPXPCtx->bench->video+=tt;
    if(mp_conf.benchmark || mp_conf.frame_dropping) {
	if(tt > MPXPCtx->bench->max_video) MPXPCtx->bench->max_video=tt;
	if(tt < MPXPCtx->bench->min_video) MPXPCtx->bench->min_video=tt;
	MPXPCtx->bench->cur_video=tt;
    }

    if(frame->flags) return 0;
    update_subtitle(sh_video,frame->pts,mpi->xp_idx);
    vo_flush_page(vo_data,dae_curr_vdecoded(xp_core));

    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    MPXPCtx->bench->vout+=tt;
    if(mp_conf.benchmark || mp_conf.frame_dropping)
    {
	if(tt > MPXPCtx->bench->max_vout) MPXPCtx->bench->max_vout = tt;
	if(tt < MPXPCtx->bench->min_vout) MPXPCtx->bench->min_vout = tt;
	MPXPCtx->bench->cur_vout=tt;
    }

    return 1;
}

void mpcv_resync_stream(any_t *opaque)
{
    priv_t* priv=(priv_t*)opaque;
    sh_video_t* sh_video = priv->parent;
    if(sh_video)
    if(sh_video->inited && priv->mpvdec) priv->mpvdec->control(sh_video,VDCTRL_RESYNC_STREAM,NULL);
}

#ifdef USE_SUB
static float sub_last_pts = -303;
#endif
static void update_subtitle(sh_video_t *sh_video,float v_pts,unsigned xp_idx)
{
    demux_stream_t *d_dvdsub=sh_video->ds->demuxer->sub;
#ifdef USE_SUB
  // find sub
  if(MPXPCtx->subtitles && v_pts>0){
      float pts=v_pts;
      if(mp_conf.sub_fps==0) mp_conf.sub_fps=sh_video->fps;
      MP_UNIT("find_sub");
      if (pts > sub_last_pts || pts < sub_last_pts-1.0 ) {
	 find_sub(MPXPCtx->subtitles,sub_uses_time?(100*pts):(pts*mp_conf.sub_fps),vo_data); // FIXME! frame counter...
	 sub_last_pts = pts;
      }
      MP_UNIT(NULL);
  }
#endif

  // DVD sub:
#if 0
  if(vo_flags & 0x08){
    static vo_mpegpes_t packet;
    static vo_mpegpes_t *pkg=&packet;
    packet.timestamp=v_pts*90000.0;
    packet.id=0x20; /* Subpic */
    while((packet.size=ds_get_packet_sub_r(d_dvdsub,&packet.data))>0){
      MSG_V("\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",packet.size,v_pts,d_dvdsub->pts);
      vo_draw_frame(&pkg);
    }
  }else
#endif
   if(vo_data->spudec){
    unsigned char* packet=NULL;
    int len,timestamp;
    MP_UNIT("spudec");
    spudec_now_pts(vo_data->spudec,90000*v_pts);
    if(spudec_visible(vo_data->spudec)) {
	vo_draw_spudec_direct(vo_data,xp_idx);
    } else {
	spudec_heartbeat(vo_data->spudec,90000*v_pts);
	if (vo_data->vobsub) {
	    if (v_pts >= 0) {
		while((len=vobsub_get_packet(vo_data->vobsub, v_pts,(any_t**)&packet, &timestamp))>0){
		    timestamp -= v_pts*90000;
		    MSG_V("\rVOB sub: len=%d v_pts=%5.3f sub=%5.3f ts=%d \n",len,v_pts,timestamp / 90000.0,timestamp);
		    spudec_assemble(vo_data->spudec,packet,len,90000*d_dvdsub->pts);
		}
	    }
	} else {
	    while((len=ds_get_packet_sub_r(d_dvdsub,&packet))>0){
		MSG_V("\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",len,v_pts,d_dvdsub->pts);
		spudec_assemble(vo_data->spudec,packet,len,90000*d_dvdsub->pts);
	    }
	}
	/* detect wether the sub has changed or not */
	if(spudec_changed(vo_data->spudec)) vo_draw_spudec_direct(vo_data,xp_idx);
	MP_UNIT(NULL);
    }
  }
}

#include "libvo/video_out.h"

extern vo_data_t* vo_data;
extern const vd_functions_t* mpvdec; // FIXME!

MPXP_Rc mpcodecs_config_vo(sh_video_t *sh, int w, int h, any_t* libinput){
    priv_t* priv=(priv_t*)sh->decoder;
    int i,j;
    unsigned int out_fmt=0;
    int screen_size_x=0;//SCREEN_SIZE_X;
    int screen_size_y=0;//SCREEN_SIZE_Y;
    vf_instance_t* vf=sh->vfilter,*sc=NULL;
    int palette=0;

    if(!(sh->src_w && sh->src_h))
	MSG_WARN(
	    "VDec: driver %s didn't set sh->src_w and sh->src_h, trying to workaround!\n"
	    ,sh->codec->codec_name);
    /* XXX: HACK, if sh->disp_* aren't set,
     * but we have w and h, set them :: atmos */
    if(!sh->src_w && w)
	sh->src_w=w;
    if(!sh->src_h && h)
	sh->src_h=h;

    MSG_V("VDec: vo config request - %d x %d\n",w,h);

csp_again:
    // check if libvo and codec has common outfmt (no conversion):
    j=-1;
    for(i=0;i<CODECS_MAX_OUTFMT;i++){
	int flags;
	out_fmt=sh->codec->outfmt[i];
	if(out_fmt==0xFFFFFFFF||out_fmt==0x0) continue;
	flags=vf_query_format(vf,out_fmt,w,h);
	MSG_DBG2("vo_debug[step i=%d]: query(%s %ix%i) returned 0x%X for:\n",i,vo_format_name(out_fmt),w,h,flags);
	if(mp_conf.verbose>1) if(mp_conf.verbose) vf_showlist(vf);
	if((flags&VFCAP_CSP_SUPPORTED_BY_HW) || ((flags&VFCAP_CSP_SUPPORTED) && j<0)){
	    // check (query) if codec really support this outfmt...
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(priv->mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)==MPXP_False) {
		MSG_DBG2("vo_debug: codec[%s] query_format(%s) returned FALSE\n",mpvdec->info->driver_name,vo_format_name(out_fmt));
		continue;
	    }
	    j=i;
	    /*vo_data->flags=flags;*/
	    if(flags&VFCAP_CSP_SUPPORTED_BY_HW) break;
	} else
	if(!palette && !(vo_data->flags&3) && (out_fmt==IMGFMT_RGB8||out_fmt==IMGFMT_BGR8)){
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(priv->mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)!=MPXP_False)
		palette=1;
	}
    }
    if(j<0){
	// TODO: no match - we should use conversion...
	if(strcmp(vf->info->name,"fmtcvt") && palette!=1){
	    int ind;
	    MSG_WARN("Can't find colorspace for: ");
	    for(ind=0;ind<CODECS_MAX_OUTFMT;ind++) {
		if(sh->codec->outfmt[ind]==0xFFFFFFFF||
		    sh->codec->outfmt[ind]==0x0) break;
		MSG_WARN("'%s' ",vo_format_name(sh->codec->outfmt[ind]));
	    }
	    MSG_WARN("Trying -vf fmtcvt\n");
	    sc=vf=vf_open_filter(vf,sh,"fmtcvt",NULL,libinput);
	    goto csp_again;
	} else
	if(palette==1){
	    MSG_V("vd: Trying -vf palette...\n");
	    palette=-1;
	    vf=vf_open_filter(vf,sh,"palette",NULL,libinput);
	    goto csp_again;
	} else {
	// sws failed, if the last filter (vf_vo) support MPEGPES try to append vf_lavc
	     vf_instance_t* voi, *vp = NULL, *ve;
	     // Remove the scale filter if we added it ourself
	     if(vf == sc) {
	       ve = vf;
	       vf = vf->next;
	       vf_uninit_filter(ve);
	     }
	     // Find the last filter (vf_vo)
	     for(voi = vf ; voi->next ; voi = voi->next)
	       vp = voi;
	}
	MSG_WARN(MSGTR_VOincompCodec);
	sh->vfilter_inited=-1;
	return MPXP_False;	// failed
    }

    out_fmt=sh->codec->outfmt[j];
    sh->outfmtidx=j;
    sh->vfilter=vf;

    // autodetect flipping
    if(vo_conf.flip==0){
	vo_FLIP_UNSET(vo_data);
	if(sh->codec->outflags[j]&CODECS_FLAG_FLIP)
	    if(!(sh->codec->outflags[j]&CODECS_FLAG_NOFLIP))
		vo_FLIP_SET(vo_data);
    }
    if(vo_data->flags&VFCAP_FLIPPED) vo_FLIP_REVERT(vo_data);
    if(vo_FLIP(vo_data) && !(vo_data->flags&VFCAP_FLIP)){
	// we need to flip, but no flipping filter avail.
	sh->vfilter=vf=vf_open_filter(vf,sh,"flip",NULL,libinput);
    }

    // time to do aspect ratio corrections...

    if(vo_conf.movie_aspect>-1.0) sh->aspect = vo_conf.movie_aspect; // cmdline overrides autodetect
    if(vo_conf.opt_screen_size_x||vo_conf.opt_screen_size_y){
	screen_size_x = vo_conf.opt_screen_size_x;
	screen_size_y = vo_conf.opt_screen_size_y;
	if(!vo_conf.vidmode){
	    if(!screen_size_x) screen_size_x=1;
	    if(!screen_size_y) screen_size_y=1;
	    if(screen_size_x<=8) screen_size_x*=sh->src_w;
	    if(screen_size_y<=8) screen_size_y*=sh->src_h;
	}
    } else {
	// check source format aspect, calculate prescale ::atmos
	screen_size_x=sh->src_w;
	screen_size_y=sh->src_h;
	if(vo_conf.screen_size_xy>=0.001){
	    if(vo_conf.screen_size_xy<=8){
	    // -xy means x+y scale
		screen_size_x*=vo_conf.screen_size_xy;
		screen_size_y*=vo_conf.screen_size_xy;
	    } else {
	    // -xy means forced width while keeping correct aspect
		screen_size_x=vo_conf.screen_size_xy;
		screen_size_y=vo_conf.screen_size_xy*sh->src_h/sh->src_w;
	    }
	}
	if(sh->aspect>0.01){
	    int _w;
	    MSG_V("Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",sh->aspect);
	    _w=(int)((float)screen_size_y*sh->aspect); _w+=_w%2; // round
	    // we don't like horizontal downscale || user forced width:
	    if(_w<screen_size_x || vo_conf.screen_size_xy>8){
		screen_size_y=(int)((float)screen_size_x*(1.0/sh->aspect));
		screen_size_y+=screen_size_y%2; // round
		if(screen_size_y<sh->src_h) // Do not downscale verticaly
		    screen_size_y=sh->src_h;
	    } else screen_size_x=_w; // keep new width
	} else {
	    MSG_V("Movie-Aspect is undefined - no prescaling applied.\n");
	}
    }

    MSG_V("vf->config(%dx%d->%dx%d,flags=0x%x,'%s',%s)\n",
	sh->src_w,sh->src_h,
	screen_size_x,screen_size_y,
	vo_data->flags,
	"MPlayerXP",vo_format_name(out_fmt));

    MSG_DBG2("vf configuring: %s\n",vf->info->name);
    if(vf->config(vf,sh->src_w,sh->src_h,
		screen_size_x,screen_size_y,
		vo_data->flags,
		out_fmt)==0){
		    MSG_WARN(MSGTR_CannotInitVO);
		    sh->vfilter_inited=-1;
		    return MPXP_False;
    }
    MSG_DBG2("vf->config(%dx%d->%dx%d,flags=%d,'%s')\n",
	sh->src_w,sh->src_h,
	screen_size_x,screen_size_y,
	vo_data->flags,
	vo_format_name(out_fmt));
    return MPXP_True;
}

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag,int w, int h){
    MSG_DBG2("mpcodecs_get_image(vf_%s,%i,%i,%i,%i) was called\n",((vf_instance_t *)(sh->vfilter))->info->name,mp_imgtype,mp_imgflag,w,h);
    mp_image_t* mpi=vf_get_new_image(sh->vfilter,sh->codec->outfmt[sh->outfmtidx],mp_imgtype,mp_imgflag,w,h,dae_curr_vdecoded(xp_core));
    mpi->x=mpi->y=0;
    if(mpi->xp_idx==XP_IDX_INVALID)
	MSG_V("[mpcodecs_get_image] Incorrect mpi->xp_idx. Be ready for segfault!\n");
    return mpi;
}

void mpcodecs_draw_slice(sh_video_t *sh, mp_image_t*mpi) {
    struct vf_instance_s* vf = sh->vfilter;
    vf->put_slice(vf,mpi);
}
