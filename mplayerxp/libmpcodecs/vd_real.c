#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "mp_config.h"

#include <dlfcn.h>

#include "help_mp.h"
#include "codecs_ld.h"
#include "vd_internal.h"
#include "vd_msg.h"

static const vd_info_t info = {
	"RealPlayer video codecs",
	"realvid",
	"Florian Schneider (using original closed source codecs for Linux)",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBVD_EXTERN(real)

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
	uint32_t* extra;
	uint32_t unknown2;
	uint32_t timestamp;
} transform_in_t;


uint32_t (*rvyuv_custom_message)(cmsg_data_t*,void*);
uint32_t (*rvyuv_free)(void*);
uint32_t (*rvyuv_hive_message)(uint32_t,uint32_t);
uint32_t (*rvyuv_init)(void*,void*);
uint32_t (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);

void *rv_handle=NULL;

void *__builtin_vec_new(unsigned long size) {
	return malloc(size);
}

void __builtin_vec_delete(void *mem) {
	free(mem);
}

void __pure_virtual(void)
{
	MSG_ERR( "I'm outa here!\n");
	exit(1);
}


// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
//    case VDCTRL_QUERY_MAX_PP_LEVEL:
//	return 9;
//    case VDCTRL_SET_PP_LEVEL:
//	vfw_set_postproc(sh,10*(*((int*)arg)));
//	return CONTROL_OK;
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV) 
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

/* exits program when failure */
static int load_syms(char *path) {
		void *handle;
		char *error;

		rv_handle = handle = dlopen (path, RTLD_LAZY);
		if (!handle) {
			MSG_ERR("DLError: %s\n",dlerror());
			return 0;
		}

		rvyuv_custom_message = ld_sym(handle, "RV20toYUV420CustomMessage");
		if ((error = dlerror()) != NULL)  {
			MSG_ERR( "ld_sym(rvyuvCustomMessage): %s\n", error);
			return 0;
		}
		rvyuv_free = ld_sym(handle, "RV20toYUV420Free");
		if ((error = dlerror()) != NULL)  {
			MSG_ERR( "ld_sym(rvyuvFree): %s\n", error);
			return 0;
		}
		rvyuv_hive_message = ld_sym(handle, "RV20toYUV420HiveMessage");
		if ((error = dlerror()) != NULL)  {
			MSG_ERR( "ld_sym(rvyuvHiveMessage): %s\n", error);
			return 0;
		}
		rvyuv_init = ld_sym(handle, "RV20toYUV420Init");
		if ((error = dlerror()) != NULL)  {
			MSG_ERR( "ld_sym(rvyuvInit): %s\n", error);
			return 0;
		}
		rvyuv_transform = ld_sym(handle, "RV20toYUV420Transform");
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

// init driver
static int init(sh_video_t *sh){
	//unsigned int out_fmt;
	int result;
	// we export codec id and sub-id from demuxer in bitmapinfohdr:
	unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
	struct rv_init_t init_data={
		11, sh->disp_w, sh->disp_h,0,0,extrahdr[0],
		1,extrahdr[1]}; // rv30

	MSG_V("realvideo codec id: 0x%08X  sub-id: 0x%08X\n",extrahdr[1],extrahdr[0]);

	if(!load_syms(sh->codec->dll_name)){
		return 0;
	}
	// only I420 supported
	if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL)) return 0;
	// init codec:
	sh->context=NULL;
	result=(*rvyuv_init)(&init_data, &sh->context);
	if (result){
	    MSG_ERR("Couldn't open RealVideo codec, error code: 0x%X  \n",result);
	    return 0;
	}
	// setup rv30 codec (codec sub-type and image dimensions):
	if(extrahdr[1]>=0x20200002){
	    uint32_t cmsg24[4]={sh->disp_w,sh->disp_h,sh->disp_w,sh->disp_h};
	    /* FIXME: Broken for 64-bit pointers */
	    cmsg_data_t cmsg_data={0x24,1+(extrahdr[1]&7), &cmsg24[0]};
	    (*rvyuv_custom_message)(&cmsg_data,sh->context);
	}
	MSG_V("INFO: RealVideo codec init OK!\n");
	return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
	if(rvyuv_free) rvyuv_free(sh->context);
	if(rv_handle) dlclose(rv_handle);
	rv_handle=NULL;
	if(!sh) __pure_virtual();
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
	mp_image_t* mpi;
	unsigned long result;
	dp_hdr_t* dp_hdr=(dp_hdr_t*)data;
	unsigned char* dp_data=((unsigned char*)data)+sizeof(dp_hdr_t);
	uint32_t* extra=(uint32_t*)(((char*)data)+dp_hdr->chunktab);

	unsigned int transform_out[5];
	transform_in_t transform_in={
		dp_hdr->len,	// length of the packet (sub-packets appended)
		0,		// unknown, seems to be unused
		dp_hdr->chunks,	// number of sub-packets - 1
		extra,		// table of sub-packet offsets
		0,		// unknown, seems to be unused
		dp_hdr->timestamp,// timestamp (the integer value from the stream)
	};

	if(len<=0 || flags&2) return NULL; // skipped frame || hardframedrop

	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
		sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
	
	result=(*rvyuv_transform)(dp_data, mpi->planes[0], &transform_in,
		transform_out, sh->context);

	return (result?NULL:mpi);
}
