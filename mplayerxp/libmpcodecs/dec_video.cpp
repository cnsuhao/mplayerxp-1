#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#ifdef _OPENMP
#include <omp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mplayerxp.h"
#include "mpxp_help.h"

#include "osdep/timer.h"
#include "osdep/shmem.h"

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/parse_es.h"
#include "libmpdemux/stheader.h"
#include "xmpcore/sig_hand.h"

#include "libmpconf/codec-cfg.h"

#include "libvo2/video_out.h"
#include "postproc/vf.h"
#include "vd.h"

#include "xmpcore/xmp_core.h"
#include "dec_video.h"
#include "libmpsub/spudec.h"
#include "libmpsub/vobsub.h"

#include "mpxp_conf_lavc.h"
#include "osdep/cpudetect.h"
#include "vd_msg.h"

// ===================================================================
namespace usr	{
vf_cfg_t vf_cfg; // Configuration for audio filters

extern int v_bright;
extern int v_cont;
extern int v_hue;
extern int v_saturation;

struct decvideo_priv_t : public Opaque {
    public:
	decvideo_priv_t(libinput_t&);
	virtual ~decvideo_priv_t();

	sh_video_t*		parent;
	Video_Decoder*		mpvdec;
	libinput_t&		libinput;
	vf_stream_t*		vfilter;
	int			vfilter_inited;
	put_slice_info_t*	psi;
};

decvideo_priv_t::decvideo_priv_t(libinput_t& _libinput)
		:libinput(_libinput),
		psi(new(zeromem) put_slice_info_t)
{
}

decvideo_priv_t::~decvideo_priv_t() { delete psi; }

MPXP_Rc mpcv_get_quality_max(video_decoder_t& opaque,unsigned& quality){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    if(priv->mpvdec){
	MPXP_Rc ret=priv->mpvdec->ctrl(VDCTRL_QUERY_MAX_PP_LEVEL,&quality);
	if(ret>=MPXP_Ok) return ret;
    }
    return MPXP_False;
}

MPXP_Rc mpcv_set_quality(video_decoder_t& opaque,int quality){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    if(priv->mpvdec)
	return priv->mpvdec->ctrl(VDCTRL_SET_PP_LEVEL, (any_t*)(&quality));
    return MPXP_False;
}

MPXP_Rc mpcv_set_colors(video_decoder_t& opaque,const std::string& item,int value)
{
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    vf_stream_t* vs=priv->vfilter;
    vf_equalizer_t eq;
    eq.item=item.c_str();
    eq.value=value*10;
    if(vf_control(vs,VFCTRL_SET_EQUALIZER,&eq)!=MPXP_True) {
	if(priv->mpvdec) return priv->mpvdec->ctrl(VDCTRL_SET_EQUALIZER,(any_t*)item.c_str(),value);
    }
    return MPXP_False;
}

void mpcv_uninit(video_decoder_t& opaque){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    sh_video_t* sh_video = priv->parent;
    if(!sh_video->inited) { delete priv; delete &opaque; return; }
    mpxp_v<<"uninit video ..."<<std::endl;
    if(priv->vfilter && priv->vfilter_inited==1) vf_uninit(priv->vfilter);
    delete priv->mpvdec;
    sh_video->inited=0;
    delete priv;
    delete &opaque;
}

#include "libvo2/video_out.h"
#define MPDEC_THREAD_COND (VF_FLAGS_THREADS|VF_FLAGS_SLICES)
static unsigned smp_num_cpus=1;
static unsigned use_vf_threads=0;

static void mpcv_print_codec_info(decvideo_priv_t& priv) {
    sh_video_t* sh_video = priv.parent;
    mpxp_ok<<"[VC] "<<(mp_conf.video_codec?"Forcing":"Selected")<<" decoder: ["<<sh_video->codec->codec_name
	<<"] drv:"<<priv.mpvdec->get_probe_information().driver<<"."<<sh_video->codec->dll_name
	<<" ("<<sh_video->src_w<<"x"<<sh_video->src_h<<" (aspect "<<sh_video->aspect<<") "<<sh_video->fps<<"fps"<<std::endl;
    // Yeah! We got it!
    sh_video->inited=1;
    priv.psi->vf_flags=vf_query_flags(priv.vfilter);
#ifdef _OPENMP
    if(mp_conf.gomp) {
	smp_num_cpus=omp_get_num_procs();
	use_vf_threads=0;
	if(((priv.psi->vf_flags&MPDEC_THREAD_COND)==MPDEC_THREAD_COND) && (smp_num_cpus>1)) use_vf_threads=1;
	if(use_vf_threads)
	    mpxp_status<<"[mpdec] will perform parallel video-filter on "<<smp_num_cpus<<" CPUs"<<std::endl;
    }
#else
    mpxp_v<<"[mpdec] GOMP was not compiled-in! Using single threaded video filtering!"<<std::endl;
#endif
}

video_decoder_t* mpcv_init(sh_video_t *sh_video,const std::string& codecname,const std::string& family,int status,libinput_t&libinput){
    UNUSED(codecname);
    UNUSED(status);
    sh_video->codec=NULL;
    video_decoder_t* handle=new(zeromem) video_decoder_t;
    decvideo_priv_t* priv = new(zeromem) decvideo_priv_t(libinput);
    priv->parent=sh_video;
    handle->vd_private=priv;
    std::string vfm=family;

    MP_UNIT("init_video_filters");
    if(priv->vfilter_inited<=0) {
	vf_conf_t conf;
	conf.w=sh_video->src_w;
	conf.h=sh_video->src_h;
	conf.fourcc=sh_video->fourcc; // may be NULL ???
	priv->vfilter=vf_init(libinput,&conf);
	priv->vfilter_inited=1;
    }

    if(!vfm.empty()) {
	const vd_info_t* vi=vfm_find_driver(vfm);
	try{
	    priv->mpvdec=vi->query_interface(*handle,*sh_video,*priv->psi,sh_video->fourcc);
	} catch(const bad_format_exception&) {
	    MSG_ERR(MSGTR_CODEC_CANT_INITV);
	    delete priv->mpvdec;
	    goto bye;
	}
    }
    else priv->mpvdec = vfm_probe_driver(*handle,*sh_video,*priv->psi);
    if(!priv->mpvdec) {
bye:
	delete handle;
	delete priv;
	return NULL;
    }

    video_probe_t probe = priv->mpvdec->get_probe_information();
    vfm=probe.driver;
    /* fake struct codecs_st*/
    sh_video->codec=new(zeromem) struct codecs_st;
    strcpy(sh_video->codec->dll_name,probe.codec_dll);
    strcpy(sh_video->codec->driver_name,probe.driver);
    strcpy(sh_video->codec->codec_name,sh_video->codec->dll_name);
    memcpy(sh_video->codec->outfmt,probe.pix_fmt,sizeof(probe.pix_fmt));

    vf_showlist(priv->vfilter);

    mpcv_print_codec_info(*priv);
    return handle;
}

void mpcodecs_draw_image(video_decoder_t& opaque,mp_image_t *mpi)
{
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    vf_stream_t* s;
    const unsigned h_step=16;
    unsigned num_slices = mpi->h/h_step;
    s=priv->vfilter;

  if(!(mpi->flags&(MP_IMGFLAG_DRAW_CALLBACK))){
    if(mpi->h%h_step) num_slices++;
    if(priv->psi->vf_flags&VF_FLAGS_SLICES)
    {
	unsigned j,i,y;
	mp_image_t *ampi[num_slices];
	static int hello_printed=0;
	if(!hello_printed) {
		mpxp_ok<<"[VC] using "<<smp_num_cpus<<" threads for video filters"<<std::endl;
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
#pragma omp parallel for shared(s) private(i)
		for(i=j;i<smp_num_cpus;i++) {
		    mpxp_dbg2<<"parallel: dec_video.put_slice["<<ampi[i]->width<<"x"<<ampi[i]->height
			<<"] "<<ampi[i]->x<<" "<<ampi[i]->y<<" "<<ampi[i]->w<<" "<<ampi[i]->h<<std::endl;
		    vf_put_slice(s,ampi[i]);
		}
	    }
	    for(;j<num_slices;j++) {
		mpxp_dbg2<<"par_tail: dec_video.put_slice["<<ampi[i]->width<<"x"<<ampi[i]->height
		    <<"] "<<ampi[i]->x<<" "<<ampi[i]->y<<" "<<ampi[i]->w<<" "<<ampi[i]->h<<std::endl;
		vf_put_slice(s,ampi[j]);
	    }
	}
	else
#endif
	{
	    /* execute slices instead of whole frame make faster multiple filters */
	    for(i=0;i<num_slices;i++) {
		mpxp_dbg2<<"dec_video.put_slice["<<ampi[i]->width<<"x"<<ampi[i]->height
		    <<"] "<<ampi[i]->x<<" "<<ampi[i]->y<<" "<<ampi[i]->w<<" "<<ampi[i]->h<<std::endl;
		vf_put_slice(s,ampi[i]);
	    }
	}
	for(i=0;i<num_slices;i++) free_mp_image(ampi[i]);
    } else {
	mpxp_dbg2<<"Put whole frame["<<mpi->width<<"x"<<mpi->height<<"]"<<std::endl;
	vf_put_slice(s,mpi);
    }
    free_mp_image(mpi);
  }
}

static void update_subtitle(video_decoder_t& opaque,float v_pts,unsigned idx);
int mpcv_decode(video_decoder_t& opaque,const enc_frame_t& frame){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    vf_stream_t* s;
    mp_image_t *mpi=NULL;
    unsigned int t;
    unsigned int t2;
    double tt;

    s=priv->vfilter;

    t=GetTimer();
    vf_control(s,VFCTRL_START_FRAME,NULL);

    priv->psi->active_slices=0;
    mpi=priv->mpvdec->run(frame);
    mpxp_dbg2<<"decvideo: decoding video "<<frame.len<<" bytes"<<std::endl;
    while(priv->psi->active_slices!=0) yield_timeslice();
/* ------------------------ frame decoded. -------------------- */

    if(!mpi) return 0; // error / skipped frame
    mpcodecs_draw_image(opaque,mpi);

    t2=GetTimer();t=t2-t;
    tt = t*0.000001f;
    mpxp_context().bench->video+=tt;
    if(mp_conf.benchmark || mp_conf.frame_dropping) {
	if(tt > mpxp_context().bench->max_video) mpxp_context().bench->max_video=tt;
	if(tt < mpxp_context().bench->min_video) mpxp_context().bench->min_video=tt;
	mpxp_context().bench->cur_video=tt;
    }

    if(frame.flags) return 0;
    update_subtitle(opaque,frame.pts,mpi->xp_idx);
    mpxp_context().video().output->flush_page(dae_curr_vdecoded(mpxp_context().engine().xp_core));

    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    mpxp_context().bench->vout+=tt;
    if(mp_conf.benchmark || mp_conf.frame_dropping)
    {
	if(tt > mpxp_context().bench->max_vout) mpxp_context().bench->max_vout = tt;
	if(tt < mpxp_context().bench->min_vout) mpxp_context().bench->min_vout = tt;
	mpxp_context().bench->cur_vout=tt;
    }

    return 1;
}

void mpcv_resync_stream(video_decoder_t& opaque)
{
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    sh_video_t* sh_video = priv->parent;
    if(sh_video)
    if(sh_video->inited && priv->mpvdec) priv->mpvdec->ctrl(VDCTRL_RESYNC_STREAM,NULL);
}

#ifdef USE_SUB
static float sub_last_pts = -303;
#endif
static void update_subtitle(video_decoder_t& opaque,float v_pts,unsigned xp_idx)
{
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    sh_video_t* sh_video = priv->parent;
    Demuxer_Stream *d_dvdsub=sh_video->ds->demuxer->sub;
#ifdef USE_SUB
  // find sub
  if(mpxp_context().subtitles && v_pts>0){
      float pts=v_pts;
      if(mp_conf.sub_fps==0) mp_conf.sub_fps=sh_video->fps;
      MP_UNIT("find_sub");
      if (pts > sub_last_pts || pts < sub_last_pts-1.0 ) {
	 find_sub(mpxp_context().subtitles,sub_uses_time?(100*pts):(pts*mp_conf.sub_fps),mpxp_context().video().output); // FIXME! frame counter...
	 sub_last_pts = pts;
      }
      MP_UNIT(NULL);
  }
#endif

   if(mpxp_context().video().output->spudec){
    unsigned char* packet=NULL;
    int len,timestamp;
    MP_UNIT("spudec");
    spudec_now_pts(mpxp_context().video().output->spudec,90000*v_pts);
    if(spudec_visible(mpxp_context().video().output->spudec)) {
	mpxp_context().video().output->draw_spudec_direct(xp_idx);
    } else {
	spudec_heartbeat(mpxp_context().video().output->spudec,90000*v_pts);
	if (mpxp_context().video().output->vobsub) {
	    if (v_pts >= 0) {
		while((len=vobsub_get_packet(mpxp_context().video().output->vobsub, v_pts,(any_t**)&packet, &timestamp))>0){
		    timestamp -= v_pts*90000;
		    mpxp_v<<"VOB sub: len="<<len<<" v_pts="<<v_pts<<" sub="<<(timestamp/90000.0)<<" ts="<<timestamp<<std::endl;
		    spudec_assemble(mpxp_context().video().output->spudec,packet,len,90000*d_dvdsub->pts);
		}
	    }
	} else {
	    while((len=ds_get_packet_sub_r(*d_dvdsub,&packet))>0){
		mpxp_v<<"DVD sub: len="<<len<<" v_pts="<<v_pts<<" s_pts="<<d_dvdsub->pts<<std::endl;
		spudec_assemble(mpxp_context().video().output->spudec,packet,len,90000*d_dvdsub->pts);
	    }
	}
	/* detect wether the sub has changed or not */
	if(spudec_changed(mpxp_context().video().output->spudec)) mpxp_context().video().output->draw_spudec_direct(xp_idx);
	MP_UNIT(NULL);
    }
  }
}

#include "libvo2/video_out.h"

MPXP_Rc mpcodecs_config_vf(video_decoder_t& opaque, int w, int h){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    sh_video_t* sh = priv->parent;
    int i,j;
    unsigned int out_fmt=0;
    int screen_size_x=0;//SCREEN_SIZE_X;
    int screen_size_y=0;//SCREEN_SIZE_Y;
    vf_stream_t* s=priv->vfilter;
    vf_conf_t conf;
    int palette=0;

    if(!(sh->src_w && sh->src_h))
	mpxp_warn<<"VDec: driver "<<sh->codec->codec_name<<" didn't set sh->src_w and sh->src_h, trying to workaround!"<<std::endl;
    /* XXX: HACK, if sh->disp_* aren't set,
     * but we have w and h, set them :: atmos */
    if(!sh->src_w && w)
	sh->src_w=w;
    if(!sh->src_h && h)
	sh->src_h=h;

    mpxp_v<<"VDec: vo config request - "<<w<<" x "<<h<<std::endl;

    if(vf_config(s,sh->src_w,sh->src_h,
		sh->src_w,sh->src_h,
		mpxp_context().video().output->flags,
		out_fmt)==0){
		    MSG_WARN(MSGTR_CannotInitVO);
		    priv->vfilter_inited=-1;
		    return MPXP_False;
    }

csp_again:
    // check if libvo2 and codec has common outfmt (no conversion):
    j=-1;
    for(i=0;i<CODECS_MAX_OUTFMT;i++){
	int flags;
	out_fmt=sh->codec->outfmt[i];
	if(out_fmt==0xFFFFFFFF||out_fmt==0x0) continue;
	flags=vf_query_format(s,out_fmt,w,h);
	mpxp_dbg2<<"vo_debug[step i="<<i<<"]: query("<<vo_format_name(out_fmt)<<" "<<w<<"x"<<h<<") returned 0x"<<std::hex<<flags<<" for:"<<std::endl;
	if(mp_conf.verbose>1) vf_showlist(s);
	if((flags&VFCAP_CSP_SUPPORTED_BY_HW) || ((flags&VFCAP_CSP_SUPPORTED) && j<0)){
	    // check (query) if codec really support this outfmt...
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(priv->mpvdec->ctrl(VDCTRL_QUERY_FORMAT,&out_fmt)==MPXP_False) {
		mpxp_dbg2<<"vo_debug: codec["<<priv->mpvdec->get_probe_information().driver<<"] query_format("<<vo_format_name(out_fmt)<<") returned FALSE"<<std::endl;
		continue;
	    }
	    j=i;
	    /*mpxp_context().video().output->flags=flags;*/
	    if(flags&VFCAP_CSP_SUPPORTED_BY_HW) break;
	} else
	if(!palette && !(mpxp_context().video().output->flags&3) && (out_fmt==IMGFMT_RGB8||out_fmt==IMGFMT_BGR8)){
	    sh->outfmtidx=j; // pass index to the control() function this way
	    if(priv->mpvdec->ctrl(VDCTRL_QUERY_FORMAT,&out_fmt)!=MPXP_False)
		palette=1;
	}
    }
    if(j<0){
	// TODO: no match - we should use conversion...
	if(strcmp(vf_get_first_name(s),"fmtcvt") && palette!=1){
	    int ind;
	    mpxp_warn<<"Can't find colorspace for: ";
	    for(ind=0;ind<CODECS_MAX_OUTFMT;ind++) {
		if(sh->codec->outfmt[ind]==0xFFFFFFFF||
		    sh->codec->outfmt[ind]==0x0) break;
		mpxp_warn<<"'"<<vo_format_name(sh->codec->outfmt[ind])<<"' "<<std::endl;
	    }
	    mpxp_warn<<"Trying -vf fmtcvt..."<<std::endl;
	    conf.w=sh->src_w;
	    conf.h=sh->src_h;
	    conf.fourcc=sh->codec->outfmt[sh->outfmtidx];
	    vf_prepend_filter(s,"fmtcvt",&conf);
	    goto csp_again;
	} else
	if(palette==1){
	    mpxp_v<<"vd: Trying -vf palette..."<<std::endl;
	    palette=-1;
	    conf.w=sh->src_w;
	    conf.h=sh->src_h;
	    conf.fourcc=sh->codec->outfmt[sh->outfmtidx];
	    vf_prepend_filter(s,"palette",&conf);
	    goto csp_again;
	} else {
	// sws failed, if the last filter (vf_vo2) support MPEGPES try to append vf_lavc
	    // Remove the scale filter if we added it ourself
	    if(strcmp(vf_get_first_name(s),"fmtcvt")==0) vf_remove_first(s);
	}
	MSG_WARN(MSGTR_VOincompCodec);
	priv->vfilter_inited=-1;
	return MPXP_False;	// failed
    }

    out_fmt=sh->codec->outfmt[j];
    sh->outfmtidx=j;

    // autodetect flipping
    if(vo_conf.flip==0){
	mpxp_context().video().output->FLIP_UNSET();
	if(sh->codec->outflags[j]&CODECS_FLAG_FLIP)
	    if(!(sh->codec->outflags[j]&CODECS_FLAG_NOFLIP))
		mpxp_context().video().output->FLIP_SET();
    }
    if(mpxp_context().video().output->flags&VFCAP_FLIPPED) mpxp_context().video().output->FLIP_REVERT();
    if(mpxp_context().video().output->FLIP() && !(mpxp_context().video().output->flags&VFCAP_FLIP)){
	// we need to flip, but no flipping filter avail.
	conf.w=sh->src_w;
	conf.h=sh->src_h;
	conf.fourcc=out_fmt;
	vf_prepend_filter(s,"flip",&conf);
    }

    // time to do aspect ratio corrections...

    if(vo_conf.movie_aspect>-1.0) sh->aspect = vo_conf.movie_aspect; // cmdline overrides autodetect
    if(vo_conf.image_width||vo_conf.image_height){
	screen_size_x = vo_conf.image_width;
	screen_size_y = vo_conf.image_height;
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
	if(vo_conf.image_zoom>=0.001){
	    if(vo_conf.image_zoom<=8){
	    // -xy means x+y scale
		screen_size_x*=vo_conf.image_zoom;
		screen_size_y*=vo_conf.image_zoom;
	    } else {
	    // -xy means forced width while keeping correct aspect
		screen_size_x=vo_conf.image_zoom;
		screen_size_y=vo_conf.image_zoom*sh->src_h/sh->src_w;
	    }
	}
	if(sh->aspect>0.01){
	    int _w;
	    mpxp_v<<"Movie-Aspect is "<<sh->aspect<<":1 - prescaling to correct movie aspect."<<std::endl;
	    _w=(int)((float)screen_size_y*sh->aspect); _w+=_w%2; // round
	    // we don't like horizontal downscale || user forced width:
	    if(_w<screen_size_x || vo_conf.image_zoom>8){
		screen_size_y=(int)((float)screen_size_x*(1.0/sh->aspect));
		screen_size_y+=screen_size_y%2; // round
		if(unsigned(screen_size_y)<sh->src_h) // Do not downscale verticaly
		    screen_size_y=sh->src_h;
	    } else screen_size_x=_w; // keep new width
	} else {
	    mpxp_v<<"Movie-Aspect is undefined - no prescaling applied."<<std::endl;
	}
    }

    mpxp_v<<"vf->config("<<sh->src_w<<"x"<<sh->src_h<<"->"<<screen_size_x<<"x"<<screen_size_y<<",flags=0x"<<std::hex
	<<mpxp_context().video().output->flags<<",'"<<"MPlayerXP"<<"',"<<vo_format_name(out_fmt)<<std::endl;

    if(vf_config(s,sh->src_w,sh->src_h,
		screen_size_x,screen_size_y,
		mpxp_context().video().output->flags,
		out_fmt)==0){
		    MSG_WARN(MSGTR_CannotInitVO);
		    priv->vfilter_inited=-1;
		    return MPXP_False;
    }
    mpxp_dbg2<<"vf->config("<<sh->src_w<<"x"<<sh->src_h<<"->"<<screen_size_x<<"x"<<screen_size_y
	<<",flags="<<mpxp_context().video().output->flags<<",'"<<vo_format_name(out_fmt)<<"')"<<std::endl;
    return MPXP_True;
}

// mp_imgtype: buffering type, see xmp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see xmp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vf() later...
mp_image_t* mpcodecs_get_image(video_decoder_t& opaque, int mp_imgtype, int mp_imgflag,int w, int h){
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    sh_video_t* sh = priv->parent;
    vf_stream_t* s = priv->vfilter;
    mp_image_t* mpi=vf_get_new_image(s,sh->codec->outfmt[sh->outfmtidx],mp_imgtype,mp_imgflag,w,h,dae_curr_vdecoded(mpxp_context().engine().xp_core));
    mpi->x=mpi->y=0;
    if(mpi->xp_idx==XP_IDX_INVALID)
	mpxp_v<<"[mpcodecs_get_image] Incorrect mpi->xp_idx. Be ready for segfault!"<<std::endl;
    return mpi;
}

void mpcodecs_draw_slice(video_decoder_t& opaque, mp_image_t*mpi) {
    decvideo_priv_t* priv=reinterpret_cast<decvideo_priv_t*>(opaque.vd_private);
    vf_stream_t* vf = priv->vfilter;
    vf_put_slice(vf,mpi);
}
} // namespace	usr