#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"

#include "help_mp.h"
#include "codecs_ld.h"
#include "osdep/cpudetect.h"
#include "osdep/bswap.h"
#include "libao2/afmt.h"

#define IEC61937_DATA_TYPE_AC3 1

struct ad_private_t {
    sh_audio_t* sh;
    float last_pts;
};

struct hwac3info {
    unsigned bitrate, framesize, samplerate, bsmod;
};

struct frmsize_s {
    unsigned short bit_rate;
    unsigned short frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[64] = {
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

unsigned ac3_iec958_build_burst(unsigned length, unsigned data_type, unsigned big_endian, unsigned char * data, unsigned char * out)
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

int ac3_iec958_parse_syncinfo(unsigned char *buf, unsigned size, struct hwac3info *ai, unsigned *skipped)
{
    int samplerates[4] = { 48000, 44100, 32000, -1 };
    unsigned short _sync = 0;
    unsigned char *ptr = buf;
    int fscod, frmsizecod;
    struct syncframe *sf;

    _sync = buf[0] << 8;
    _sync |= buf[1];
    ptr = buf + 2;
    *skipped = 0;
    while (_sync != 0xb77 && *skipped < size - 8) {
	_sync <<= 8;
	_sync |= *ptr;
	ptr++;
	*skipped += 1;
    }
    if (_sync != 0xb77)
	return -1;
    ptr -= 2;
    sf = (struct syncframe *) ptr;
    fscod = (sf->syncinfo.code >> 6) & 0x03;
    ai->samplerate = samplerates[fscod];
    if ((int)ai->samplerate == -1)
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
int a52_fillbuff(ad_private_t *sh_audio,float *pts);

static const ad_info_t info = {
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

static const audio_probe_t probes[] = {
    { "hwac3", "hwac3", 0x2000, ACodecStatus_Working, {AFMT_AC3} },
    { "hwac3", "hwac3", 0x2001, ACodecStatus_Working, {AFMT_AC3} },
    { "hwac3", "hwac3", FOURCC_TAG('A','C','_','3'), ACodecStatus_Working, {AFMT_AC3} },
    { "hwac3", "hwac3", FOURCC_TAG('D','N','E','T'), ACodecStatus_Working, {AFMT_AC3} },
    { "hwac3", "hwac3", FOURCC_TAG('S','A','C','3'), ACodecStatus_Working, {AFMT_AC3} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(ad_private_t* ctx,uint32_t wtag) {
    unsigned i;
    UNUSED(ctx);
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}


ad_private_t* preinit(sh_audio_t *sh)
{
    /* Dolby AC3 audio: */
    ad_private_t* ctx=mpcodecs_ad_a52.preinit(sh);
    sh->audio_out_minsize=4*256*6;
    sh->audio_in_minsize=3840;
    sh->nch=2;
    sh->afmt=AFMT_AC3;
    return ctx;
}

MPXP_Rc init(ad_private_t *ctx)
{
    sh_audio_t* sh_audio = ctx->sh;
    /* Dolby AC3 passthrough:*/
    float pts;
    mpxp_a52_state=a52_init (mpxp_a52_accel); /* doesn't require mmx optimzation */
    if (mpxp_a52_state == NULL) {
	MSG_ERR("A52 init failed\n");
	return MPXP_False;
    }
    if(a52_fillbuff(ctx,&pts)<0) {
	MSG_ERR("A52 sync failed\n");
	return MPXP_False;
    }
 /*
  sh_audio->samplerate=ai.samplerate;   // SET by a52_fillbuff()
  sh_audio->samplesize=ai.framesize;
  sh_audio->i_bps=ai.bitrate*(1000/8);  // SET by a52_fillbuff()
  sh_audio->ac3_frame=mp_malloc(6144);
  sh_audio->o_bps=sh_audio->i_bps;  // XXX FIXME!!! XXX

   o_bps is calculated from samplesize*channels*samplerate
   a single ac3 frame is always translated to 6144 byte packet. (zero padding)*/
    sh_audio->nch=2;
    sh_audio->afmt=bps2afmt(2);   /* 2*2*(6*256) = 6144 (very TRICKY!)*/
    return MPXP_Ok;
}

void uninit(ad_private_t *ctx)
{
    mpcodecs_ad_a52.uninit(ctx);
}

MPXP_Rc control_ad(ad_private_t *ctx,int cmd,any_t* arg, ...)
{
    sh_audio_t* sh = ctx->sh;
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_RESYNC_STREAM:
	    sh->a_in_buffer_len=0;   // reset ACM/DShow audio buffer
	    return MPXP_True;
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    a52_fillbuff(ctx,&pts); // skip AC3 frame
	    return MPXP_True;
	}
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned decode(ad_private_t *ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    sh_audio_t* sh_audio = ctx->sh;
  unsigned len=0;
  UNUSED(minlen);
  UNUSED(maxlen);
  if(!sh_audio->a_in_buffer_len)
    if((int)(len=a52_fillbuff(ctx,pts))<0) return 0; /*EOF*/
  sh_audio->a_in_buffer_len=0;
  len = ac3_iec958_build_burst(len, 0x01, 1, (unsigned char *)sh_audio->a_in_buffer, buf);
  return len;
}
