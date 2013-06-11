#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <algorithm>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#ifdef HAVE_GOMP
#include <omp.h>
#endif

#include "mpxp_help.h"

#include "osdep/bswap.h"

#include "vd_internal.h"
#include "codecs_ld.h"
#include "postproc/postprocess.h"
#include "postproc/vf.h"
#include "libvo2/video_out.h"
#include "osdep/bswap.h"

#include "libavcodec/avcodec.h"
#include "libavformat/riff.h"
#include "libvo2/video_out.h"

namespace	usr {
    class vlavc_decoder : public Video_Decoder {
	public:
	    vlavc_decoder(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~vlavc_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    MPXP_Rc			find_vdecoder();
	    static int			get_buffer(AVCodecContext *avctx, AVFrame *pic);
	    static void			release_buffer(struct AVCodecContext *avctx, AVFrame *pic);
	    static void			draw_slice(struct AVCodecContext *s,const AVFrame *src, int offset[4],int y, int type, int height);

	    video_decoder_t&		parent;
	    sh_video_t&			sh;
	    int				use_slices;
	    int				cap_slices;
	    int				use_dr1;
	    int				cap_dr1;
	    AVCodec*			lavc_codec;
	    AVCodecContext*		ctx;
	    AVFrame*			lavc_picture;
	    mp_image_t*			mpi;
	    unsigned long long		frame_number; /* total frame number since begin of movie */
	    int				b_age;
	    int				ip_age[2];
	    int				qp_stat[32];
//    double qp_sum;
//    double inv_qp_sum;
	    int				ip_count;
	    int				b_count;
	    int				vo_inited;
	    int				hello_printed;
	    video_probe_t*		probe;
	    put_slice_info_t&		psi;
	    pp_context*			ppContext;
    };

static int lavc_param_error_resilience=2;
static int lavc_param_error_concealment=3;
static int lavc_param_vstats=0;
static int lavc_param_idct_algo=0;
static int lavc_param_debug=0;
static int lavc_param_vismv=0;
static int lavc_param_skip_top=0;
static int lavc_param_skip_bottom=0;
static int lavc_param_lowres=0;
static char *lavc_param_lowres_str=NULL;
static char *lavc_param_skip_loop_filter_str = NULL;
static char *lavc_param_skip_idct_str = NULL;
static char *lavc_param_skip_frame_str = NULL;
static int lavc_param_threads=-1;
static char *lavc_avopt = NULL;

static int enable_ffslices=1;

static enum AVDiscard str2AVDiscard(char *str) {
    if (!str)					return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "none"   ) == 0)	return AVDISCARD_NONE;
    if (strcasecmp(str, "default") == 0)	return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "nonref" ) == 0)	return AVDISCARD_NONREF;
    if (strcasecmp(str, "bidir"  ) == 0)	return AVDISCARD_BIDIR;
    if (strcasecmp(str, "nonkey" ) == 0)	return AVDISCARD_NONKEY;
    if (strcasecmp(str, "all"    ) == 0)	return AVDISCARD_ALL;
    mpxp_dbg2<<"Unknown discard value "<<str<<std::endl;
    return AVDISCARD_DEFAULT;
}

/* stupid workaround for current version of lavc */
const __attribute((used)) uint8_t last_coeff_flag_offset_8x8[63] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8
};

/* to set/get/query special features/parameters */
MPXP_Rc vlavc_decoder::ctrl(int cmd,any_t* arg,long arg2){
    uint32_t out_fourcc;
    AVCodecContext* avctx = ctx;
    UNUSED(arg2);
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    *((unsigned*)arg)=PP_QUALITY_MAX;
	    return MPXP_Ok;
	case VDCTRL_SET_PP_LEVEL: {
	    int quality=*((int*)arg);
	    if(quality<0 || quality>PP_QUALITY_MAX) quality=PP_QUALITY_MAX;
	    return MPXP_Ok;
	}
	case VDCTRL_QUERY_FORMAT:
	{
	    uint32_t format =(*((int*)arg));
	    if(avctx->pix_fmt == -1 &&
		avctx->get_format &&
		avctx->codec->pix_fmts)
			avctx->pix_fmt = avctx->get_format(avctx, avctx->codec->pix_fmts);
	    mpxp_dbg2<<"[vd_lavc QUERY_FORMAT for ";
	    fourcc(mpxp_dbg2,format);
	    mpxp_dbg2<<"] pixfmt = "<<std::hex<<avctx->pix_fmt<<std::endl;
	    if(avctx->codec->pix_fmts) {
	    unsigned i;
		mpxp_dbg2<<"[vd_lavc]avctx->codec->pix_fmts:";
		for(i=0;;i++) { mpxp_dbg2<<" "<<std::hex<<avctx->codec->pix_fmts[i]; if(avctx->codec->pix_fmts[i]==-1) break; }
		mpxp_dbg2<<std::endl;
	    }
	    else
		mpxp_dbg2<<"[vd_lavc]avctx->codec->pix_fmts doesn't exist"<<std::endl;
	    out_fourcc = fourcc_from_pixfmt(avctx->pix_fmt);
	    if(out_fourcc==format) return MPXP_True;
	// possible conversions:
	    switch( format ){
		case IMGFMT_YV12:
		case IMGFMT_IYUV:
		case IMGFMT_I420:
		    // "converted" using pointer/stride modification
		    if(	avctx->pix_fmt==PIX_FMT_YUV420P || // u/v swap
			avctx->pix_fmt==PIX_FMT_YUV422P ||
			avctx->pix_fmt==PIX_FMT_YUVJ420P) return MPXP_True;// half stride
		    /* these codecs may return only: PIX_FMT_YUV422P, PIX_FMT_YUV444P, PIX_FMT_YUV420P*/
		    /* TODO: we must test pix_fmt after decoding first frame at least */
		    if(	avctx->codec_id == CODEC_ID_MPEG1VIDEO ||
			avctx->codec_id == CODEC_ID_MPEG2VIDEO) return MPXP_True;
		    break;
#ifdef HAVE_XVMC
		case IMGFMT_XVMC_IDCT_MPEG2:
		case IMGFMT_XVMC_MOCO_MPEG2:
		    if(avctx->pix_fmt==PIX_FMT_XVMC_MPEG2_IDCT) return MPXP_True;
#endif
	    }
	    return MPXP_False;
	}
	break;
	case VDCTRL_RESYNC_STREAM:
	    avcodec_flush_buffers(avctx);
	    return MPXP_True;
    }
    return MPXP_Unknown;
}

MPXP_Rc vlavc_decoder::find_vdecoder()
{
    unsigned i;
    unsigned char flag = CODECS_FLAG_NOFLIP;
    const char* what="AVCodecID";

    enum AVCodecID ff_id = ff_codec_get_id(ff_codec_bmp_tags,sh.fourcc);
    if (ff_id == AV_CODEC_ID_NONE) {
prn_err:
	mpxp_v<<"Cannot find "<<what<<" for '";
	fourcc(mpxp_v,sh.fourcc);
	mpxp_v<<"' fourcc! Try force -vc option"<<std::endl;
	return MPXP_False;
    }
    AVCodec *codec=avcodec_find_decoder(ff_id);
    if(!codec) { what="AVCodec"; goto prn_err; }
    probe=new(zeromem) video_probe_t;
    probe->driver="lavc";
    probe->codec_dll=mp_strdup(avcodec_get_name(ff_id));
    i=0;
    if(codec->pix_fmts)
    for(i=0;i<Video_MaxOutFmt;i++) {
	if(codec->pix_fmts[i]==-1) break;
	probe->pix_fmt[i]=avcodec_pix_fmt_to_codec_tag(codec->pix_fmts[i]);
	probe->flags[i]=video_flags_e(flag);
    }
    if(!i) { probe->pix_fmt[i]=IMGFMT_YV12; probe->flags[i]=video_flags_e(flag); }

    if(probe) {
	sh.codec=new(zeromem) struct codecs_st;
	strcpy(sh.codec->dll_name,probe->codec_dll);
	strcpy(sh.codec->driver_name,probe->driver);
	strcpy(sh.codec->codec_name,sh.codec->dll_name);
	memcpy(sh.codec->outfmt,probe->pix_fmt,sizeof(probe->pix_fmt));
    }
    return MPXP_Ok;
}

vlavc_decoder::vlavc_decoder(video_decoder_t& _parent,sh_video_t& _sh,put_slice_info_t& _psi,uint32_t _fourcc)
	    :Video_Decoder(_parent,_sh,_psi,_fourcc)
	    ,parent(_parent)
	    ,sh(_sh)
	    ,psi(_psi)
{

    unsigned avc_version=0;
    int pp_flags;

    if(mp_conf.npp_options) pp2_init();
    avcodec_register_all();

    frame_number=-2;
    if(!sh.codec) if(find_vdecoder()!=MPXP_Ok) {
	mpxp_v<<"Can't find lavc decoder"<<std::endl;
	throw bad_format_exception();
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh.codec->dll_name);
    if(!lavc_codec){
	MSG_V(MSGTR_MissingLAVCcodec,sh.codec->dll_name);
	throw bad_format_exception();
    }

    ctx = avcodec_alloc_context3(lavc_codec);
    lavc_picture = avcodec_alloc_frame();
    if(!(ctx && lavc_picture)) {
	MSG_ERR(MSGTR_OutOfMemory);
	throw std::bad_alloc();
    }

    ctx->width = sh.src_w;
    ctx->height= sh.src_h;
  //  ctx->error_recognition= lavc_param_error_resilience;
    ctx->error_concealment= lavc_param_error_concealment;
    ctx->debug= lavc_param_debug;
    ctx->codec_tag= sh.fourcc;
    ctx->stream_codec_tag=sh.video.fccHandler;
    ctx->idct_algo=0; /*auto*/
#if 0
    if (lavc_param_debug)
	av_log_set_level(AV_LOG_DEBUG);
#endif
    ctx->debug_mv= lavc_param_vismv;
    ctx->skip_top   = lavc_param_skip_top;
    ctx->skip_bottom= lavc_param_skip_bottom;
    if(lavc_param_lowres_str != NULL) {
	int lowres_w=0;
	sscanf(lavc_param_lowres_str, "%d,%d", &lavc_param_lowres, &lowres_w);
	if(lavc_param_lowres < 1 || lavc_param_lowres > 16 || (lowres_w > 0 && ctx->width < lowres_w))
	    lavc_param_lowres = 0;
	ctx->lowres = lavc_param_lowres;
    }
    ctx->skip_loop_filter = str2AVDiscard(lavc_param_skip_loop_filter_str);
    ctx->skip_idct = str2AVDiscard(lavc_param_skip_idct_str);
    ctx->skip_frame = str2AVDiscard(lavc_param_skip_frame_str);
    if(sh.bih)
	ctx->bits_per_coded_sample= sh.bih->biBitCount;
    mpxp_dbg2<<"libavcodec.size: "<<ctx->width<<" x "<<ctx->height<<std::endl;
    /* AVRn stores huffman table in AVI header */
    /* Pegasus MJPEG stores it also in AVI header, but it uses the common
       MJPG _fourcc :( */
    if (sh.bih && (sh.bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh.fourcc == mmioFOURCC('A','V','R','n') ||
	sh.fourcc == mmioFOURCC('M','J','P','G'))) {
//	ctx->flags |= CODEC_FLAG_EXTERN_HUFF;
	ctx->extradata_size = sh.bih->biSize-sizeof(BITMAPINFOHEADER);
	ctx->extradata = new uint8_t [ctx->extradata_size];
	memcpy(ctx->extradata, sh.bih+sizeof(BITMAPINFOHEADER),
	    ctx->extradata_size);
    }
    if(sh.fourcc == mmioFOURCC('R', 'V', '1', '0')
	|| sh.fourcc == mmioFOURCC('R', 'V', '1', '3')
	|| sh.fourcc == mmioFOURCC('R', 'V', '2', '0')
	|| sh.fourcc == mmioFOURCC('R', 'V', '3', '0')
	|| sh.fourcc == mmioFOURCC('R', 'V', '4', '0')) {
	    ctx->extradata_size= 8;
	    ctx->extradata = new uint8_t[ctx->extradata_size];
	    if(sh.bih->biSize!=sizeof(*sh.bih)+8){
		/* only 1 packet per frame & sub_id from _fourcc */
		((uint32_t*)ctx->extradata)[0] = 0;
		((uint32_t*)ctx->extradata)[1] =
		(sh.fourcc == mmioFOURCC('R', 'V', '1', '3')) ? 0x10003001 : 0x10000000;
	    } else {
		/* has extra slice header (demux_rm or rm->avi streamcopy) */
		unsigned int* extrahdr=(unsigned int*)(sh.bih+1);
		((uint32_t*)ctx->extradata)[0] = extrahdr[0];
		((uint32_t*)ctx->extradata)[1] = extrahdr[1];
	    }
	}
    if (sh.bih && (sh.bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh.fourcc == mmioFOURCC('M','4','S','2') ||
	 sh.fourcc == mmioFOURCC('M','P','4','S') ||
	 sh.fourcc == mmioFOURCC('H','F','Y','U') ||
	 sh.fourcc == mmioFOURCC('F','F','V','H') ||
	 sh.fourcc == mmioFOURCC('W','M','V','2') ||
	 sh.fourcc == mmioFOURCC('W','M','V','3') ||
	 sh.fourcc == mmioFOURCC('A','S','V','1') ||
	 sh.fourcc == mmioFOURCC('A','S','V','2') ||
	 sh.fourcc == mmioFOURCC('V','S','S','H') ||
	 sh.fourcc == mmioFOURCC('M','S','Z','H') ||
	 sh.fourcc == mmioFOURCC('Z','L','I','B') ||
	 sh.fourcc == mmioFOURCC('M','P','4','V') ||
	 sh.fourcc == mmioFOURCC('F','L','I','C') ||
	 sh.fourcc == mmioFOURCC('S','N','O','W') ||
	 sh.fourcc == mmioFOURCC('a','v','c','1') ||
	 sh.fourcc == mmioFOURCC('L','O','C','O') ||
	 sh.fourcc == mmioFOURCC('t','h','e','o')
	 )) {
	    ctx->extradata_size = sh.bih->biSize-sizeof(BITMAPINFOHEADER);
	    ctx->extradata = new uint8_t [ctx->extradata_size];
	    memcpy(ctx->extradata, sh.bih+1, ctx->extradata_size);
    }
    if (sh.ImageDesc &&
	 sh.fourcc == mmioFOURCC('S','V','Q','3')){
	    ctx->extradata_size = *(int*)sh.ImageDesc;
	    ctx->extradata = new uint8_t [ctx->extradata_size];
	    memcpy(ctx->extradata, ((int*)sh.ImageDesc)+1, ctx->extradata_size);
    }
    /* Pass palette to codec */
#if 0
    if (sh.bih && (sh.bih->biBitCount <= 8)) {
	ctx->palctrl = (AVPaletteControl*)mp_calloc(1,sizeof(AVPaletteControl));
	ctx->palctrl->palette_changed = 1;
	if (sh.bih->biSize-sizeof(BITMAPINFOHEADER))
	    /* Palette size in biSize */
	    memcpy(ctx->palctrl->palette, sh.bih+1,
		   std::min(sh.bih->biSize-sizeof(BITMAPINFOHEADER), AVPALETTE_SIZE));
	else
	    /* Palette size in biClrUsed */
	    memcpy(ctx->palctrl->palette, sh.bih+1,
		   std::min(sh.bih->biClrUsed * 4, AVPALETTE_SIZE));
	}
#endif
    if(sh.bih)
	ctx->bits_per_coded_sample= sh.bih->biBitCount;

#ifdef _OPENMP
    /* Note: Slices have effect on UNI-processor machines only */
    if(enable_ffslices && omp_get_num_procs()>1 && mp_conf.gomp) enable_ffslices=0;
#endif
    if(lavc_codec->capabilities&CODEC_CAP_DRAW_HORIZ_BAND && enable_ffslices) cap_slices=1;
/* enable DR1 method */
    if(lavc_codec->capabilities&CODEC_CAP_DR1) cap_dr1=1;
    ctx->flags|= CODEC_FLAG_EMU_EDGE;

    if(lavc_param_threads < 0) lavc_param_threads = get_number_cpu();
    if(lavc_param_threads > 1) {
	ctx->thread_count = lavc_param_threads;
	mpxp_v<<"Using "<<lavc_param_threads<<" threads in lavc"<<std::endl;
    }
    /* open it */
    if (avcodec_open2(ctx, lavc_codec, NULL) < 0) {
	MSG_ERR(MSGTR_CantOpenCodec);
	throw bad_format_exception();
    }
    mpxp_v<<"INFO: libavcodec.so ("<<std::hex<<avc_version<<") video codec[";
    fourcc(mpxp_v,sh.fourcc);
    mpxp_v<<"] init OK!"<<std::endl;
    if(mp_conf.npp_options) {
	pp_flags=0;
	switch(sh.codec->outfmt[sh.outfmtidx]) {
	    case IMGFMT_YV12:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:	pp_flags = PP_FORMAT_420;
				break;
	    case IMGFMT_YVYU:
	    case IMGFMT_YUY2:	pp_flags = PP_FORMAT_422;
				break;
	    case IMGFMT_411P:	pp_flags = PP_FORMAT_411;
				break;
	    default: {
		mpxp_warn<<"Can't apply postprocessing for";
		fourcc(mpxp_warn,sh.codec->outfmt[sh.outfmtidx]);
		mpxp_warn<<std::endl;
		break;
	    }
	}
	if(pp_flags) ppContext=pp2_get_context(sh.src_w,sh.src_h,pp_flags);
    }
    if(mpcodecs_config_vf(parent,sh.src_w,sh.src_h)!=MPXP_Ok) throw bad_format_exception();
}

// uninit driver
vlavc_decoder::~vlavc_decoder(){
    if(ppContext) pp_free_context(ppContext);
    ppContext=NULL;
    pp2_uninit();
    if(ctx) {
	if (avcodec_close(ctx) < 0)
	    MSG_ERR( MSGTR_CantCloseCodec);
	if (ctx->extradata_size)
	    delete ctx->extradata;
	delete ctx;
	delete lavc_picture;
    }
    if(probe) { delete probe->codec_dll; delete probe; }
}

video_probe_t vlavc_decoder::get_probe_information() const { return *probe; }

int vlavc_decoder::get_buffer(AVCodecContext *avctx, AVFrame *pic){
    vlavc_decoder& priv = *reinterpret_cast<vlavc_decoder*>(avctx->opaque);
    mp_image_t* mpi=NULL;
    int flags= MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE;
    int type= MP_IMGTYPE_IPB;
    int width= avctx->width;
    int height= avctx->height;
    int align=15;
//printf("get_buffer %d %d %d\n", pic->reference, ctx->ip_count, ctx->b_count);
    if(avctx->pix_fmt == PIX_FMT_YUV410P)
	align=63; //yes seriously, its really needed (16x16 chroma blocks in SVQ1 -> 64x64)

    if (pic->buffer_hints) {
	mpxp_dbg2<<"Buffer hints: "<<pic->buffer_hints<<std::endl;
	type = MP_IMGTYPE_TEMP;
	if (pic->buffer_hints & FF_BUFFER_HINTS_READABLE)
	    flags |= MP_IMGFLAG_READABLE;
	if (pic->buffer_hints & FF_BUFFER_HINTS_PRESERVE) {
	    type = MP_IMGTYPE_STATIC;
	    flags |= MP_IMGFLAG_PRESERVE;
	}
	if (pic->buffer_hints & FF_BUFFER_HINTS_REUSABLE) {
	    type = MP_IMGTYPE_STATIC;
	    flags |= MP_IMGFLAG_PRESERVE;
	}
	flags|=((avctx->skip_frame==AVDISCARD_NONE) && priv.use_slices) ?
		MP_IMGFLAG_DRAW_CALLBACK:0;
	mpxp_dbg2<<( type == MP_IMGTYPE_STATIC ? "using STATIC" : "using TEMP")<<std::endl;
    } else {
	if(!pic->reference){
	    priv.b_count++;
	    flags|=((avctx->skip_frame==AVDISCARD_NONE) && priv.use_slices) ?
		    MP_IMGFLAG_DRAW_CALLBACK:0;
	}else{
	    priv.ip_count++;
	    flags|= MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE
		    | (priv.use_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0);
	}
    }

    if (!pic->buffer_hints) {
	if(priv.b_count>1 || priv.ip_count>2){
	    mpxp_warn<<"DR1 failure"<<std::endl;
	    priv.use_dr1=0; //FIXME
	    avctx->get_buffer= avcodec_default_get_buffer;
	    return avctx->get_buffer(avctx, pic);
	}
	if(avctx->has_b_frames){
	    type= MP_IMGTYPE_IPB;
	}else{
	    type= MP_IMGTYPE_IP;
	}
	mpxp_dbg2<<( type== MP_IMGTYPE_IPB ? "using IPB" : "using IP")<<std::endl;
    }

    mpxp_v<<"ff width="<<width<<" height="<<height<<std::endl;
    mpi= mpcodecs_get_image(priv.parent,type, flags, (width+align)&(~align), (height+align)&(~align));
    if(mpi->flags & MP_IMGFLAG_DIRECT) mpi->flags |= MP_IMGFLAG_RENDERED;
    // Palette support: libavcodec copies palette to *data[1]
    if (mpi->bpp == 8) mpi->planes[1] = new unsigned char [AVPALETTE_SIZE];

    pic->data[0]= mpi->planes[0];
    pic->data[1]= mpi->planes[1];
    pic->data[2]= mpi->planes[2];

    /* Note, some (many) codecs in libavcodec must have stride1==stride2 && no changes between frames
     * lavc will check that and die with an error message, if its not true
     */
    pic->linesize[0]= mpi->stride[0];
    pic->linesize[1]= mpi->stride[1];
    pic->linesize[2]= mpi->stride[2];

    pic->opaque = mpi;

    if(pic->reference) {
//	 pic->age= ip_age[0];
	priv.ip_age[0]= priv.ip_age[1]+1;
	priv.ip_age[1]= 1;
	priv.b_age++;
    } else {
//	 pic->age= b_age;
	priv.ip_age[0]++;
	priv.ip_age[1]++;
	priv.b_age=1;
    }
    pic->type= FF_BUFFER_TYPE_USER;
    return 0;
}

void vlavc_decoder::release_buffer(struct AVCodecContext *avctx, AVFrame *pic){
    mp_image_t* mpi= reinterpret_cast<mp_image_t*>(pic->opaque);
    vlavc_decoder& priv = *reinterpret_cast<vlavc_decoder*>(avctx->opaque);
    int i;

    if(priv.ip_count <= 2 && priv.b_count<=1){
	if(mpi->flags&MP_IMGFLAG_PRESERVE)
	    priv.ip_count--;
	else
	    priv.b_count--;
    }

    if(mpi) {
	if(mpi->bpp == 8 && mpi->planes[1]) delete mpi->planes[1];
	if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK) free_mp_image(mpi);
    }

    if(pic->type!=FF_BUFFER_TYPE_USER){
	avcodec_default_release_buffer(avctx, pic);
	return;
    }

    for(i=0; i<4; i++){
	pic->data[i]= NULL;
    }
//printf("R%X %X\n", pic->linesize[0], pic->data[0]);
}

void vlavc_decoder::draw_slice(struct AVCodecContext* s,
				const AVFrame* src, int offset[4],
				int y, int type, int height)
{
    UNUSED(offset);
    UNUSED(type);
    vlavc_decoder& priv=*reinterpret_cast<vlavc_decoder*>(s->opaque);
    sh_video_t& sh=priv.sh;
    mp_image_t *mpi=priv.mpi;
    unsigned long long int total_frame;
    unsigned orig_idx = mpi->xp_idx;
    /* sync-point*/
    if(src->pict_type==AV_PICTURE_TYPE_I) priv.frame_number = src->coded_picture_number;
    total_frame = priv.frame_number;
    if(priv.use_dr1) { mpxp_dbg2<<"Ignoring draw_slice due dr1"<<std::endl; return; } /* we may call vo_start_slice() here */
//    mpi=mpcodecs_get_image(sh,MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_DRAW_CALLBACK|MP_IMGFLAG_DIRECT,s->width,s->height);

    mpi->stride[0]=src->linesize[0];
    mpi->stride[1]=src->linesize[1];
    mpi->stride[2]=src->linesize[2];
    mpi->planes[0] = src->data[0];
    mpi->planes[1] = src->data[1];
    mpi->planes[2] = src->data[2];
    mpi->w=s->width;
    mpi->y=y;
    mpi->h=height;
    mpi->chroma_height = height >> mpi->chroma_y_shift;
    /* provide info for pp */
    mpi->qscale=(char *)priv.lavc_picture->qscale_table;
    mpi->qstride=priv.lavc_picture->qstride;
    mpi->pict_type=priv.lavc_picture->pict_type;
    mpi->qscale_type=priv.lavc_picture->qscale_type;

    if(sh.codec->outfmt[sh.outfmtidx] == IMGFMT_I420 ||
       sh.codec->outfmt[sh.outfmtidx] == IMGFMT_IYUV)
    {
	uint8_t *tmp;
	unsigned ls;
	tmp=mpi->planes[2];
	mpi->planes[2]=mpi->planes[1];
	mpi->planes[1]=tmp;
	ls=mpi->stride[2];
	mpi->stride[2]=mpi->stride[1];
	mpi->stride[1]=ls;
    }
#if 0
    /* handle IPB-frames here */
    if(total_frame!=src->coded_picture_number) {
	unsigned long long int tf = total_frame;
	/* we can do only 1 step forward */
	if(total_frame<src->coded_picture_number)
	    mpi->xp_idx = vo_get_decoding_next_frame(mpi->xp_idx);
	else
	while(tf>src->coded_picture_number) {
	    mpi->xp_idx = vo_get_decoding_prev_frame(mpi->xp_idx);
	    tf--;
	}
    }
#endif
    mpxp_dbg2<<"ff_draw_callback<"<<orig_idx<<"->"<<mpi->xp_idx<<":"<<(unsigned)total_frame
	<<":"<<src->coded_picture_number
	<<"-"<<(src->pict_type==AV_PICTURE_TYPE_BI?"bi":
		src->pict_type==AV_PICTURE_TYPE_SP?"sp":
		src->pict_type==AV_PICTURE_TYPE_SI?"si":
		src->pict_type==AV_PICTURE_TYPE_S?"s":
		src->pict_type==AV_PICTURE_TYPE_B?"b":
		src->pict_type==AV_PICTURE_TYPE_P?"p":
		src->pict_type==AV_PICTURE_TYPE_I?"i":"??")
	    <<">["<<mpi->width<<"x"<<mpi->height<<"] "<<mpi->x<<" "
	    <<mpi->y<<" "<<mpi->w<<" "<<mpi->h<<std::endl;
    __MP_ATOMIC(priv.psi.active_slices++);
    mpcodecs_draw_slice (priv.parent, mpi);
    mpi->xp_idx = orig_idx;
    __MP_ATOMIC(priv.psi.active_slices--);
}

/* copypaste from demux_real.c - it should match to get it working!*/

typedef struct __attribute__((__packed__)) dp_hdr_s {
    uint32_t chunks;
    uint32_t timestamp;
    uint32_t len;
    uint32_t chunktab;
} dp_hdr_t;

// decode a frame
mp_image_t* vlavc_decoder::run(const enc_frame_t& frame){
    int got_picture=0;
    int ret,has_b_frames;
    unsigned len=frame.len;
    any_t* data=frame.data;
    mp_image_t* _mpi=NULL;

    ctx->opaque=this;
    if(frame.len<=0) return NULL; // skipped frame

    ctx->skip_frame=(frame.flags&3)?((frame.flags&2)?AVDISCARD_NONKEY:AVDISCARD_DEFAULT):AVDISCARD_NONE;
    if(cap_slices)	use_slices= !(psi.vf_flags&VF_FLAGS_SLICES)?0:(ctx->skip_frame!=AVDISCARD_NONE)?0:1;
    else			use_slices=0;
/*
    if codec is capable DR1
    if sh.vfilter==vf_vo2 (DR1 is meaningless into temp buffer)
    It always happens with (vidix+bus mastering), (if (src_w%16==0)) with xv
*/
    has_b_frames=ctx->has_b_frames||
		 sh.fourcc==0x10000001 || /* mpeg1 may have b frames */
		 lavc_codec->id==CODEC_ID_SVQ3||
		 1;
    _mpi= mpcodecs_get_image(parent,has_b_frames?MP_IMGTYPE_IPB:MP_IMGTYPE_IP,MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE|MP_IMGFLAG_READABLE|MP_IMGFLAG_PRESERVE,
			    16,16);
    if(cap_dr1 &&
       lavc_codec->id != CODEC_ID_H264 &&
       use_slices && _mpi->flags&MP_IMGFLAG_DIRECT)
		use_dr1=1;
    if(has_b_frames) {
	mpxp_v<<"Disable slice-based rendering in lavc due possible B-frames in video-stream"<<std::endl;
	use_slices=0;
    }
    if(use_slices) use_dr1=0;
    if(   sh.fourcc == mmioFOURCC('R', 'V', '1', '0')
       || sh.fourcc == mmioFOURCC('R', 'V', '1', '3')
       || sh.fourcc == mmioFOURCC('R', 'V', '2', '0')
       || sh.fourcc == mmioFOURCC('R', 'V', '3', '0')
       || sh.fourcc == mmioFOURCC('R', 'V', '4', '0'))
    if(sh.bih->biSize==sizeof(*sh.bih)+8){
	int i;
	const dp_hdr_t *hdr= (const dp_hdr_t*)data;

	if(ctx->slice_offset==NULL)
	    ctx->slice_offset= new int [1000];

//        for(i=0; i<25; i++) printf("%02X ", ((uint8_t*)data)[i]);

	ctx->slice_count= hdr->chunks+1;
	for(i=0; i<ctx->slice_count; i++)
	    ctx->slice_offset[i]= ((const uint32_t*)(data+hdr->chunktab))[2*i+1];
	len=hdr->len;
	data=reinterpret_cast<any_t*>(reinterpret_cast<long>(data)+sizeof(dp_hdr_t));
    }
    if(use_dr1){
	b_age= ip_age[0]= ip_age[1]= 256*256*256*64;
	ip_count= b_count= 0;
	ctx->get_buffer= get_buffer;
	ctx->release_buffer= release_buffer;
	ctx->reget_buffer= get_buffer;
    }
    if(!(frame.flags&3) && use_slices)
    {
	if(_mpi) free_mp_image(_mpi);
	_mpi=mpcodecs_get_image(parent, MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_DRAW_CALLBACK|MP_IMGFLAG_DIRECT,sh.src_w, sh.src_h);
	_mpi = _mpi;
	frame_number++;
	ctx->draw_horiz_band=draw_slice;
    }
    else ctx->draw_horiz_band=NULL; /* skip draw_slice on framedropping */
    if(!hello_printed) {
	if(use_slices)
	    mpxp_status<<"Use slice-based rendering in lavc"<<std::endl;
	else if (use_dr1)
	    mpxp_status<<"Use DR1 rendering in lavc"<<std::endl;
	else
	hello_printed=1;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data=reinterpret_cast<uint8_t*>(data);
    pkt.size=len;
    ret = avcodec_decode_video2(ctx, lavc_picture,
				&got_picture, &pkt);
    if(ret<0) mpxp_warn<<"Error while decoding frame!"<<std::endl;
    if(!got_picture) return NULL;	// skipped image
    if(!ctx->draw_horiz_band)
    {
	if(_mpi) free_mp_image(_mpi);
	_mpi=mpcodecs_get_image(parent, MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,sh.src_w,sh.src_h);
	if(!_mpi){	// temporary!
	    mpxp_err<<"couldn't allocate image for lavc codec"<<std::endl;
	    return NULL;
	}
	_mpi->planes[0]=lavc_picture->data[0];
	_mpi->planes[1]=lavc_picture->data[1];
	_mpi->planes[2]=lavc_picture->data[2];
	_mpi->stride[0]=lavc_picture->linesize[0];
	_mpi->stride[1]=lavc_picture->linesize[1];
	_mpi->stride[2]=lavc_picture->linesize[2];
	/* provide info for pp */
	_mpi->qscale=(char *)lavc_picture->qscale_table;
	_mpi->qstride=lavc_picture->qstride;
	_mpi->pict_type=lavc_picture->pict_type;
	_mpi->qscale_type=lavc_picture->qscale_type;
	/*
	if(sh.codec->outfmt[sh.outfmtidx] == IMGFMT_I420 ||
	   sh.codec->outfmt[sh.outfmtidx] == IMGFMT_IYUV)
	{
	    uint8_t *tmp;
	    unsigned ls;
	    tmp=_mpi->planes[2];
	    _mpi->planes[2]=_mpi->planes[1];
	    _mpi->planes[1]=tmp;
	    ls=_mpi->stride[2];
	    _mpi->stride[2]=_mpi->stride[1];
	    _mpi->stride[1]=ls;
	}*/
	if(ctx->pix_fmt==PIX_FMT_YUV422P){
	    _mpi->stride[1]*=2;
	    _mpi->stride[2]*=2;
	}
    } /* endif use_slices */
    return _mpi;
}

static const mpxp_option_t ff_options[] = {
    {"slices", &enable_ffslices, CONF_TYPE_FLAG, 0, 0, 1, "enables slice-based method of frame rendering in lavc decoder"},
    {"noslices", &enable_ffslices, CONF_TYPE_FLAG, 0, 1, 0, "disables slice-based method of frame rendering in lavc decoder"},
    {"er", &lavc_param_error_resilience, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies error resilience for lavc decoders"},
    {"idct", &lavc_param_idct_algo, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies idct algorithm for lavc decoders"},
    {"ec", &lavc_param_error_concealment, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies error concealment for lavc decoders"},
    {"vstats", &lavc_param_vstats, CONF_TYPE_FLAG, 0, 0, 1, "specifies vstat for lavc decoders"},
    {"debug", &lavc_param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 9999999, "specifies debug level for lavc decoders"},
    {"vismv", &lavc_param_vismv, CONF_TYPE_INT, CONF_RANGE, 0, 9999999, "specifies visualize motion vectors (MVs) for lavc decoders"},
    {"st", &lavc_param_skip_top, CONF_TYPE_INT, CONF_RANGE, 0, 999, "specifies skipping top lines for lavc decoders"},
    {"sb", &lavc_param_skip_bottom, CONF_TYPE_INT, CONF_RANGE, 0, 999, "specifies skipping bottom lines for lavc decoders"},
    {"lowres", &lavc_param_lowres_str, CONF_TYPE_STRING, 0, 0, 0, "specifies decoding at 1= 1/2, 2=1/4, 3=1/8 resolutions for lavc decoders"},
    {"skiploopfilter", &lavc_param_skip_loop_filter_str, CONF_TYPE_STRING, 0, 0, 0, "specifies skipping of loop filters for lavc decoders"},
    {"skipidct", &lavc_param_skip_idct_str, CONF_TYPE_STRING, 0, 0, 0, "specifies skipping of IDCT filters for lavc decoders"},
    {"skipframe", &lavc_param_skip_frame_str, CONF_TYPE_STRING, 0, 0, 0, "indicates frame skipping for lavc decoders"},
    {"threads", &lavc_param_threads, CONF_TYPE_INT, CONF_RANGE, 1, 8, "specifies number of threads for lavc decoders"},
    {"o", &lavc_avopt, CONF_TYPE_STRING, 0, 0, 0, "specifies additional option for lavc decoders"},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const mpxp_option_t options[] = {
    {"lavc", (any_t*)&ff_options, CONF_TYPE_SUBCONFIG, 0, 0, 0, "lavc specific options"},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(video_decoder_t& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) vlavc_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_lavc_info = {
    "lavc codec family",
    "lavc",
    "A'rpi",
    "build-in",
    query_interface,
    options
};
} // namespace	usr