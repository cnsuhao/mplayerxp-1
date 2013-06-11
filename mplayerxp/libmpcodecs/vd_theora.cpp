#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

#include <theora/theora.h>

#include "mpxp_help.h"
#include "codecs_ld.h"

#include "vd_internal.h"
#include "vd_msg.h"
#include "osdep/bswap.h"

namespace	usr {
    class theora_decoder : public Video_Decoder {
	public:
	    theora_decoder(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~theora_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    theora_state		st;
	    theora_comment		cc;
	    theora_info			inf;
	    video_decoder_t&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;
	    static const int THEORA_NUM_HEADER_PACKETS=3;
    };

video_probe_t theora_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "theora", "libtheora", FOURCC_TAG('T','H','E','O'), VCodecStatus_Problems, {IMGFMT_YV12,IMGFMT_422P,IMGFMT_444P}, {VideoFlag_None, VideoFlag_None } },
    { "theora", "libtheora", FOURCC_TAG('T','H','R','A'), VCodecStatus_Problems, {IMGFMT_YV12,IMGFMT_422P,IMGFMT_444P}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

theora_decoder::theora_decoder(video_decoder_t& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    int failed = 1;
    int errorCode = 0;
    ogg_packet op;
    float pts;

    /* check whether video output format is supported */
    switch(sh.codec->outfmt[sh.outfmtidx]) {
	case IMGFMT_YV12: /* well, this should work... */ break;
	default:
	    mpxp_v<<"Unsupported out_fmt: 0x"<<std::hex<<sh.codec->outfmt[sh.outfmtidx]<<std::endl;
	    throw bad_format_exception();
    }

    /* this is not a loop, just a context, from which we can break on error */
    do {
	theora_info_init(&inf);
	theora_comment_init(&cc);

	/* Read all header packets, pass them to theora_decode_header. */
	for (i = 0; i < THEORA_NUM_HEADER_PACKETS; i++) {
	    op.bytes = ds_get_packet_r (*sh.ds, &op.packet,pts);
	    op.b_o_s = 1;
	    if ( (errorCode = theora_decode_header (&inf, &cc, &op))) {
		mpxp_v<<"Broken Theora header; errorCode="<<errorCode<<"!"<<std::endl;
		break;
	    }
	}
	if (errorCode) break;

	/* now init codec */
	errorCode = theora_decode_init (&st, &inf);
	if (errorCode) {
	    mpxp_v<<"Theora decode init failed: "<<errorCode<<std::endl;
	    break;
	}
	failed = 0;
    } while (0);

    if (failed) throw bad_format_exception();

    if(sh.aspect==0.0 && inf.aspect_denominator!=0) {
	sh.aspect = (float)(inf.aspect_numerator * inf.frame_width)/
		(inf.aspect_denominator * inf.frame_height);
    }

    mpxp_ok<<"INFO: Theora video init ok!"<<std::endl;

    if(mpcodecs_config_vf(parent,sh.src_w,sh.src_h)!=MPXP_Ok) throw bad_format_exception();
}

/*
 * uninit driver
 */
theora_decoder::~theora_decoder() {
    theora_clear (&st);
}

// to set/get/query special features/parameters
MPXP_Rc theora_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12)
			return MPXP_True;
	    else	return MPXP_False;
	default: break;
    }
    return MPXP_Unknown;
}

/*
 * decode frame
 */
mp_image_t* theora_decoder::run(const enc_frame_t& frame)
{
    int errorCode = 0;
    ogg_packet op;
    yuv_buffer yuv;
    mp_image_t* mpi;

    bzero (&op, sizeof (op));
    op.bytes = frame.len;
    op.packet = reinterpret_cast<unsigned char*>(frame.data);
    op.granulepos = -1;

    errorCode = theora_decode_packetin (&st, &op);
    if (errorCode) {
	mpxp_err<<"Theora decode packetin failed: "<<errorCode<<std::endl;
	return NULL;
    }

    errorCode = theora_decode_YUVout (&st, &yuv);
    if (errorCode) {
	mpxp_err<<"Theora decode YUVout failed: "<<errorCode<<std::endl;
	return NULL;
    }

    mpi = mpcodecs_get_image(parent, MP_IMGTYPE_EXPORT, 0,sh.src_w, sh.src_h);

    mpi->planes[0]=yuv.y;
    mpi->stride[0]=yuv.y_stride;
    mpi->planes[1]=yuv.u;
    mpi->stride[1]=yuv.uv_stride;
    mpi->planes[2]=yuv.v;
    mpi->stride[2]=yuv.uv_stride;

    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(video_decoder_t& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) theora_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_theora_info = {
   "Theora/VP3 video decoder",
   "theora",
   "David Kuehling (www.theora.org)",
   "build-in",
   query_interface,
   options
};
} // namespace	usr