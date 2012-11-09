#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"
#include "libao2/afmt.h"

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

MPXP_Rc init(sh_audio_t *sh_audio)
{
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

MPXP_Rc preinit(sh_audio_t *sh)
{
    sh->audio_out_minsize=16384;
    return MPXP_Ok;
}

void uninit(sh_audio_t *sh)
{
    UNUSED(sh);
}

MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
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

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
  unsigned len = sh_audio->nch*afmt2bps(sh_audio->afmt);
  len = (minlen + len - 1) / len * len;
  if (len > maxlen)
      /* if someone needs hundreds of channels adjust audio_out_minsize based on channels in preinit() */
      return -1;
  len=demux_read_data_r(sh_audio->ds,buf,len,pts);
  return len;
}
