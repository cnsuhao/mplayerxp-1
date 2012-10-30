#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"

#include "mp_config.h"
#include "help_mp.h"
#include "codecs_ld.h"
#include "osdep/cpudetect.h"

#include "libao2/afmt.h"
#include "ad_a52.h"

#define IEC61937_DATA_TYPE_AC3 1

struct hwac3info {
  int bitrate, framesize, samplerate, bsmod;
};

struct frmsize_s
{
        unsigned short bit_rate;
        unsigned short frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[64] =
{
        { 32  ,{64   ,69   ,96   } },
        { 32  ,{64   ,70   ,96   } },
        { 40  ,{80   ,87   ,120  } },
        { 40  ,{80   ,88   ,120  } },
        { 48  ,{96   ,104  ,144  } },
        { 48  ,{96   ,105  ,144  } },
        { 56  ,{112  ,121  ,168  } },
        { 56  ,{112  ,122  ,168  } },
        { 64  ,{128  ,139  ,192  } },
        { 64  ,{128  ,140  ,192  } },
        { 80  ,{160  ,174  ,240  } },
        { 80  ,{160  ,175  ,240  } },
        { 96  ,{192  ,208  ,288  } },
        { 96  ,{192  ,209  ,288  } },
        { 112 ,{224  ,243  ,336  } },
        { 112 ,{224  ,244  ,336  } },
        { 128 ,{256  ,278  ,384  } },
        { 128 ,{256  ,279  ,384  } },
        { 160 ,{320  ,348  ,480  } },
        { 160 ,{320  ,349  ,480  } },
        { 192 ,{384  ,417  ,576  } },
        { 192 ,{384  ,418  ,576  } },
        { 224 ,{448  ,487  ,672  } },
        { 224 ,{448  ,488  ,672  } },
        { 256 ,{512  ,557  ,768  } },
        { 256 ,{512  ,558  ,768  } },
        { 320 ,{640  ,696  ,960  } },
        { 320 ,{640  ,697  ,960  } },
        { 384 ,{768  ,835  ,1152 } },
        { 384 ,{768  ,836  ,1152 } },
        { 448 ,{896  ,975  ,1344 } },
        { 448 ,{896  ,976  ,1344 } },
        { 512 ,{1024 ,1114 ,1536 } },
        { 512 ,{1024 ,1115 ,1536 } },
        { 576 ,{1152 ,1253 ,1728 } },
        { 576 ,{1152 ,1254 ,1728 } },
        { 640 ,{1280 ,1393 ,1920 } },
        { 640 ,{1280 ,1394 ,1920 } }
};

struct syncframe {
  struct syncinfo {
    unsigned char syncword[2];
    unsigned char crc1[2];
    unsigned char code;
  } syncinfo;
  struct bsi {
    unsigned char bsidmod;
    unsigned char acmod;
  } bsi;
}; 

int ac3_iec958_build_burst(int length, int data_type, int big_endian, unsigned char * data, unsigned char * out)
{
	out[0] = 0x72;
	out[1] = 0xF8;
	out[2] = 0x1F;
	out[3] = 0x4E;
	out[4] = (length) ? data_type : 0; /* & 0x1F; */
	out[5] = 0x00;
	out[6] = (length << 3) & 0xFF;
	out[7] = (length >> 5) & 0xFF;
	if (big_endian)
		swab(data, out + 8, length);
	else
		memcpy(out + 8, data, length);
	memset(out + 8 + length, 0, 6144 - 8 - length);
	return 6144;
}

int ac3_iec958_parse_syncinfo(unsigned char *buf, int size, struct hwac3info *ai, int *skipped)
{
	int samplerates[4] = { 48000, 44100, 32000, -1 };
	unsigned short sync = 0;
	unsigned char *ptr = buf;
	int fscod, frmsizecod;
	struct syncframe *sf;
	
	sync = buf[0] << 8;
	sync |= buf[1];
	ptr = buf + 2;
	*skipped = 0;
	while (sync != 0xb77 && *skipped < size - 8) {
		sync <<= 8;
		sync |= *ptr;
		ptr++;
		*skipped += 1;
	}
	if (sync != 0xb77)
		return -1;
	ptr -= 2;
	sf = (struct syncframe *) ptr;
	fscod = (sf->syncinfo.code >> 6) & 0x03;
	ai->samplerate = samplerates[fscod];
	if (ai->samplerate == -1)
		return -1;
	frmsizecod = sf->syncinfo.code & 0x3f;
	ai->framesize = 2 * frmsizecod_tbl[frmsizecod].frm_size[fscod];
	ai->bitrate = frmsizecod_tbl[frmsizecod].bit_rate;
	if (((sf->bsi.bsidmod >> 3) & 0x1f) != 0x08)
		return -1;
	ai->bsmod = sf->bsi.bsidmod & 0x7;

	return 0;
}

typedef struct a52_state_s a52_state_t;
extern a52_state_t *mpxp_a52_state;
extern uint32_t mpxp_a52_accel;
extern uint32_t mpxp_a52_flags;
int a52_fillbuff(sh_audio_t *sh_audio,float *pts);

static const ad_info_t info =
{
	"AC3/DTS pass-through S/PDIF",
	"hwac3",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(hwac3)
extern ad_functions_t mpcodecs_ad_a52;

static a52_state_t * (*a52_init_ptr) (uint32_t mm_accel);
#define a52_init(a) (*a52_init_ptr)(a)

int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  mpcodecs_ad_a52.preinit(sh);
  sh->audio_out_minsize=4*256*6;
  sh->audio_in_minsize=3840;
  sh->channels=2;
  sh->sample_format=AFMT_AC3;
  return 1;
}

int init(sh_audio_t *sh_audio)
{
  /* Dolby AC3 passthrough:*/
  float pts;
  mpxp_a52_state=a52_init (mpxp_a52_accel); /* doesn't require mmx optimzation */
  if (mpxp_a52_state == NULL) {
       MSG_ERR("A52 init failed\n");
       return 0;
  }
  if(a52_fillbuff(sh_audio,&pts)<0) {
       MSG_ERR("A52 sync failed\n");
       return 0;
  }
 /* 
  sh_audio->samplerate=ai.samplerate;   // SET by a52_fillbuff()
  sh_audio->samplesize=ai.framesize;
  sh_audio->i_bps=ai.bitrate*(1000/8);  // SET by a52_fillbuff()
  sh_audio->ac3_frame=mp_malloc(6144);
  sh_audio->o_bps=sh_audio->i_bps;  // XXX FIXME!!! XXX

   o_bps is calculated from samplesize*channels*samplerate
   a single ac3 frame is always translated to 6144 byte packet. (zero padding)*/
  sh_audio->channels=2;
  sh_audio->samplesize=2;   /* 2*2*(6*256) = 6144 (very TRICKY!)*/
  return 1;
}

void uninit(sh_audio_t *sh)
{
  mpcodecs_ad_a52.uninit(sh);
}

int control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(arg);
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
	  sh->a_in_buffer_len=0;   // reset ACM/DShow audio buffer
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
	{
	  float pts;
	  a52_fillbuff(sh,&pts); // skip AC3 frame
	  return CONTROL_TRUE;
	}
      default:
	  return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
  unsigned len=0;
  UNUSED(minlen);
  UNUSED(maxlen);
  if(!sh_audio->a_in_buffer_len)
    if((int)(len=a52_fillbuff(sh_audio,pts))<0) return 0; /*EOF*/
  sh_audio->a_in_buffer_len=0;
  len = ac3_iec958_build_burst(len, 0x01, 1, sh_audio->a_in_buffer, buf);
  return len;
}
