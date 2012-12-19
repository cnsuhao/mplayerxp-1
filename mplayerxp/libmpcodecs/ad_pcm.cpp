#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"
#include "libao3/afmt.h"
#include "osdep/bswap.h"

struct ad_private_t {
    sh_audio_t* sh;
    audio_filter_info_t* afi;
};

static const ad_info_t info = {
    "Uncompressed PCM audio decoder",
    "pcm",
    "Nickols_K",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(pcm)

static const audio_probe_t probes[] = {
    { "pcm", "pcm", 0x0,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0x1,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0x3,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0xFFFE,ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('F','L','3','2'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('2','3','L','F'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('N','O','N','E'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('I','N','2','4'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('4','2','N','I'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('I','N','3','2'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('2','3','N','I'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('L','P','C','M'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('R','A','W',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('T','W','O','S'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('S','O','W','T'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}


MPXP_Rc init(ad_private_t *priv)
{
    sh_audio_t* sh_audio = priv->sh;
    WAVEFORMATEX *h=sh_audio->wf;
    sh_audio->i_bps=h->nAvgBytesPerSec;
    sh_audio->nch=h->nChannels;
    sh_audio->rate=h->nSamplesPerSec;
    sh_audio->afmt=bps2afmt((h->wBitsPerSample+7)/8);
    switch(sh_audio->wtag){ /* hardware formats: */
	case 0x3:  sh_audio->afmt=AFMT_FLOAT32; break;
	case 0x6:  sh_audio->afmt=AFMT_A_LAW;break;
	case 0x7:  sh_audio->afmt=AFMT_MU_LAW;break;
	case 0x11: sh_audio->afmt=AFMT_IMA_ADPCM;break;
	case 0x50: sh_audio->afmt=AFMT_MPEG;break;
/*	case 0x2000: sh_audio->sample_format=AFMT_AC3; */
	case mmioFOURCC('r','a','w',' '): /* 'raw '*/
	    break;
	case mmioFOURCC('t','w','o','s'): /* 'twos'*/
	    if(afmt2bps(sh_audio->afmt)!=1) sh_audio->afmt=AFMT_S16_BE;
	    break;
	case mmioFOURCC('s','o','w','t'): /* 'swot'*/
	    if(afmt2bps(sh_audio->afmt)!=1) sh_audio->afmt=AFMT_S16_LE;
	    break;
	default:
	    if(afmt2bps(sh_audio->afmt)==1);
	    else if(afmt2bps(sh_audio->afmt)==2) sh_audio->afmt=AFMT_S16_LE;
	    else if(afmt2bps(sh_audio->afmt)==3) sh_audio->afmt=AFMT_S24_LE;
	    else sh_audio->afmt=AFMT_S32_LE;
	    break;
    }
    return MPXP_Ok;
}

ad_private_t* preinit(const audio_probe_t* probe,sh_audio_t *sh,audio_filter_info_t* afi)
{
    UNUSED(probe);
    sh->audio_out_minsize=16384;
    ad_private_t* priv = new(zeromem) ad_private_t;
    priv->sh = sh;
    priv->afi = afi;
    return priv;
}

void uninit(ad_private_t *priv)
{
    delete priv;
}

MPXP_Rc control_ad(ad_private_t *priv,int cmd,any_t* arg, ...)
{
    sh_audio_t* sh = priv->sh;
    int skip;
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    skip=sh->i_bps/16;
	    skip=skip&(~3);
	    demux_read_data_r(sh->ds,NULL,skip,&pts);
	    return MPXP_True;
	}
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned decode(ad_private_t *priv,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    sh_audio_t* sh_audio = priv->sh;
  unsigned len = sh_audio->nch*afmt2bps(sh_audio->afmt);
  len = (minlen + len - 1) / len * len;
  if (len > maxlen)
      /* if someone needs hundreds of channels adjust audio_out_minsize based on channels in preinit() */
      return -1;
  len=demux_read_data_r(sh_audio->ds,buf,len,pts);
  return len;
}
