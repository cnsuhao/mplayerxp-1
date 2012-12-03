#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "codecs_ld.h"

#include "mplayerxp.h"
#include "help_mp.h"
#include "osdep/cpudetect.h"
#include "osdep/bswap.h"

#include "osdep/mm_accel.h"
#include "mplayerxp.h"
#include "liba52/a52.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "postproc/af.h"

struct ad_private_t {
    sh_audio_t* sh;
    float last_pts;
};

#define MAX_AC3_FRAME 3840

a52_state_t* mpxp_a52_state;
uint32_t mpxp_a52_accel=0;
uint32_t mpxp_a52_flags=0;

static const ad_info_t info =
{
	"liba52 AC3 driver",
	"liba52",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(a52)

static const audio_probe_t probes[] = {
    { "liba52", "liba52", 0x2000, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", 0x20736D, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('A','C','_','3'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('D','N','E','T'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('S','A','C','3'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
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

int a52_fillbuff(ad_private_t *priv,float *pts){
    int length=0;
    int flags=0;
    int sample_rate=0;
    int bit_rate=0;
    float apts=0.,null_pts;
    sh_audio_t* sh_audio = priv->sh;

    sh_audio->a_in_buffer_len=0;
    /* sync frame:*/
    while(1){
	while(sh_audio->a_in_buffer_len<8){
	    int c=demux_getc_r(sh_audio->ds,apts?&null_pts:&apts);
	    if(c<0) { priv->last_pts=*pts=apts; return -1; } /* EOF*/
	    sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
	}
	if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
	length = a52_syncinfo ((uint8_t*)sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
	if(length>=7 && length<=MAX_AC3_FRAME) break; /* we're done.*/
	/* bad file => resync*/
	if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
	memmove(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,7);
	--sh_audio->a_in_buffer_len;
	apts=0;
    }
    MSG_DBG2("a52: len=%d  flags=0x%X  %d Hz %d bit/s\n",length,flags,sample_rate,bit_rate);
    sh_audio->rate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    demux_read_data_r(sh_audio->ds,(uint8_t*)sh_audio->a_in_buffer+8,length-8,apts?&null_pts:&apts);
    if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer+8,sh_audio->a_in_buffer+8,length-8);
    priv->last_pts=*pts=apts;
    if(crc16_block((uint8_t*)sh_audio->a_in_buffer+2,length-2)!=0)
	MSG_STATUS("a52: CRC check failed!  \n");
    return length;
}

/* returns: number of available channels*/
static int a52_printinfo(sh_audio_t *sh_audio){
    int flags, sample_rate, bit_rate;
    const char* mode="unknown";
    int channels=0;
    a52_syncinfo ((uint8_t*)sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    switch(flags&A52_CHANNEL_MASK){
	case A52_CHANNEL: mode="channel"; channels=2; break;
	case A52_MONO: mode="mono"; channels=1; break;
	case A52_STEREO: mode="stereo"; channels=2; break;
	case A52_3F: mode="3f";channels=3;break;
	case A52_2F1R: mode="2f+1r";channels=3;break;
	case A52_3F1R: mode="3f+1r";channels=4;break;
	case A52_2F2R: mode="2f+2r";channels=4;break;
	case A52_3F2R: mode="3f+2r";channels=5;break;
	case A52_CHANNEL1: mode="channel1"; channels=2; break;
	case A52_CHANNEL2: mode="channel2"; channels=2; break;
	case A52_DOLBY: mode="dolby"; channels=2; break;
    }
    MSG_INFO("AC3: %d.%d (%s%s)  %d Hz  %3.1f kbit/s Out: %u-bit\n",
	channels, (flags&A52_LFE)?1:0,
	mode, (flags&A52_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f,
	afmt2bps(sh_audio->afmt)*8);
  return (flags&A52_LFE) ? (channels+1) : channels;
}

ad_private_t* preinit(sh_audio_t *sh)
{
    ad_private_t* ctx=new(zeromem) ad_private_t;
  /* Dolby AC3 audio: */
  /* however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
#ifdef WORDS_BIGENDIAN
#define A52_FMT32 AFMT_S32_BE
#define A52_FMT24 AFMT_S24_BE
#else
#define A52_FMT32 AFMT_S32_LE
#define A52_FMT24 AFMT_S24_LE
#endif
  sh->afmt=bps2afmt(2);
  if(af_query_fmt(sh->afilter,mpaf_format_e(AFMT_FLOAT32)) == MPXP_Ok||
     af_query_fmt(sh->afilter,mpaf_format_e(A52_FMT32)) == MPXP_Ok ||
     af_query_fmt(sh->afilter,mpaf_format_e(A52_FMT24)) == MPXP_Ok) {
    sh->afmt=AFMT_FLOAT32;
  }
  sh->audio_out_minsize=mp_conf.ao_channels*afmt2bps(sh->afmt)*256*6;
  sh->audio_in_minsize=MAX_AC3_FRAME;
  ctx->sh=sh;
  return ctx;
}

MPXP_Rc init(ad_private_t *ctx)
{
    sh_audio_t* sh_audio = ctx->sh;
    sample_t level=1, bias=384;
    float pts;
    int flags=0;
    /* Dolby AC3 audio:*/
    mpxp_a52_accel = mpxp_context().mplayer_accel;
    mpxp_a52_state=a52_init (mpxp_a52_accel);
    if (mpxp_a52_state == NULL) {
	MSG_ERR("A52 init failed\n");
	return MPXP_False;
    }
    if(a52_fillbuff(ctx,&pts)<0){
	MSG_ERR("A52 sync failed\n");
	return MPXP_False;
    }
    /* 'a52 cannot upmix' hotfix:*/
    a52_printinfo(ctx->sh);
    sh_audio->nch=mp_conf.ao_channels;
    while(sh_audio->nch>0){
	switch(sh_audio->nch){
	    case 1: mpxp_a52_flags=A52_MONO; break;
/*	    case 2: mpxp_a52_flags=A52_STEREO; break; */
	    case 2: mpxp_a52_flags=A52_DOLBY; break;
/*	    case 3: mpxp_a52_flags=A52_3F; break;*/
	    case 3: mpxp_a52_flags=A52_2F1R; break;
	    case 4: mpxp_a52_flags=A52_2F2R; break; /* 2+2*/
	    case 5: mpxp_a52_flags=A52_3F2R; break;
	    case 6: mpxp_a52_flags=A52_3F2R|A52_LFE; break; /* 5.1*/
	}
	/* test:*/
	flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
	MSG_V("A52 flags before a52_frame: 0x%X\n",flags);
	if (a52_frame (mpxp_a52_state, (uint8_t*)sh_audio->a_in_buffer, &flags, &level, bias)){
	    MSG_ERR("a52: error decoding frame -> nosound\n");
	    return MPXP_False;
	}
	MSG_V("A52 flags after a52_frame: 0x%X\n",flags);
	/* frame decoded, let's init resampler:*/
	if(afmt2bps(sh_audio->afmt)==4) {
	    if(a52_resample_init_float(mpxp_a52_state,mpxp_a52_accel,flags,sh_audio->nch)) break;
	} else {
	    if(a52_resample_init(mpxp_a52_state,mpxp_a52_accel,flags,sh_audio->nch)) break;
	}
	--sh_audio->nch; /* try to decrease no. of channels*/
    }
    if(sh_audio->nch<=0){
	MSG_ERR("a52: no resampler. try different channel setup!\n");
	return MPXP_False;
    }
    return MPXP_Ok;
}

void uninit(ad_private_t *ctx)
{
    delete ctx;
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
    sample_t level=1, bias=384;
    int flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
    unsigned i;
    unsigned len=0;
    UNUSED(minlen);
    UNUSED(maxlen);
    if(!sh_audio->a_in_buffer_len) {
	if(a52_fillbuff(ctx,pts)<0) return len; /* EOF */
    } else *pts=ctx->last_pts;
    sh_audio->a_in_buffer_len=0;
    if (a52_frame (mpxp_a52_state, (uint8_t*)sh_audio->a_in_buffer, &flags, &level, bias)){
	MSG_WARN("a52: error decoding frame\n");
	return len;
    }
//	a52_dynrng(&mpxp_a52_state, NULL, NULL);
    len=0;
    for (i = 0; i < 6; i++) {
	if (a52_block (mpxp_a52_state)){
	    MSG_WARN("a52: error at resampling\n");
	    break;
	}
	if(afmt2bps(sh_audio->afmt)==4)
	    len+=4*a52_resample32(a52_samples(mpxp_a52_state),(float *)&buf[len]);
	else
	    len+=2*a52_resample(a52_samples(mpxp_a52_state),(int16_t *)&buf[len]);
    }
    return len;
}
