#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <dlfcn.h>

#include "help_mp.h"
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

struct vd_private_t {
    any_t* handle;
    sh_video_t* sh;
    video_decoder_t* parent;
};

static const video_probe_t probes[] = {
    { "realvideo", "drv2.so.6.0", FOURCC_TAG('R','V','2','0'), VCodecStatus_Problems, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','3','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "realvideo", "drvc.so",     FOURCC_TAG('R','V','4','0'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(vd_private_t *p,uint32_t fourcc) {
    UNUSED(p);
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

/* copypaste from demux_real.c - it should match to get it working! */
typedef struct dp_hdr_s {
    uint32_t chunks;	// number of chunks
    uint32_t timestamp; // timestamp from packet header
    uint32_t len;	// length of actual data
    uint32_t chunktab;	// offset to chunk offset array
} dp_hdr_t;

/*
 * Structures for data packets.  These used to be tables of unsigned ints, but
 * that does not work on 64 bit platforms (e.g. Alpha).  The entries that are
 * pointers get truncated.  Pointers on 64 bit platforms are 8 byte longs.
 * So we have to use structures so the compiler will assign the proper space
 * for the pointer.
 */
typedef struct cmsg_data_s {
    uint32_t data1;
    uint32_t data2;
    uint32_t* dimensions;
} cmsg_data_t;

typedef struct transform_in_s {
    uint32_t len;
    uint32_t unknown1;
    uint32_t chunks;
    const uint32_t* extra;
    uint32_t unknown2;
    uint32_t timestamp;
} transform_in_t;


uint32_t (*rvyuv_custom_message)(cmsg_data_t*,any_t*);
uint32_t (*rvyuv_free)(any_t*);
uint32_t (*rvyuv_hive_message)(uint32_t,uint32_t);
uint32_t (*rvyuv_init)(any_t*,any_t*);
uint32_t (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,any_t*);

any_t*rv_handle=NULL;

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


// to set/get/query special features/parameters
static MPXP_Rc control_vd(vd_private_t *p,int cmd,any_t* arg,...){
    UNUSED(p);
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
static int load_syms(char *path) {
    any_t*handle;
    char *error;

    rv_handle = handle = dlopen (path, RTLD_LAZY);
    if (!handle) {
	MSG_ERR("DLError: %s\n",dlerror());
	return 0;
    }

    rvyuv_custom_message = (uint32_t (*)(cmsg_data_t*,any_t*))ld_sym(handle, "RV20toYUV420CustomMessage");
    if ((error = dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvCustomMessage): %s\n", error);
	return 0;
    }
    rvyuv_free = (uint32_t (*)(any_t*))ld_sym(handle, "RV20toYUV420Free");
    if ((error = dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvFree): %s\n", error);
	return 0;
    }
    rvyuv_hive_message = (uint32_t (*)(uint32_t,uint32_t))ld_sym(handle, "RV20toYUV420HiveMessage");
    if ((error = dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvHiveMessage): %s\n", error);
	return 0;
    }
    rvyuv_init = (uint32_t (*)(any_t*,any_t*))ld_sym(handle, "RV20toYUV420Init");
    if ((error = dlerror()) != NULL)  {
	MSG_ERR( "ld_sym(rvyuvInit): %s\n", error);
	return 0;
    }
    rvyuv_transform = (uint32_t (*)(char*,char*,transform_in_t*,unsigned int*,any_t*))ld_sym(handle, "RV20toYUV420Transform");
    if ((error = dlerror()) != NULL)  {
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

static vd_private_t* preinit(sh_video_t *sh){
    vd_private_t* priv = new(zeromem) vd_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(vd_private_t *p,video_decoder_t* opaque){
    sh_video_t* sh = p->sh;
    p->parent = opaque;
    //unsigned int out_fmt;
    int result;
    // we export codec id and sub-id from demuxer in bitmapinfohdr:
    unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
    struct rv_init_t init_data={
	11, sh->src_w, sh->src_h,0,0,extrahdr[0],
	1,extrahdr[1]
    }; // rv30

    MSG_V("realvideo codec id: 0x%08X  sub-id: 0x%08X\n",extrahdr[1],extrahdr[0]);

    if(!load_syms(sh->codec->dll_name)) return MPXP_False;

    // only I420 supported
    if(!mpcodecs_config_vf(opaque,sh->src_w,sh->src_h)) return MPXP_False;
    // init codec:
    result=(*rvyuv_init)(&init_data, &p->handle);
    if (result){
	MSG_ERR("Couldn't open RealVideo codec, error code: 0x%X  \n",result);
	return MPXP_False;
    }
    // setup rv30 codec (codec sub-type and image dimensions):
    if(extrahdr[1]>=0x20200002){
	uint32_t cmsg24[4]={sh->src_w,sh->src_h,sh->src_w,sh->src_h};
	/* FIXME: Broken for 64-bit pointers */
	cmsg_data_t cmsg_data={0x24,1+(extrahdr[1]&7), &cmsg24[0]};
	(*rvyuv_custom_message)(&cmsg_data,p->handle);
    }
    MSG_V("INFO: RealVideo codec init OK!\n");
    return MPXP_Ok;
}

// uninit driver
static void uninit(vd_private_t *p){
    sh_video_t* sh = p->sh;
    if(rvyuv_free) rvyuv_free(p->handle);
    if(rv_handle) dlclose(rv_handle);
    rv_handle=NULL;
    if(!sh) __pure_virtual();
    delete p;
}

// decode a frame
static mp_image_t* decode(vd_private_t *p,const enc_frame_t* frame){
    sh_video_t* sh = p->sh;
    mp_image_t* mpi;
    unsigned long result;
    const dp_hdr_t* dp_hdr=(const dp_hdr_t*)frame->data;
    const char* dp_data=((const char*)frame->data)+sizeof(dp_hdr_t);
    const uint32_t* extra=(const uint32_t*)(((const char*)frame->data)+dp_hdr->chunktab);

    unsigned int transform_out[5];
    transform_in_t transform_in={
	dp_hdr->len,	// length of the packet (sub-packets appended)
	0,		// unknown, seems to be unused
	dp_hdr->chunks,	// number of sub-packets - 1
	extra,		// table of sub-packet offsets
	0,		// unknown, seems to be unused
	dp_hdr->timestamp,// timestamp (the integer value from the stream)
    };

    if(frame->len<=0 || frame->flags&2) return NULL; // skipped frame || hardframedrop

    mpi=mpcodecs_get_image(p->parent, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    result=(*rvyuv_transform)(const_cast<char *>(dp_data), reinterpret_cast<char*>(mpi->planes[0]), &transform_in,
	transform_out, p->handle);

    return (result?NULL:mpi);
}
