#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "mp_config.h"
#include "help_mp.h"
#include "osdep/mplib.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libao2/afmt.h"

#include "ad_internal.h"

static ad_info_t info =
{
	"Raw DV Audio Decoder",
	"libdv",
	"Alexander Neundorf <neundorf@kde.org>",
	"http://libdv.sourceforge.net"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(libdv)

// defined in vd_libdv.c:
dv_decoder_t*  init_global_rawdv_decoder(void);

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=4*DV_AUDIO_MAX_SAMPLES*2;
  return 1;
}

typedef struct libdv_priv_s
{
    dv_decoder_t* decoder;
    int16_t *audioBuffers[4];
}priv_t;

static int init(sh_audio_t *sh)
{
  unsigned i;
  priv_t *priv;
  WAVEFORMATEX *h=sh->wf;

  if(!h) return 0;

  sh->i_bps=h->nAvgBytesPerSec;
  sh->nch=h->nChannels;
  sh->rate=h->nSamplesPerSec;
  sh->afmt=bps2afmt((h->wBitsPerSample+7)/8);
  priv = mp_mallocz(sizeof(priv_t));
  priv->decoder=init_global_rawdv_decoder();
  sh->context = priv;
  for (i=0; i < 4; i++)
    priv->audioBuffers[i] = mp_malloc(2*DV_AUDIO_MAX_SAMPLES);

  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
  priv_t *priv = sh_audio->context;
  unsigned i;
  UNUSED(sh_audio);
  for (i=0; i < 4; i++)
    mp_free(priv->audioBuffers[i]);
}

static MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    // TODO!!!
  UNUSED(sh);
  UNUSED(cmd);
  UNUSED(arg);
  return MPXP_Unknown;
}

static unsigned decode(sh_audio_t *sh, unsigned char *buf, unsigned minlen, unsigned maxlen,float *pts)
{
   priv_t *priv = sh->context;
   dv_decoder_t* decoder=priv->decoder;  //global_rawdv_decoder;
   unsigned char* dv_audio_frame=NULL;
   unsigned xx;
   size_t len=0;
   float apts;
   UNUSED(maxlen);
   while(len<minlen) {
    xx=ds_get_packet_r(sh->ds,&dv_audio_frame,len>0?&apts:pts);
    if((int)xx<=0 || !dv_audio_frame) return 0; // EOF?

    dv_parse_header(decoder, dv_audio_frame);

    if(xx!=decoder->frame_size)
       MSG_WARN("AudioFramesize differs %u %u\n",xx, decoder->frame_size);

    if (dv_decode_full_audio(decoder, dv_audio_frame,(int16_t**)priv->audioBuffers)) {
      /* Interleave the audio into a single buffer */
      int i=0;
      int16_t *bufP=(int16_t*)buf;

      for (i=0; i < decoder->audio->samples_this_frame; i++)
      {
         int ch;
         for (ch=0; ch < decoder->audio->num_channels; ch++)
            bufP[len++] = priv->audioBuffers[ch][i];
      }
    }
    len+=decoder->audio->samples_this_frame;
    buf+=decoder->audio->samples_this_frame;
   }
   return len*2;
}
