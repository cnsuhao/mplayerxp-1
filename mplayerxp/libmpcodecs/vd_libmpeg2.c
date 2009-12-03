/*
 we still need that:
 benchmarks:
    ffmpeg12-0.4.9.pre.2 = 47.07%
    libmpeg2-0.2.0       = 42.46%
    libmpeg2-0.4.0       = 37.12%
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "config.h"
#include "../mplayer.h"
#include "vd_internal.h"
#include "../cpudetect.h"
#include "../mm_accel.h"
#include "../postproc/postprocess.h"
#include "codecs_ld.h"

static const vd_info_t info =
{
	"MPEG 1/2 Video decoder",
	"libmpeg2",
	"A'rpi",
	"Aaron & Walken",
	"native"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(libmpeg2)

#include "libmpdemux/parse_es.h"
#include "libvo/video_out.h"
#include "interface/mpeg2.h"
#ifdef ATTRIBUTE_ALIGNED_MAX
#define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#define ATTR_ALIGN(align)
#endif
#include "interface/mpeg2_internal.h"

typedef struct priv_s
{
    mpeg2dec_t *mpeg2dec;
}priv_t;

static mpeg2dec_t* (*mpeg2_init_ptr) (unsigned);
#define mpeg2_init(a) (*mpeg2_init_ptr)(a)
static void (*mpeg2_close_ptr) (mpeg2dec_t * mpeg2dec);
#define mpeg2_close(a) (*mpeg2_close_ptr)(a)
static const mpeg2_info_t * (*mpeg2_info_ptr) (mpeg2dec_t * mpeg2dec);
#define mpeg2_info(a) (*mpeg2_info_ptr)(a)
static int (*mpeg2_parse_ptr) (mpeg2dec_t * mpeg2dec);
#define mpeg2_parse(a) (*mpeg2_parse_ptr)(a)
static void (*mpeg2_buffer_ptr) (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end);
#define mpeg2_buffer(a,b,c) (*mpeg2_buffer_ptr)(a,b,c)
static void (*mpeg2_set_buf_ptr) (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], void * id);
#define mpeg2_set_buf(a,b,c) (*mpeg2_set_buf_ptr)(a,b,c)
static int (*mpeg2_stride_ptr) (mpeg2dec_t * mpeg2dec, int stride);
#define mpeg2_stride(a,b) (*mpeg2_stride_ptr)(a,b)
static void (*mpeg2_reset_ptr) (mpeg2dec_t * mpeg2dec, int full_reset);
#define mpeg2_reset(a,b) (*mpeg2_reset_ptr)(a,b)

static void *dll_handle;
static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  mpeg2_init_ptr = ld_sym(dll_handle,"mpeg2_init");
  mpeg2_close_ptr = ld_sym(dll_handle,"mpeg2_close");
  mpeg2_info_ptr = ld_sym(dll_handle,"mpeg2_info");
  mpeg2_parse_ptr = ld_sym(dll_handle,"mpeg2_parse");
  mpeg2_buffer_ptr = ld_sym(dll_handle,"mpeg2_buffer");
  mpeg2_set_buf_ptr = ld_sym(dll_handle,"mpeg2_set_buf");
  mpeg2_stride_ptr = ld_sym(dll_handle,"mpeg2_stride");
  mpeg2_reset_ptr = ld_sym(dll_handle,"mpeg2_reset");
  return mpeg2_init_ptr && mpeg2_close_ptr && mpeg2_info_ptr &&
	 mpeg2_parse_ptr && mpeg2_buffer_ptr && mpeg2_set_buf_ptr &&
	 mpeg2_stride_ptr && mpeg2_reset_ptr;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    priv_t *priv;
    priv=sh->context;
    switch(cmd)
    {
	case VDCTRL_RESYNC_STREAM:
	    /*lib starts looking for the next sequence header.*/
	    mpeg2_reset(priv->mpeg2dec,1);
	    return CONTROL_TRUE;
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12)
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
	default: break;
    }
    return CONTROL_UNKNOWN;
}

static int init(sh_video_t *sh){
    priv_t *priv;
    if(!load_lib(codec_name("libmpeg2"SLIBSUFFIX))) return 0;
    priv=sh->context=malloc(sizeof(priv_t));
    if(!(priv->mpeg2dec=mpeg2_init(mplayer_accel))) return 0;
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL);
}

// uninit driver
static void uninit(sh_video_t *sh){
    priv_t *priv=sh->context;
    mpeg2_close(priv->mpeg2dec);
    free(priv);
    dlclose(dll_handle);
}

static void draw_frame(mp_image_t *mpi,sh_video_t *sh,unsigned w,const mpeg2_fbuf_t *src)
{
    mpi->planes[0]=src->buf[0];
    mpi->planes[1]=src->buf[1];
    mpi->planes[2]=src->buf[2];
    mpi->stride[0]=w;
    mpi->stride[1]=
    mpi->stride[2]=w>>1;
    mpcodecs_draw_slice(sh,mpi);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    priv_t *priv=sh->context;
    mp_image_t *mpi;
    const mpeg2_info_t *info;
    int state,buf;
    if(len<=0) return NULL; // skipped null frame
    
#if 0
    // append extra 'end of frame' code:
    ((char*)data+len)[0]=0;
    ((char*)data+len)[1]=0;
    ((char*)data+len)[2]=1;
    ((char*)data+len)[3]=0xff;
    len+=4;
#endif
    info=mpeg2_info(priv->mpeg2dec);
    mpi=NULL;
    buf=0;
    MSG_DBG2("len=%u ***mpeg2_info***\n",len);
    while(1)
    {
	state=mpeg2_parse(priv->mpeg2dec);
	MSG_DBG2("%i=mpeg2_parse\n",state);
	switch(state)
	{
	    case STATE_BUFFER:
		mpeg2_buffer(priv->mpeg2dec,data,data+len);
		buf++;
		if(buf>2) return NULL; /* parsing of the passed buffer finished, return. */
		break;
	    case STATE_PICTURE:
		    if(!priv->mpeg2dec->decoder.mpq_store)
		    {
			priv->mpeg2dec->decoder.mpq_stride=(info->sequence->picture_width+15)>>4;
			priv->mpeg2dec->decoder.mpq_store=malloc(priv->mpeg2dec->decoder.mpq_stride*((info->sequence->picture_height+15)>>4));
		    }
		    mpi=mpcodecs_get_image(sh,MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_DRAW_CALLBACK
					,info->sequence->width,info->sequence->height);
		    mpeg2_stride(priv->mpeg2dec,mpi->stride[0]);
		    break;
	    case STATE_SLICE:
	    case STATE_END:
	    case STATE_INVALID:
		/* we must call draw_frame() only after STATE_BUFFER and STATE_PICTURE events */
		MSG_DBG2("display=%X discard=%X current=%X mpi=%X\n",info->display_fbuf,info->discard_fbuf,info->current_fbuf,mpi);
		/* Workaround for broken (badly demuxed) streams.
		Reset libmpeg2 to start decoding at the next picture. */
		if(state==STATE_END) mpeg2_reset(priv->mpeg2dec,0);
		if (info->display_fbuf && mpi)
		{
		    mpi->pict_type=info->current_picture->flags&PIC_MASK_CODING_TYPE;
		    mpi->qscale_type= 1;
		    mpi->qscale=priv->mpeg2dec->decoder.mpq_store;
		    mpi->qstride=priv->mpeg2dec->decoder.mpq_stride;
		    draw_frame (mpi,sh,info->sequence->width,info->display_fbuf);
		    return mpi;
		}
		break;
	    default: break;
	}
    }
    return NULL; /* segfault */
}

