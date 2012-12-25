#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <dlfcn.h>

#include "mpxp_help.h"
#include "codecs_ld.h"
#include "vd_internal.h"
#include "vd_msg.h"
#include "osdep/bswap.h"

static const vd_info_t info = {
    "RealPlayer video codecs",
    "realvid",
    "Florian Schneider (using original closed source codecs for Linux)",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(real)


static const video_probe_t probes[] = {
    { "realvideo", "drv2.so.6.0", FOURCC_TAG('R','V','2','0'), VCodecStatus_Problems, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','3','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','4','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

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

#if 0
any_t*__builtin_vec_new(unsigned long size) {
    return mp_malloc(size);
}

void __builtin_vec_delete(any_t*mem) {
    delete mem;
}

void __pure_virtual(void)
{
    MSG_ERR( "I'm outa here!\n");
    exit(1);
}
#endif
struct vreal_private_t : public Opaque {
    vreal_private_t();
    virtual ~vreal_private_t();

    any_t* handle;
    sh_video_t* sh;
    video_decoder_t* parent;
    any_t* rv_handle;
    uint32_t (*rvyuv_custom_message)(cmsg_data_t*,any_t*);
    uint32_t (*rvyuv_free)(any_t*);
    uint32_t (*rvyuv_hive_message)(uint32_t,uint32_t);
    uint32_t (*rvyuv_init)(any_t*,any_t*);
    uint32_t (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,any_t*);
};
vreal_private_t::vreal_private_t() {}
vreal_private_t::~vreal_private_t() {
    if(rvyuv_free) rvyuv_free(handle);
    if(rv_handle) ::dlclose(rv_handle);
}

// to set/get/query special features/parameters
static MPXP_Rc control_vd(Opaque& ctx,int cmd,any_t* arg,...){
    UNUSED(ctx);
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

/* exits program when failure */
static int load_lib(vreal_private_t& priv,const char *path) {
    any_t*handle;
    char *error;

    priv.rv_handle = handle = ::dlopen (path, RTLD_LAZY);
    if (!handle) {
	MSG_ERR("DLError: %s\n",dlerror());
	return 0;
    }

    priv.rvyuv_custom_message = (uint32_t (*)(cmsg_data_t*,any_t*))ld_sym(handle, "RV20toYUV420CustomMessage");
    if ((error = ::dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvCustomMessage): %s\n", error);
	return 0;
    }
    priv.rvyuv_free = (uint32_t (*)(any_t*))ld_sym(handle, "RV20toYUV420Free");
    if ((error = ::dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvFree): %s\n", error);
	return 0;
    }
    priv.rvyuv_hive_message = (uint32_t (*)(uint32_t,uint32_t))ld_sym(handle, "RV20toYUV420HiveMessage");
    if ((error = ::dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvHiveMessage): %s\n", error);
	return 0;
    }
    priv.rvyuv_init = (uint32_t (*)(any_t*,any_t*))ld_sym(handle, "RV20toYUV420Init");
    if ((error = ::dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvInit): %s\n", error);
	return 0;
    }
    priv.rvyuv_transform = (uint32_t (*)(char*,char*,transform_in_t*,unsigned int*,any_t*))ld_sym(handle, "RV20toYUV420Transform");
    if ((error = ::dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvTransform): %s\n", error);
	return 0;
    }
    return 1;
}

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
} rv_init_t;

static Opaque* preinit(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi){
    UNUSED(psi);
    vreal_private_t* priv = new(zeromem) vreal_private_t;
    if(!load_lib(*priv,probe.codec_dll)) { delete priv; return NULL; }
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(Opaque& ctx,video_decoder_t& opaque){
    vreal_private_t& priv=static_cast<vreal_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    priv.parent = &opaque;
    //unsigned int out_fmt;
    int result;
    // we export codec id and sub-id from demuxer in bitmapinfohdr:
    unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
    struct rv_init_t init_data={
	11, sh->src_w, sh->src_h,0,0,extrahdr[0],
	1,extrahdr[1]
    }; // rv30

    MSG_V("realvideo codec id: 0x%08X  sub-id: 0x%08X\n",extrahdr[1],extrahdr[0]);

    // only I420 supported
    if(!mpcodecs_config_vf(opaque,sh->src_w,sh->src_h)) return MPXP_False;
    // init codec:
    result=(*priv.rvyuv_init)(&init_data, &priv.handle);
    if (result){
	MSG_ERR("Couldn't open RealVideo codec, error code: 0x%X  \n",result);
	return MPXP_False;
    }
    // setup rv30 codec (codec sub-type and image dimensions):
    if(extrahdr[1]>=0x20200002){
	uint32_t cmsg24[4]={sh->src_w,sh->src_h,sh->src_w,sh->src_h};
	/* FIXME: Broken for 64-bit pointers */
	cmsg_data_t cmsg_data={0x24,1+(extrahdr[1]&7), &cmsg24[0]};
	(*priv.rvyuv_custom_message)(&cmsg_data,priv.handle);
    }
    MSG_V("INFO: RealVideo codec init OK!\n");
    return MPXP_Ok;
}

// uninit driver
static void uninit(Opaque& ctx){ UNUSED(ctx); }

// decode a frame
static mp_image_t* decode(Opaque& ctx,const enc_frame_t& frame){
    vreal_private_t& priv=static_cast<vreal_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
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

    mpi=mpcodecs_get_image(*priv.parent, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    result=(*priv.rvyuv_transform)(const_cast<char *>(dp_data), reinterpret_cast<char*>(mpi->planes[0]), &transform_in,
	transform_out, priv.handle);

    return (result?NULL:mpi);
}
