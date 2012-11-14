#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "mp_config.h"
#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#ifdef HAVE_GOMP
#include <omp.h>
#endif

#include "help_mp.h"

#include "osdep/bswap.h"
#include "osdep/mplib.h"

#include "vd_internal.h"
#include "codecs_ld.h"
#include "postproc/postprocess.h"
#include "postproc/vf.h"
#include "libvo/video_out.h"
#include "osdep/bswap.h"

static const vd_info_t info = {
    "FFmpeg's libavcodec codec family",
    "ffmpeg",
    "A'rpi",
    "build-in"
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
static const config_t ff_options[] = {
    {"slices", &enable_ffslices, CONF_TYPE_FLAG, 0, 0, 1, "enables slice-based method of frame rendering in ffmpeg decoder"},
    {"noslices", &enable_ffslices, CONF_TYPE_FLAG, 0, 1, 0, "disables slice-based method of frame rendering in ffmpeg decoder"},
    {"er", &lavc_param_error_resilience, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies error resilience for ffmpeg decoders"},
    {"idct", &lavc_param_idct_algo, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies idct algorithm for ffmpeg decoders"},
    {"ec", &lavc_param_error_concealment, CONF_TYPE_INT, CONF_RANGE, 0, 99, "specifies error concealment for ffmpeg decoders"},
    {"vstats", &lavc_param_vstats, CONF_TYPE_FLAG, 0, 0, 1, "specifies vstat for ffmpeg decoders"},
    {"debug", &lavc_param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 9999999, "specifies debug level for ffmpeg decoders"},
    {"vismv", &lavc_param_vismv, CONF_TYPE_INT, CONF_RANGE, 0, 9999999, "specifies visualize motion vectors (MVs) for ffmpeg decoders"},
    {"st", &lavc_param_skip_top, CONF_TYPE_INT, CONF_RANGE, 0, 999, "specifies skipping top lines for ffmpeg decoders"},
    {"sb", &lavc_param_skip_bottom, CONF_TYPE_INT, CONF_RANGE, 0, 999, "specifies skipping bottom lines for ffmpeg decoders"},
    {"lowres", &lavc_param_lowres_str, CONF_TYPE_STRING, 0, 0, 0, "specifies decoding at 1= 1/2, 2=1/4, 3=1/8 resolutions for ffmpeg decoders"},
    {"skiploopfilter", &lavc_param_skip_loop_filter_str, CONF_TYPE_STRING, 0, 0, 0, "specifies skipping of loop filters for ffmpeg decoders"},
    {"skipidct", &lavc_param_skip_idct_str, CONF_TYPE_STRING, 0, 0, 0, "specifies skipping of IDCT filters for ffmpeg decoders"},
    {"skipframe", &lavc_param_skip_frame_str, CONF_TYPE_STRING, 0, 0, 0, "indicates frame skipping for ffmpeg decoders"},
    {"threads", &lavc_param_threads, CONF_TYPE_INT, CONF_RANGE, 1, 8, "specifies number of threads for ffmpeg decoders"},
    {"o", &lavc_avopt, CONF_TYPE_STRING, 0, 0, 0, "specifies additional option for ffmpeg decoders"},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t options[] = {
    {"ffmpeg", &ff_options, CONF_TYPE_SUBCONFIG, 0, 0, 0, "FFMPEG specific options"},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(ffmpeg)

#include "libavcodec/avcodec.h"
#include "libavformat/riff.h"
#include "libvo/video_out.h"


static int vcodec_inited=0;
typedef struct priv_s {
    int use_slices;
    int cap_slices;
    int use_dr1;
    int cap_dr1;
    AVCodec *lavc_codec;
    AVCodecContext *ctx;
    AVFrame *lavc_picture;
    mp_image_t* mpi;
    unsigned long long  frame_number; /* total frame number since begin of movie */
    int b_age;
    int ip_age[2];
    int qp_stat[32];
//    double qp_sum;
//    double inv_qp_sum;
    int ip_count;
    int b_count;
    int vo_inited;
    int hello_printed;
    video_probe_t* probe;
}priv_t;
static pp_context* ppContext=NULL;
static void draw_slice(struct AVCodecContext *s, const AVFrame *src, int offset[4], int y, int type, int height);

static enum AVDiscard str2AVDiscard(char *str) {
    if (!str)					return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "none"   ) == 0)	return AVDISCARD_NONE;
    if (strcasecmp(str, "default") == 0)	return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "nonref" ) == 0)	return AVDISCARD_NONREF;
    if (strcasecmp(str, "bidir"  ) == 0)	return AVDISCARD_BIDIR;
    if (strcasecmp(str, "nonkey" ) == 0)	return AVDISCARD_NONKEY;
    if (strcasecmp(str, "all"    ) == 0)	return AVDISCARD_ALL;
    MSG_ERR("Unknown discard value %s\n", str);
    return AVDISCARD_DEFAULT;
}

/* stupid workaround for current version of ffmpeg */
const __attribute((used)) uint8_t last_coeff_flag_offset_8x8[63] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8
};

/* to set/get/query special features/parameters */
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    priv_t *ctx = sh->context;
    uint32_t out_fourcc;
    AVCodecContext *avctx = ctx->ctx;
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
	    MSG_DBG2("[vd_ffmpeg QUERY_FORMAT for %c%c%c%c] pixfmt = %X\n"
		,((char *)&format)[0],((char *)&format)[1],((char *)&format)[2],((char *)&format)[3]
		,avctx->pix_fmt);
	    if(avctx->codec->pix_fmts) {
	    unsigned i;
		MSG_DBG2("[vd_ffmpeg]avctx->codec->pix_fmts:");
		for(i=0;;i++) { MSG_DBG2(" %X",avctx->codec->pix_fmts[i]); if(avctx->codec->pix_fmts[i]==-1) break; }
		MSG_DBG2("\n");
	    }
	    else
		MSG_DBG2("[vd_ffmpeg]avctx->codec->pix_fmts doesn't exist\n");
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

static const video_probe_t* __FASTCALL__ probe(sh_video_t *sh,uint32_t fcc) {
    unsigned i;
    unsigned char flag = CODECS_FLAG_NOFLIP;
    video_probe_t* vprobe = NULL;
    priv_t* priv=sh->context;
    const char* what="AVCodecID";
    if(!priv) priv=mp_mallocz(sizeof(priv_t));
    sh->context = priv;
    if(!vcodec_inited){
//	avcodec_init();
	avcodec_register_all();
	vcodec_inited=1;
    }
    enum AVCodecID ff_id = ff_codec_get_id(ff_codec_bmp_tags,fcc);
    if (ff_id == AV_CODEC_ID_NONE) {
	const char *fourcc;
	prn_err:
	fourcc=&fcc;
	MSG_ERR("Cannot find %s for '%c%c%c%c' fourcc! Try force -vc option\n"
		,what
		,fourcc[0],fourcc[1],fourcc[2],fourcc[3]);
	return NULL;
    }
    AVCodec *codec=avcodec_find_decoder(ff_id);
    if(!codec) { what="AVCodec"; goto prn_err; }
    vprobe=mp_mallocz(sizeof(video_probe_t));
    vprobe->driver="ffmpeg";
    vprobe->codec_dll=mp_strdup(avcodec_get_name(ff_id));
    if(codec->pix_fmts)
    for(i=0;i<CODECS_MAX_OUTFMT;i++) {
	if(codec->pix_fmts[i]==-1) break;
	vprobe->pix_fmt[i]=avcodec_pix_fmt_to_codec_tag(codec->pix_fmts[i]);
	vprobe->flags[i]=flag;
    }
    priv->probe=vprobe;
    return vprobe;
}

static MPXP_Rc find_vdecoder(sh_video_t* sh) {
    unsigned i;
    const video_probe_t* vprobe=probe(sh,sh->fourcc);
    if(vprobe) {
	sh->codec=mp_mallocz(sizeof(struct codecs_st));
	strcpy(sh->codec->dll_name,vprobe->codec_dll);
	strcpy(sh->codec->driver_name,vprobe->driver);
	strcpy(sh->codec->codec_name,sh->codec->dll_name);
	memcpy(sh->codec->outfmt,vprobe->pix_fmt,sizeof(vprobe->pix_fmt));
	return MPXP_Ok;
    }
    return MPXP_False;
}

extern unsigned xp_num_cpu;
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    unsigned avc_version=0;
    priv_t *priv = sh->context;
    int pp_flags;
    if(mp_conf.npp_options) pp2_init();
    if(!vcodec_inited){
//	avcodec_init();
	avcodec_register_all();
	vcodec_inited=1;
    }
    if(!priv) priv=mp_mallocz(sizeof(priv_t));
    sh->context = priv;
    priv->frame_number=-2;
    if(!sh->codec) if(find_vdecoder(sh)!=MPXP_False) {
	MSG_ERR("Can't find ffmpeg decoder\n");
	return MPXP_False;
    }
    priv->lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh->codec->dll_name);
    if(!priv->lavc_codec){
	MSG_ERR(MSGTR_MissingLAVCcodec,sh->codec->dll_name);
	return MPXP_False;
    }

    priv->ctx = avcodec_alloc_context3(priv->lavc_codec);
    priv->lavc_picture = avcodec_alloc_frame();
    if(!(priv->ctx && priv->lavc_picture)) {
	MSG_ERR(MSGTR_OutOfMemory);
	return MPXP_False;
    }

    priv->ctx->width = sh->src_w;
    priv->ctx->height= sh->src_h;
  //  priv->ctx->error_recognition= lavc_param_error_resilience;
    priv->ctx->error_concealment= lavc_param_error_concealment;
    priv->ctx->debug= lavc_param_debug;
    priv->ctx->codec_tag= sh->fourcc;
    priv->ctx->stream_codec_tag=sh->video.fccHandler;
    priv->ctx->idct_algo=0; /*auto*/
#if 0
    if (lavc_param_debug)
	av_log_set_level(AV_LOG_DEBUG);
#endif
    priv->ctx->debug_mv= lavc_param_vismv;
    priv->ctx->skip_top   = lavc_param_skip_top;
    priv->ctx->skip_bottom= lavc_param_skip_bottom;
    if(lavc_param_lowres_str != NULL) {
	int lowres_w=0;
	sscanf(lavc_param_lowres_str, "%d,%d", &lavc_param_lowres, &lowres_w);
	if(lavc_param_lowres < 1 || lavc_param_lowres > 16 || (lowres_w > 0 && priv->ctx->width < lowres_w))
	    lavc_param_lowres = 0;
	priv->ctx->lowres = lavc_param_lowres;
    }
    priv->ctx->skip_loop_filter = str2AVDiscard(lavc_param_skip_loop_filter_str);
    priv->ctx->skip_idct = str2AVDiscard(lavc_param_skip_idct_str);
    priv->ctx->skip_frame = str2AVDiscard(lavc_param_skip_frame_str);
    if(sh->bih)
	priv->ctx->bits_per_coded_sample= sh->bih->biBitCount;
    MSG_DBG2("libavcodec.size: %d x %d\n",priv->ctx->width,priv->ctx->height);
    /* AVRn stores huffman table in AVI header */
    /* Pegasus MJPEG stores it also in AVI header, but it uses the common
       MJPG fourcc :( */
    if (sh->bih && (sh->bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh->fourcc == mmioFOURCC('A','V','R','n') ||
	sh->fourcc == mmioFOURCC('M','J','P','G'))) {
//	priv->ctx->flags |= CODEC_FLAG_EXTERN_HUFF;
	priv->ctx->extradata_size = sh->bih->biSize-sizeof(BITMAPINFOHEADER);
	priv->ctx->extradata = mp_malloc(priv->ctx->extradata_size);
	memcpy(priv->ctx->extradata, sh->bih+sizeof(BITMAPINFOHEADER),
	    priv->ctx->extradata_size);
    }
    if(sh->fourcc == mmioFOURCC('R', 'V', '1', '0')
	|| sh->fourcc == mmioFOURCC('R', 'V', '1', '3')
	|| sh->fourcc == mmioFOURCC('R', 'V', '2', '0')
	|| sh->fourcc == mmioFOURCC('R', 'V', '3', '0')
	|| sh->fourcc == mmioFOURCC('R', 'V', '4', '0')) {
	    priv->ctx->extradata_size= 8;
	    priv->ctx->extradata = mp_malloc(priv->ctx->extradata_size);
	    if(sh->bih->biSize!=sizeof(*sh->bih)+8){
		/* only 1 packet per frame & sub_id from fourcc */
		((uint32_t*)priv->ctx->extradata)[0] = 0;
		((uint32_t*)priv->ctx->extradata)[1] =
		(sh->fourcc == mmioFOURCC('R', 'V', '1', '3')) ? 0x10003001 : 0x10000000;
	    } else {
		/* has extra slice header (demux_rm or rm->avi streamcopy) */
		unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
		((uint32_t*)priv->ctx->extradata)[0] = extrahdr[0];
		((uint32_t*)priv->ctx->extradata)[1] = extrahdr[1];
	    }
	}
    if (sh->bih && (sh->bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh->fourcc == mmioFOURCC('M','4','S','2') ||
	 sh->fourcc == mmioFOURCC('M','P','4','S') ||
	 sh->fourcc == mmioFOURCC('H','F','Y','U') ||
	 sh->fourcc == mmioFOURCC('F','F','V','H') ||
	 sh->fourcc == mmioFOURCC('W','M','V','2') ||
	 sh->fourcc == mmioFOURCC('W','M','V','3') ||
	 sh->fourcc == mmioFOURCC('A','S','V','1') ||
	 sh->fourcc == mmioFOURCC('A','S','V','2') ||
	 sh->fourcc == mmioFOURCC('V','S','S','H') ||
	 sh->fourcc == mmioFOURCC('M','S','Z','H') ||
	 sh->fourcc == mmioFOURCC('Z','L','I','B') ||
	 sh->fourcc == mmioFOURCC('M','P','4','V') ||
	 sh->fourcc == mmioFOURCC('F','L','I','C') ||
	 sh->fourcc == mmioFOURCC('S','N','O','W') ||
	 sh->fourcc == mmioFOURCC('a','v','c','1') ||
	 sh->fourcc == mmioFOURCC('L','O','C','O') ||
	 sh->fourcc == mmioFOURCC('t','h','e','o')
	 )) {
	    priv->ctx->extradata_size = sh->bih->biSize-sizeof(BITMAPINFOHEADER);
	    priv->ctx->extradata = mp_malloc(priv->ctx->extradata_size);
	    memcpy(priv->ctx->extradata, sh->bih+1, priv->ctx->extradata_size);
    }
    if (sh->ImageDesc &&
	 sh->fourcc == mmioFOURCC('S','V','Q','3')){
	    priv->ctx->extradata_size = *(int*)sh->ImageDesc;
	    priv->ctx->extradata = mp_malloc(priv->ctx->extradata_size);
	    memcpy(priv->ctx->extradata, ((int*)sh->ImageDesc)+1, priv->ctx->extradata_size);
    }
    /* Pass palette to codec */
#if 0
    if (sh->bih && (sh->bih->biBitCount <= 8)) {
	priv->ctx->palctrl = (AVPaletteControl*)mp_calloc(1,sizeof(AVPaletteControl));
	priv->ctx->palctrl->palette_changed = 1;
	if (sh->bih->biSize-sizeof(BITMAPINFOHEADER))
	    /* Palette size in biSize */
	    memcpy(priv->ctx->palctrl->palette, sh->bih+1,
		   min(sh->bih->biSize-sizeof(BITMAPINFOHEADER), AVPALETTE_SIZE));
	else
	    /* Palette size in biClrUsed */
	    memcpy(priv->ctx->palctrl->palette, sh->bih+1,
		   min(sh->bih->biClrUsed * 4, AVPALETTE_SIZE));
	}
#endif
    if(sh->bih)
	priv->ctx->bits_per_coded_sample= sh->bih->biBitCount;

#ifdef _OPENMP
    /* Note: Slices have effect on UNI-processor machines only */
    if(enable_ffslices && omp_get_num_procs()>1 && mp_conf.gomp) enable_ffslices=0;
#endif
    if(priv->lavc_codec->capabilities&CODEC_CAP_DRAW_HORIZ_BAND && enable_ffslices) priv->cap_slices=1;
/* enable DR1 method */
    if(priv->lavc_codec->capabilities&CODEC_CAP_DR1) priv->cap_dr1=1;
    priv->ctx->flags|= CODEC_FLAG_EMU_EDGE;

    if(lavc_param_threads < 0) lavc_param_threads = xp_num_cpu;
    if(lavc_param_threads > 1) {
	priv->ctx->thread_count = lavc_param_threads;
	MSG_STATUS("Using %i threads in FFMPEG\n",lavc_param_threads);
    }
    /* open it */
    if (avcodec_open2(priv->ctx, priv->lavc_codec, NULL) < 0) {
	MSG_ERR(MSGTR_CantOpenCodec);
	return 0;
    }
    MSG_V("INFO: libavcodec.so (%06X) video codec[%c%c%c%c] init OK!\n"
	,avc_version
	,((char *)&sh->fourcc)[0],((char *)&sh->fourcc)[1],((char *)&sh->fourcc)[2],((char *)&sh->fourcc)[3]);
    if(mp_conf.npp_options) {
	pp_flags=0;
	switch(sh->codec->outfmt[sh->outfmtidx]) {
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
		const char *fmt;
		fmt = (const char *)&sh->codec->outfmt[sh->outfmtidx];
		MSG_WARN("Can't apply postprocessing for");
		if(isprint(fmt[0]) && isprint(fmt[1]) && isprint(fmt[2]) && isprint(fmt[3]))
		    MSG_WARN(" '%c%c%c%c'!\n",fmt[0],fmt[1],fmt[2],fmt[3]);
		else MSG_ERR(" 0x%08X!\n",sh->codec->outfmt[sh->outfmtidx]);
		break;
	    }
	}
	if(pp_flags) ppContext=pp2_get_context(sh->src_w,sh->src_h,pp_flags);
    }
    return mpcodecs_config_vo(sh,sh->src_w,sh->src_h,libinput);
}

// uninit driver
static void uninit(sh_video_t *sh){
    priv_t *priv=sh->context;
    if (avcodec_close(priv->ctx) < 0)
	MSG_ERR( MSGTR_CantCloseCodec);
    if (priv->ctx->extradata_size)
	mp_free(priv->ctx->extradata);
    mp_free(priv->ctx);
    mp_free(priv->lavc_picture);
    if(priv->probe) { free(priv->probe->codec_dll); free(priv->probe); }
    mp_free(priv);
    if(ppContext) pp_free_context(ppContext);
    ppContext=NULL;
    pp2_uninit();
    vcodec_inited=0;
}

static int get_buffer(AVCodecContext *avctx, AVFrame *pic){
    sh_video_t * sh = avctx->opaque;
    priv_t *priv = sh->context;
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
	MSG_DBG2( "Buffer hints: %u\n", pic->buffer_hints);
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
	flags|=((avctx->skip_frame==AVDISCARD_NONE) && priv->use_slices) ?
		MP_IMGFLAG_DRAW_CALLBACK:0;
	MSG_DBG2( type == MP_IMGTYPE_STATIC ? "using STATIC\n" : "using TEMP\n");
    } else {
	if(!pic->reference){
	    priv->b_count++;
	    flags|=((avctx->skip_frame==AVDISCARD_NONE) && priv->use_slices) ?
		    MP_IMGFLAG_DRAW_CALLBACK:0;
	}else{
	    priv->ip_count++;
	    flags|= MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE
		    | (priv->use_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0);
	}
    }

    if (!pic->buffer_hints) {
	if(priv->b_count>1 || priv->ip_count>2){
	    MSG_WARN("DR1 failure\n");
	    priv->use_dr1=0; //FIXME
	    avctx->get_buffer= avcodec_default_get_buffer;
	    return avctx->get_buffer(avctx, pic);
	}
	if(avctx->has_b_frames){
	    type= MP_IMGTYPE_IPB;
	}else{
	    type= MP_IMGTYPE_IP;
	}
	MSG_DBG2( type== MP_IMGTYPE_IPB ? "using IPB\n" : "using IP\n");
    }

    MSG_V("ff width=%i height=%i\n",width,height);
    mpi= mpcodecs_get_image(sh,type, flags, (width+align)&(~align), (height+align)&(~align));
    if(mpi->flags & MP_IMGFLAG_DIRECT) mpi->flags |= MP_IMGFLAG_RENDERED;
    // Palette support: libavcodec copies palette to *data[1]
    if (mpi->bpp == 8) mpi->planes[1] = mp_malloc(AVPALETTE_SIZE);

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
//	 pic->age= priv->ip_age[0];
	priv->ip_age[0]= priv->ip_age[1]+1;
	priv->ip_age[1]= 1;
	priv->b_age++;
    } else {
//	 pic->age= priv->b_age;
	priv->ip_age[0]++;
	priv->ip_age[1]++;
	priv->b_age=1;
    }
    pic->type= FF_BUFFER_TYPE_USER;
    return 0;
}

static void release_buffer(struct AVCodecContext *avctx, AVFrame *pic){
    mp_image_t* mpi= pic->opaque;
    sh_video_t * sh = avctx->opaque;
    priv_t *priv = sh->context;
    int i;

    if(priv->ip_count <= 2 && priv->b_count<=1){
	if(mpi->flags&MP_IMGFLAG_PRESERVE)
	    priv->ip_count--;
	else
	    priv->b_count--;
    }

    if(mpi) {
	if(mpi->bpp == 8 && mpi->planes[1]) mp_free(mpi->planes[1]);
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


static void draw_slice(struct AVCodecContext *s,
			const AVFrame *src, int offset[4],
			int y, int type, int height)
{
    sh_video_t *sh=s->opaque;
    priv_t *priv=sh->context;
    mp_image_t *mpi=priv->mpi;
    unsigned long long int total_frame;
    unsigned orig_idx = mpi->xp_idx;
    /* sync-point*/
    if(src->pict_type==AV_PICTURE_TYPE_I) priv->frame_number = src->coded_picture_number;
    total_frame = priv->frame_number;
    if(priv->use_dr1) { MSG_DBG2("Ignoring draw_slice due dr1\n"); return; } /* we may call vo_start_slice() here */
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
    mpi->qscale=(QP_STORE_T *)priv->lavc_picture->qscale_table;
    mpi->qstride=priv->lavc_picture->qstride;
    mpi->pict_type=priv->lavc_picture->pict_type;
    mpi->qscale_type=priv->lavc_picture->qscale_type;

    if(sh->codec->outfmt[sh->outfmtidx] == IMGFMT_I420 ||
       sh->codec->outfmt[sh->outfmtidx] == IMGFMT_IYUV)
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
    MSG_DBG2("ff_draw_callback<%u->%u:%u:%u-%s>[%ux%u] %i %i %i %i\n",
    orig_idx,mpi->xp_idx,(unsigned)total_frame,src->coded_picture_number,
    src->pict_type==AV_PICTURE_TYPE_BI?"bi":
    src->pict_type==AV_PICTURE_TYPE_SP?"sp":
    src->pict_type==AV_PICTURE_TYPE_SI?"si":
    src->pict_type==AV_PICTURE_TYPE_S?"s":
    src->pict_type==AV_PICTURE_TYPE_B?"b":
    src->pict_type==AV_PICTURE_TYPE_P?"p":
    src->pict_type==AV_PICTURE_TYPE_I?"i":"??"
    ,mpi->width,mpi->height,mpi->x,mpi->y,mpi->w,mpi->h);
    __MP_ATOMIC(sh->active_slices++);
    mpcodecs_draw_slice (sh, mpi);
    mpi->xp_idx = orig_idx;
    __MP_ATOMIC(sh->active_slices--);
}

/* copypaste from demux_real.c - it should match to get it working!*/

typedef struct __attribute__((__packed__)) dp_hdr_s {
    uint32_t chunks;
    uint32_t timestamp;
    uint32_t len;
    uint32_t chunktab;
} dp_hdr_t;

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame){
    int got_picture=0;
    int ret,has_b_frames;
    unsigned len=frame->len;
    any_t* data=frame->data;
    priv_t *priv=sh->context;
    mp_image_t* mpi=NULL;

    priv->ctx->opaque=sh;
    if(frame->len<=0) return NULL; // skipped frame

    priv->ctx->skip_frame=(frame->flags&3)?((frame->flags&2)?AVDISCARD_NONKEY:AVDISCARD_DEFAULT):AVDISCARD_NONE;
    if(priv->cap_slices)	priv->use_slices= !(sh->vf_flags&VF_FLAGS_SLICES)?0:(priv->ctx->skip_frame!=AVDISCARD_NONE)?0:1;
    else			priv->use_slices=0;
/*
    if codec is capable DR1
    if sh->vfilter==vf_vo (DR1 is meaningless into temp buffer)
    It always happens with (vidix+bus mastering), (if (src_w%16==0)) with xv
*/
    has_b_frames=priv->ctx->has_b_frames||
		 sh->fourcc==0x10000001 || /* mpeg1 may have b frames */
		 priv->lavc_codec->id==CODEC_ID_SVQ3||
		 1;
    mpi= mpcodecs_get_image(sh,has_b_frames?MP_IMGTYPE_IPB:MP_IMGTYPE_IP,MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE|MP_IMGFLAG_READABLE|MP_IMGFLAG_PRESERVE,
			    16,16);
    if(priv->cap_dr1 &&
       priv->lavc_codec->id != CODEC_ID_H264 &&
       priv->use_slices && mpi->flags&MP_IMGFLAG_DIRECT)
		priv->use_dr1=1;
    if(has_b_frames) {
	MSG_V("Disable slice-based rendering in FFMPEG due possible B-frames in video-stream\n");
	priv->use_slices=0;
    }
    if(priv->use_slices) priv->use_dr1=0;
    if(   sh->fourcc == mmioFOURCC('R', 'V', '1', '0')
       || sh->fourcc == mmioFOURCC('R', 'V', '1', '3')
       || sh->fourcc == mmioFOURCC('R', 'V', '2', '0')
       || sh->fourcc == mmioFOURCC('R', 'V', '3', '0')
       || sh->fourcc == mmioFOURCC('R', 'V', '4', '0'))
    if(sh->bih->biSize==sizeof(*sh->bih)+8){
	int i;
	const dp_hdr_t *hdr= (const dp_hdr_t*)data;

	if(priv->ctx->slice_offset==NULL)
	    priv->ctx->slice_offset= mp_malloc(sizeof(int)*1000);

//        for(i=0; i<25; i++) printf("%02X ", ((uint8_t*)data)[i]);

	priv->ctx->slice_count= hdr->chunks+1;
	for(i=0; i<priv->ctx->slice_count; i++)
	    priv->ctx->slice_offset[i]= ((const uint32_t*)(data+hdr->chunktab))[2*i+1];
	len=hdr->len;
	data+= sizeof(dp_hdr_t);
    }
    if(priv->use_dr1){
	priv->b_age= priv->ip_age[0]= priv->ip_age[1]= 256*256*256*64;
	priv->ip_count= priv->b_count= 0;
	priv->ctx->get_buffer= get_buffer;
	priv->ctx->release_buffer= release_buffer;
	priv->ctx->reget_buffer= get_buffer;
    }
    if(!(frame->flags&3) && priv->use_slices)
    {
	if(mpi) free_mp_image(mpi);
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_DRAW_CALLBACK|MP_IMGFLAG_DIRECT,sh->src_w, sh->src_h);
	priv->mpi = mpi;
	priv->frame_number++;
	priv->ctx->draw_horiz_band=draw_slice;
    }
    else priv->ctx->draw_horiz_band=NULL; /* skip draw_slice on framedropping */
    if(!priv->hello_printed) {
	if(priv->use_slices)
	    MSG_STATUS("Use slice-based rendering in FFMPEG\n");
	else if (priv->use_dr1)
	    MSG_STATUS("Use DR1 rendering in FFMPEG\n");
	else
	priv->hello_printed=1;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data=data;
    pkt.size=len;
    ret = avcodec_decode_video2(priv->ctx, priv->lavc_picture,
				&got_picture, &pkt);
    if(ret<0) MSG_WARN("Error while decoding frame!\n");
    if(!got_picture) return NULL;	// skipped image
    if(!priv->ctx->draw_horiz_band)
    {
	if(mpi) free_mp_image(mpi);
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,sh->src_w,sh->src_h);
	if(!mpi){	// temporary!
	    MSG_ERR("couldn't allocate image for ffmpeg codec\n");
	    return NULL;
	}
	mpi->planes[0]=priv->lavc_picture->data[0];
	mpi->planes[1]=priv->lavc_picture->data[1];
	mpi->planes[2]=priv->lavc_picture->data[2];
	mpi->stride[0]=priv->lavc_picture->linesize[0];
	mpi->stride[1]=priv->lavc_picture->linesize[1];
	mpi->stride[2]=priv->lavc_picture->linesize[2];
	/* provide info for pp */
	mpi->qscale=(QP_STORE_T *)priv->lavc_picture->qscale_table;
	mpi->qstride=priv->lavc_picture->qstride;
	mpi->pict_type=priv->lavc_picture->pict_type;
	mpi->qscale_type=priv->lavc_picture->qscale_type;
	/*
	if(sh->codec->outfmt[sh->outfmtidx] == IMGFMT_I420 ||
	   sh->codec->outfmt[sh->outfmtidx] == IMGFMT_IYUV)
	{
	    uint8_t *tmp;
	    unsigned ls;
	    tmp=mpi->planes[2];
	    mpi->planes[2]=mpi->planes[1];
	    mpi->planes[1]=tmp;
	    ls=mpi->stride[2];
	    mpi->stride[2]=mpi->stride[1];
	    mpi->stride[1]=ls;
	}*/
	if(priv->ctx->pix_fmt==PIX_FMT_YUV422P){
	    mpi->stride[1]*=2;
	    mpi->stride[2]*=2;
	}
    } /* endif use_slices */
    return mpi;
}

