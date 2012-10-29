#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"
#include "osdep/bswap.h"
#include "libao2/afmt.h"

static const ad_info_t info =
{
	"Uncompressed DVD/VOB LPCM audio decoder",
	"dvdpcm",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(dvdpcm)

int init(sh_audio_t *sh)
{
/* DVD PCM Audio:*/
    sh->i_bps = 0;
    if(sh->codecdata_len==3){
	// we have LPCM header:
	unsigned char h=sh->codecdata[1];
	sh->channels=1+(h&7);
	switch((h>>4)&3){
	case 0: sh->samplerate=48000;break;
	case 1: sh->samplerate=96000;break;
	case 2: sh->samplerate=44100;break;
	case 3: sh->samplerate=32000;break;
	}
	switch ((h >> 6) & 3) {
	  case 0:
	    sh->sample_format = AFMT_S16_BE;
	    sh->samplesize = 2;
	    break;
	  case 1:
	    //sh->sample_format = AFMT_S20_BE;
	    sh->i_bps = sh->channels * sh->samplerate * 5 / 2;
	  case 2: 
	    sh->sample_format = AFMT_S24_BE;
	    sh->samplesize = 3;
	    break;
	  default:
	    sh->sample_format = AFMT_S16_BE;
	    sh->samplesize = 2;
	}
    } else {
	// use defaults:
	sh->channels=2;
	sh->samplerate=48000;
	sh->sample_format = AFMT_S16_BE;
	sh->samplesize = 2;
    }
    if (!sh->i_bps)
    sh->i_bps = sh->samplesize * sh->channels * sh->samplerate;
    return 1;
}

int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=2048;
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

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
  int j;
  unsigned len;
  float null_pts;
  UNUSED(maxlen);
  if (sh_audio->samplesize == 3) {
    if (((sh_audio->codecdata[1] >> 6) & 3) == 1) {
      // 20 bit
      // not sure if the "& 0xf0" and "<< 4" are the right way around
      // can somebody clarify?
      for (j = 0; j < minlen; j += 12) {
        char tmp[10];
        len = demux_read_data_r(sh_audio->ds, tmp, 10,j?&null_pts:pts);
        if (len < 10) break;
        // first sample
        buf[j + 0] = tmp[0];
        buf[j + 1] = tmp[1];
        buf[j + 2] = tmp[8] & 0xf0;
        // second sample
        buf[j + 3] = tmp[2];
        buf[j + 4] = tmp[3];
        buf[j + 5] = tmp[8] << 4;
        // third sample
        buf[j + 6] = tmp[4];
        buf[j + 7] = tmp[5];
        buf[j + 8] = tmp[9] & 0xf0;
        // fourth sample
        buf[j + 9] = tmp[6];
        buf[j + 10] = tmp[7];
        buf[j + 11] = tmp[9] << 4;
      }
      len = j;
    } else {
      // 24 bit
      for (j = 0; j < minlen; j += 12) {
        char tmp[12];
        len = demux_read_data_r(sh_audio->ds, tmp, 12, j?&null_pts:pts);
        if (len < 12) break;
        // first sample
        buf[j + 0] = tmp[0];
        buf[j + 1] = tmp[1];
        buf[j + 2] = tmp[8];
        // second sample
        buf[j + 3] = tmp[2];
        buf[j + 4] = tmp[3];
        buf[j + 5] = tmp[9];
        // third sample
        buf[j + 6] = tmp[4];
        buf[j + 7] = tmp[5];
        buf[j + 8] = tmp[10];
        // fourth sample
        buf[j + 9] = tmp[6];
        buf[j + 10] = tmp[7];
        buf[j + 11] = tmp[11];
      }
      len = j;
    }
  } else
  len=demux_read_data_r(sh_audio->ds,buf,(minlen+3)&(~3),pts);
  return len;
}
