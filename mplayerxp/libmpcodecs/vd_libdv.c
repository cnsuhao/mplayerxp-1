#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "mp_config.h"

#include "libvo/img_format.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "vd_internal.h"

static const vd_info_t info = {
    "Raw DV Video Decoder",
    "libdv",
    "Alexander Neundorf <neundorf@kde.org>",
    "http://libdv.sourceforge.net"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(libdv)

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    return MPXP_Unknown;
}

static dv_decoder_t* global_rawdv_decoder=NULL;

dv_decoder_t* init_global_rawdv_decoder(void)
{
    if(!global_rawdv_decoder){
	global_rawdv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
	global_rawdv_decoder->quality=DV_QUALITY_BEST;
	global_rawdv_decoder->prev_frame_decoded = 0;
    }
    return global_rawdv_decoder;
}

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput)
{
    sh->context = (any_t*)init_global_rawdv_decoder();
    return mpcodecs_config_vo(sh,sh->src_w,sh->src_h,NULL,libinput);
}

// uninit driver
static void uninit(sh_video_t *sh) {}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame)
{
   mp_image_t* mpi;
   dv_decoder_t *priv=sh->context;

   if(frame->len<=0 || (frame->flags&3)){
//      fprintf(stderr,"decode() (rawdv) SKIPPED\n");
      return NULL; // skipped frame
   }

   dv_parse_header(priv, frame->data);

   mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, sh->src_w, sh->src_h);

   if(!mpi){	// temporary!
      MSG_ERR("couldn't allocate image for stderr codec\n");
      return NULL;
   }

   dv_decode_full_frame(priv, frame->data, e_dv_color_yuv, mpi->planes, mpi->stride);

   return mpi;
}
