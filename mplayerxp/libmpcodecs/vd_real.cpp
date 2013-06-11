#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <dlfcn.h>

#include "mpxp_help.h"
#include "codecs_ld.h"
#include "vd_msg.h"
#include "osdep/bswap.h"

#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
/* copypaste from demux_real.c - it should match to get it working! */
    struct dp_hdr_t {
	uint32_t chunks;	// number of chunks
	uint32_t timestamp; // timestamp from packet header
	uint32_t len;	// length of actual data
	uint32_t chunktab;	// offset to chunk offset array
    };

/*
 * Structures for data packets.  These used to be tables of unsigned ints, but
 * that does not work on 64 bit platforms (e.g. Alpha).  The entries that are
 * pointers get truncated.  Pointers on 64 bit platforms are 8 byte longs.
 * So we have to use structures so the compiler will assign the proper space
 * for the pointer.
 */
    struct cmsg_data_t {
	uint32_t data1;
	uint32_t data2;
	uint32_t* dimensions;
    };

    struct transform_in_t {
	uint32_t len;
	uint32_t unknown1;
	uint32_t chunks;
	const uint32_t* extra;
	uint32_t unknown2;
	uint32_t timestamp;
    };

/* we need exact positions */
    struct rv_init_t {
	short unk1;
	short w;
	short h;
	short unk3;
	int unk2;
	int subformat;
	int unk5;
	int format;
    };

    class vreal_decoder : public Video_Decoder {
	public:
	    vreal_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~vreal_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    int				load_lib(const std::string& path);

	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;
	    any_t*			handle;
	    any_t*			rv_handle;
	    uint32_t		(*rvyuv_custom_message)(cmsg_data_t*,any_t*);
	    uint32_t		(*rvyuv_free)(any_t*);
	    uint32_t		(*rvyuv_hive_message)(uint32_t,uint32_t);
	    uint32_t		(*rvyuv_init)(any_t*,any_t*);
	    uint32_t		(*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,any_t*);
    };

video_probe_t vreal_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "realvideo", "drv2.so.6.0", FOURCC_TAG('R','V','2','0'), VCodecStatus_Problems, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','3','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','4','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

/* exits program when failure */
int vreal_decoder::load_lib(const std::string& path) {
    char *error;

    rv_handle = ::dlopen (path.c_str(), RTLD_LAZY);
    if (!handle) {
	mpxp_v<<"DLError: "<<::dlerror()<<std::endl;
	return 0;
    }

    rvyuv_custom_message = (uint32_t (*)(cmsg_data_t*,any_t*))ld_sym(rv_handle, "RV20toYUV420CustomMessage");
    if ((error = ::dlerror()) != NULL)  {
	mpxp_v<<"ld_sym(rvyuvCustomMessage): "<<error<<std::endl;
	return 0;
    }
    rvyuv_free = (uint32_t (*)(any_t*))ld_sym(rv_handle, "RV20toYUV420Free");
    if ((error = ::dlerror()) != NULL)  {
	mpxp_v<<"ld_sym(rvyuvFree): "<<error<<std::endl;
	return 0;
    }
    rvyuv_hive_message = (uint32_t (*)(uint32_t,uint32_t))ld_sym(rv_handle, "RV20toYUV420HiveMessage");
    if ((error = ::dlerror()) != NULL)  {
	mpxp_v<<"ld_sym(rvyuvHiveMessage): "<<error<<std::endl;
	return 0;
    }
    rvyuv_init = (uint32_t (*)(any_t*,any_t*))ld_sym(rv_handle, "RV20toYUV420Init");
    if ((error = ::dlerror()) != NULL)  {
	mpxp_v<<"ld_sym(rvyuvInit): "<<error<<std::endl;
	return 0;
    }
    rvyuv_transform = (uint32_t (*)(char*,char*,transform_in_t*,unsigned int*,any_t*))ld_sym(rv_handle, "RV20toYUV420Transform");
    if ((error = ::dlerror()) != NULL)  {
	mpxp_v<<"ld_sym(rvyuvTransform): "<<error<<std::endl;
	return 0;
    }
    return 1;
}

vreal_decoder::vreal_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    if(!load_lib(probe->codec_dll)) throw bad_format_exception();

    //unsigned int out_fmt;
    int result;
    // we export codec id and sub-id from demuxer in bitmapinfohdr:
    unsigned int* extrahdr=(unsigned int*)(sh.bih+1);
    struct rv_init_t init_data={
	11, sh.src_w, sh.src_h,0,0,extrahdr[0],
	1,extrahdr[1]
    }; // rv30

    mpxp_v<<"realvideo codec id: 0x"<<std::hex<<extrahdr[1]<<" sub-id: 0x"<<std::hex<<extrahdr[0]<<std::endl;

    // only I420 supported
    if(!parent.config_vf(sh.src_w,sh.src_h)) throw bad_format_exception();
    // init codec:
    result=(*rvyuv_init)(&init_data, &handle);
    if (result){
	mpxp_v<<"Couldn't open RealVideo codec, error code: 0x"<<std::hex<<result<<std::endl;
	throw bad_format_exception();
    }
    // setup rv30 codec (codec sub-type and image dimensions):
    if(extrahdr[1]>=0x20200002){
	uint32_t cmsg24[4]={sh.src_w,sh.src_h,sh.src_w,sh.src_h};
	/* FIXME: Broken for 64-bit pointers */
	cmsg_data_t cmsg_data={0x24,1+(extrahdr[1]&7), &cmsg24[0]};
	(*rvyuv_custom_message)(&cmsg_data,handle);
    }
    mpxp_ok<<"INFO: RealVideo codec init OK!"<<std::endl;
}

// uninit driver
vreal_decoder::~vreal_decoder() {
    if(rvyuv_free) rvyuv_free(handle);
    if(rv_handle) ::dlclose(rv_handle);
}

// to set/get/query special features/parameters
MPXP_Rc vreal_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    switch(cmd){
//    case VDCTRL_QUERY_MAX_PP_LEVEL:
//	    *((unsigned*)arg)=9;
//	    return MPXP_Ok;
//    case VDCTRL_SET_PP_LEVEL:
//	vfw_set_postproc(sh,10*(*((int*)arg)));
//	return MPXP_Ok;
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV)
			return MPXP_True;
	    else 	return MPXP_False;
    }
    return MPXP_Unknown;
}

// decode a frame
mp_image_t* vreal_decoder::run(const enc_frame_t& frame) {
    mp_image_t* mpi;
    unsigned long result;
    const dp_hdr_t* dp_hdr=(const dp_hdr_t*)frame.data;
    const char* dp_data=((const char*)frame.data)+sizeof(dp_hdr_t);
    const uint32_t* extra=(const uint32_t*)(((const char*)frame.data)+dp_hdr->chunktab);

    unsigned int transform_out[5];
    transform_in_t transform_in={
	dp_hdr->len,	// length of the packet (sub-packets appended)
	0,		// unknown, seems to be unused
	dp_hdr->chunks,	// number of sub-packets - 1
	extra,		// table of sub-packet offsets
	0,		// unknown, seems to be unused
	dp_hdr->timestamp,// timestamp (the integer value from the stream)
    };

    if(frame.len<=0 || frame.flags&2) return NULL; // skipped frame || hardframedrop

    mpi=parent.get_image(MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh.src_w, sh.src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    result=(*rvyuv_transform)(const_cast<char *>(dp_data), reinterpret_cast<char*>(mpi->planes[0]), &transform_in,
	transform_out, handle);

    return (result?NULL:mpi);
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) vreal_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_real_info = {
    "RealPlayer video codecs",
    "realvid",
    "Florian Schneider (using original closed source codecs for Linux)",
    "build-in",
    query_interface,
    options
};
} // namespace usr