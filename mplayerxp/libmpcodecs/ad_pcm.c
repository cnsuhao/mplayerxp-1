#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"
#include "../libao2/afmt.h"

static const ad_info_t info =
{
	"Uncompressed PCM audio decoder",
	"pcm",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(pcm)

int init(sh_audio_t *sh_audio)
{
  WAVEFORMATEX *h=sh_audio->wf;
  sh_audio->i_bps=h->nAvgBytesPerSec;
  sh_audio->channels=h->nChannels;
  sh_audio->samplerate=h->nSamplesPerSec;
  sh_audio->samplesize=(h->wBitsPerSample+7)/8;
  switch(sh_audio->format){ /* hardware formats: */
    case 0x3: sh_audio->sample_format=AFMT_FLOAT32; break;
    case 0x6:  sh_audio->sample_format=AFMT_A_LAW;break;
    case 0x7:  sh_audio->sample_format=AFMT_MU_LAW;break;
    case 0x11: sh_audio->sample_format=AFMT_IMA_ADPCM;break;
    case 0x50: sh_audio->sample_format=AFMT_MPEG;break;
/*    case 0x2000: sh_audio->sample_format=AFMT_AC3; */
    case mmioFOURCC('r','a','w',' '): /* 'raw '*/
       if(sh_audio->samplesize==1) sh_audio->sample_format=AFMT_S8;
       else if(sh_audio->samplesize==2) sh_audio->sample_format=AFMT_S16_BE;
       else if(sh_audio->samplesize==3) sh_audio->sample_format=AFMT_S24_BE;
       else sh_audio->sample_format=AFMT_S32_BE;
       break;
    case mmioFOURCC('t','w','o','s'): /* 'twos'*/
       if(sh_audio->samplesize==1) sh_audio->sample_format=AFMT_S8;
       else			   sh_audio->sample_format=AFMT_S16_BE;
       break;
    case mmioFOURCC('s','o','w','t'): /* 'swot'*/
       if(sh_audio->samplesize==1) sh_audio->sample_format=AFMT_S8;
       else			   sh_audio->sample_format=AFMT_S16_LE;
       break;
    default:
       if(sh_audio->samplesize==1) sh_audio->sample_format=AFMT_S8;
       else if(sh_audio->samplesize==2) sh_audio->sample_format=AFMT_S16_LE;
       else if(sh_audio->samplesize==3) sh_audio->sample_format=AFMT_S24_LE;
       else sh_audio->sample_format=AFMT_S32_LE;
       break;
  }
  return 1;
}

int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=16384;
  return 1;
}

void uninit(sh_audio_t *sh)
{
    UNUSED(sh);
}

int control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
  int skip;
  UNUSED(arg);
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
      {
        float pts;
	skip=sh->i_bps/16;
	skip=skip&(~3);
	demux_read_data_r(sh->ds,NULL,skip,&pts);
	return CONTROL_TRUE;
      }
      default:
	return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

unsigned mpca_decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
  unsigned len = sh_audio->channels*sh_audio->samplesize;
  len = (minlen + len - 1) / len * len;
  if (len > maxlen)
      /* if someone needs hundreds of channels adjust audio_out_minsize based on channels in preinit() */
      return -1;
  len=demux_read_data_r(sh_audio->ds,buf,len,pts);
  return len;
}
