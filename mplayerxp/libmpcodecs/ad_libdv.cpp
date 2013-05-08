#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "mpxp_help.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libao3/afmt.h"
#include "osdep/bswap.h"
#include "ad_internal.h"

struct libadv_private_t : public Opaque {
    libadv_private_t();
    virtual ~libadv_private_t();

    dv_decoder_t*	decoder;
    int16_t*		audioBuffers[4];
    sh_audio_t*		sh;
};
libadv_private_t::libadv_private_t() {}
libadv_private_t::~libadv_private_t() {
    unsigned i;
    for (i=0; i < 4; i++) delete audioBuffers[i];
}

static const ad_info_t info = {
    "Raw DV Audio Decoder",
    "libdv",
    "Alexander Neundorf <neundorf@kde.org>",
    "http://libdv.sourceforge.net"
};

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(libdv)

static const audio_probe_t probes[] = {
    { "libdv", "libdv", FOURCC_TAG('R','A','D','V'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}

// defined in vd_libdv.c:
dv_decoder_t*  init_global_rawdv_decoder(void);

static Opaque* preinit(const audio_probe_t& probe,sh_audio_t *sh_audio,audio_filter_info_t& afi)
{
    UNUSED(probe);
    UNUSED(afi);
    sh_audio->audio_out_minsize=4*DV_AUDIO_MAX_SAMPLES*2;
    libadv_private_t* priv = new(zeromem) libadv_private_t;
    priv->sh = sh_audio;
    return priv;
}

static MPXP_Rc init(Opaque& ctx)
{
    libadv_private_t& priv=static_cast<libadv_private_t&>(ctx);
    unsigned i;
    sh_audio_t* sh = priv.sh;
    WAVEFORMATEX *h=sh->wf;

    if(!h) return MPXP_False;

    sh->i_bps=h->nAvgBytesPerSec;
    sh->nch=h->nChannels;
    sh->rate=h->nSamplesPerSec;
    sh->afmt=bps2afmt((h->wBitsPerSample+7)/8);
    priv.decoder=init_global_rawdv_decoder();
    for (i=0; i < 4; i++)
	priv.audioBuffers[i] = new int16_t[DV_AUDIO_MAX_SAMPLES];

    return MPXP_Ok;
}

static void uninit(Opaque& ctx) { UNUSED(ctx); }

static MPXP_Rc control_ad(Opaque& ctx,int cmd,any_t* arg, ...)
{
    // TODO!!!
    UNUSED(ctx);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static unsigned decode(Opaque& ctx, unsigned char *buf, unsigned minlen, unsigned maxlen,float& pts)
{
    libadv_private_t& priv=static_cast<libadv_private_t&>(ctx);
    sh_audio_t* sh = priv.sh;
    dv_decoder_t* decoder=priv.decoder;  //global_rawdv_decoder;
    unsigned char* dv_audio_frame=NULL;
    unsigned xx;
    size_t len=0;
    float apts;
    UNUSED(maxlen);
   while(len<minlen) {
    xx=ds_get_packet_r(*sh->ds,&dv_audio_frame,len>0?apts:pts);
    if((int)xx<=0 || !dv_audio_frame) return 0; // EOF?

    dv_parse_header(decoder, dv_audio_frame);

    if(xx!=decoder->frame_size)
       MSG_WARN("AudioFramesize differs %u %u\n",xx, decoder->frame_size);

    if (dv_decode_full_audio(decoder, dv_audio_frame,(int16_t**)priv.audioBuffers)) {
      /* Interleave the audio into a single buffer */
      int i=0;
      int16_t *bufP=(int16_t*)buf;

      for (i=0; i < decoder->audio->samples_this_frame; i++)
      {
	 int ch;
	 for (ch=0; ch < decoder->audio->num_channels; ch++)
	    bufP[len++] = priv.audioBuffers[ch][i];
      }
    }
    len+=decoder->audio->samples_this_frame;
    buf+=decoder->audio->samples_this_frame;
   }
   return len*2;
}
