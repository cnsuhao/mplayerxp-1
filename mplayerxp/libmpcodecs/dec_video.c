#include "mp_config.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <stdio.h>
#ifdef HAVE_MALLOC
#include <malloc.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include "../mplayer.h"
#include "help_mp.h"

#include "osdep/timer.h"
#include "osdep/shmem.h"

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "codec-cfg.h"

#include "libvo/video_out.h"
#include "postproc/vf.h"

#include "stheader.h"
#include "vd.h"

#include "dec_video.h"
// ===================================================================
vf_cfg_t vf_cfg; // Configuration for audio filters

#include "postproc/postprocess.h"
#include "cpudetect.h"
#include "vd_msg.h"

extern int v_bright;
extern int v_cont;
extern int v_hue;
extern int v_saturation;

int divx_quality=PP_QUALITY_MAX;

const vd_functions_t* mpvdec=NULL;

int get_video_quality_max(sh_video_t *sh_video){
  if(mpvdec){
    int ret=mpvdec->control(sh_video,VDCTRL_QUERY_MAX_PP_LEVEL,NULL);
    if(ret>=0) return ret;
  }
 return 0;
}

void set_video_quality(sh_video_t *sh_video,int quality){
  if(mpvdec)
    mpvdec->control(sh_video,VDCTRL_SET_PP_LEVEL, (void*)(&quality));
}

int set_video_colors(sh_video_t *sh_video,char *item,int value)
{
    vf_instance_t* vf=sh_video->vfilter;
    vf_equalizer_t eq;
    eq.item=item;
    eq.value=value*10;
    if(vf->control(vf,VFCTRL_SET_EQUALIZER,&eq)!=CONTROL_TRUE)
    {
	if(mpvdec) return mpvdec->control(sh_video,VDCTRL_SET_EQUALIZER,item,(int)value)==CONTROL_OK?1:0;
    }
    return 1;
}

void uninit_video(sh_video_t *sh_video){
    if(!sh_video->inited) return;
    MSG_V("uninit video: %s\n",sh_video->codec->driver_name);
    if(sh_video->vfilter && sh_video->vfilter_inited==1) vf_uninit_filter_chain(sh_video->vfilter);
    mpvdec->uninit(sh_video);
    sh_video->inited=0;
}

#define MPDEC_THREAD_COND (VF_FLAGS_THREADS|VF_FLAGS_SLICES)
static unsigned smp_num_cpus=1;
static unsigned use_vf_threads=0;
extern int enable_gomp;

extern char *video_codec;
int init_video(sh_video_t *sh_video,const char* codecname,const char * vfm,int status){
    unsigned o_bps,bpp;
    sh_video->codec=NULL;
    MSG_DBG3("init_video(%p, %s, %s, %i)\n",sh_video,codecname,vfm,status);
    while((sh_video->codec=find_codec(sh_video->format,
      sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,
      sh_video->codec,0) )){
	// ok we found one codec
	int i;
	if(sh_video->codec->flags&CODECS_FLAG_SELECTED) {
	    MSG_DBG3("init_video: %s already tried and failed\n",sh_video->codec->codec_name);
	    continue;
	}
	if(codecname && strcmp(sh_video->codec->codec_name,codecname)) {
	    MSG_DBG3("init_video: %s != %s [-vc]\n",sh_video->codec->codec_name,codecname);
	    continue;
	}
	if(vfm && strcmp(sh_video->codec->driver_name,vfm)!=0) {
	    MSG_DBG3("init_video: vfm doesn't match %s != %s\n",vfm,sh_video->codec->driver_name);
	    continue; // vfm doesn't match
	}
	if(sh_video->codec->status<status) {
	    MSG_DBG3("init_video: %s too unstable\n",sh_video->codec->codec_name);
	    continue;
	}
	sh_video->codec->flags|=CODECS_FLAG_SELECTED; // tagging it
	// ok, it matches all rules, let's find the driver!
	for (i=0; mpcodecs_vd_drivers[i] != NULL; i++)
	    if(strcmp(mpcodecs_vd_drivers[i]->info->driver_name,sh_video->codec->driver_name)==0) break;
	mpvdec=mpcodecs_vd_drivers[i];
	if(!mpvdec) {
	    MSG_DBG3("init_video: mpcodecs_vd_drivers[%s]->mpvdec==0\n",mpcodecs_vd_drivers[i]->info->driver_name);
	    continue;
	}
	// it's available, let's try to init!
	if(!mpvdec->init(sh_video)){
	    MSG_ERR(MSGTR_CODEC_CANT_INITV);
	    continue; // try next...
	}
	switch(sh_video->codec->outfmt[sh_video->outfmtidx])
	{
		case IMGFMT_RGB8:
		case IMGFMT_BGR8:
		case IMGFMT_Y800: bpp=8; break;
		case IMGFMT_YVU9:
		case IMGFMT_IF09: bpp = 9; break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV: bpp = 12; break;
		default:
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
		case IMGFMT_YUY2:
		case IMGFMT_YVYU:
		case IMGFMT_UYVY: bpp = 16; break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24: bpp = 24; break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32: bpp = 32; break;
	}
	o_bps=sh_video->fps*sh_video->disp_w*sh_video->disp_h*bpp/8;
	MSG_OK("[VC] %s decoder: [%s] drv:%s.%s (%dx%d (aspect %g) %4.2ffps ratio %i->%i\n"
	,video_codec?"Forcing":"Selected"
	,sh_video->codec->codec_name
	,mpvdec->info->driver_name
	,sh_video->codec->dll_name
	,sh_video->disp_w
	,sh_video->disp_h
	,sh_video->aspect
	,sh_video->fps
	,sh_video->i_bps
	,o_bps);
	// Yeah! We got it!
	sh_video->inited=1;
	sh_video->vf_flags=vf_query_flags(sh_video->vfilter);
#ifdef _OPENMP
	if(enable_gomp) {
	    smp_num_cpus=omp_get_num_procs();
	    use_vf_threads=0;
	    if(((sh_video->vf_flags&MPDEC_THREAD_COND)==MPDEC_THREAD_COND) && (smp_num_cpus>1)) use_vf_threads=1;
	    if(use_vf_threads)
		MSG_STATUS("[mpdec] will perform parallel video-filter on %u CPUs\n",smp_num_cpus);
	}
#else
	MSG_V("[mpdec] GOMP was not compiled-in! Using single threaded video filtering!\n");
#endif
	return 1;
    }
    return 0;
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
	mp_image_t ampi[num_slices];
	static int hello_printed=0;
	if(!hello_printed) {
		MSG_OK("[VC] using %u threads for video filters\n",smp_num_cpus);
		hello_printed=1;
	}
	y=0;
	for(i=0;i<num_slices;i++) {
	    mpi_fake_slice(&ampi[i],mpi,y,h_step);
	    y+=h_step;
	}
#ifdef _OPENMP
	if(use_vf_threads && (num_slices>smp_num_cpus)) {
	    for(j=0;j<num_slices;j+=smp_num_cpus) {
#pragma omp parallel for shared(vf) private(i)
		for(i=j;i<smp_num_cpus;i++) {
		    MSG_DBG2("parallel: dec_video.put_slice[%ux%u] %i %i %i %i\n",ampi[i].width,ampi[i].height,ampi[i].x,ampi[i].y,ampi[i].w,ampi[i].h);
		    vf->put_slice(vf,&ampi[i]);
		}
	    }
	    for(;j<num_slices;j++) {
		MSG_DBG2("par_tail: dec_video.put_slice[%ux%u] %i %i %i %i\n",ampi[i].width,ampi[i].height,ampi[i].x,ampi[i].y,ampi[i].w,ampi[i].h);
		vf->put_slice(vf,&ampi[j]);
	    }
	}
	else
#endif
	{
	    /* execute slices instead of whole frame make faster multiple filters */
	    for(i=0;i<num_slices;i++) {
		MSG_DBG2("dec_video.put_slice[%ux%u] %i %i %i %i\n",ampi[i].width,ampi[i].height,ampi[i].x,ampi[i].y,ampi[i].w,ampi[i].h);
		vf->put_slice(vf,&ampi[i]);
	    }
	}
    } else {
	MSG_DBG2("Put whole frame[%ux%u]\n",mpi->width,mpi->height);
	vf->put_slice(vf,mpi);
    }
  }
}

extern void update_subtitle(float v_pts);
int decode_video(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame, float pts){
  vf_instance_t* vf;
mp_image_t *mpi=NULL;
unsigned int t;
unsigned int t2;
double tt;

vf=sh_video->vfilter;

t=GetTimer();
vf->control(vf,VFCTRL_START_FRAME,NULL);

sh_video->active_slices=0;
mpi=mpvdec->decode(sh_video, start, in_size, drop_frame);
MSG_DBG2("decvideo: decoding video %u bytes\n",in_size);
while(sh_video->active_slices!=0) usleep(0);
/* ------------------------ frame decoded. -------------------- */

if(!mpi) return 0; // error / skipped frame
mpcodecs_draw_image(sh_video,mpi);

t2=GetTimer();t=t2-t;
tt = t*0.000001f;
video_time_usage+=tt;
if(benchmark || frame_dropping)
{
    if(tt > max_video_time_usage) max_video_time_usage=tt;
    if(tt < min_video_time_usage) min_video_time_usage=tt;
    cur_video_time_usage=tt;
}

if(drop_frame) return 0;
update_subtitle(pts);
vo_flush_pages();

    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    vout_time_usage+=tt;
    if(benchmark || frame_dropping)
    {
	if(tt > max_vout_time_usage) max_vout_time_usage = tt;
	if(tt < min_vout_time_usage) min_vout_time_usage = tt;
	cur_vout_time_usage=tt;
    }

  return 1;
}

void resync_video_stream(sh_video_t *sh_video)
{
  if(sh_video)
  if(sh_video->inited && mpvdec) mpvdec->control(sh_video,VDCTRL_RESYNC_STREAM,NULL);
}

