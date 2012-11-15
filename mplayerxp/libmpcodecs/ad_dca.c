#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "codecs_ld.h"

#include "mp_config.h"
#include "mplayerxp.h"
#include "help_mp.h"
#include "osdep/cpudetect.h"

#include "libdca/dca.h"
#include "osdep/mm_accel.h"
#include "mplayerxp.h"
#include "osdep/bswap.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "postproc/af.h"
#include "osdep/mplib.h"

#define MAX_AC5_FRAME 4096

dca_state_t* mpxp_dca_state;
uint32_t mpxp_dca_accel=0;
uint32_t mpxp_dca_flags=0;

typedef struct priv_s {
    float last_pts;
}priv_t;


static const ad_info_t info = {
    "DTS Coherent Acoustics",
    "libdca",
    "Nickols_K",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(dca)

static const audio_probe_t probes[] = {
    { "libdca", "libdca", 0x86,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", 0x2001, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','B'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','C'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','E'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','H'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','L'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(sh_audio_t* sh,uint32_t wtag) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}

int dca_fillbuff(sh_audio_t *sh_audio,float *pts){
    int length=0,flen=0;
    int flags=0;
    int sample_rate=0;
    int bit_rate=0;
    float apts=0.,null_pts;
    priv_t *priv=sh_audio->context;

    sh_audio->a_in_buffer_len=0;
    /* sync frame:*/
    while(1){
	while(sh_audio->a_in_buffer_len<16){
	    int c=demux_getc_r(sh_audio->ds,apts?&null_pts:&apts);
	    if(c<0) { priv->last_pts=*pts=apts; return -1; } /* EOF*/
	    sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
	}
	length = dca_syncinfo (mpxp_dca_state,sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate, &flen);
	if(length>=16 && length<=MAX_AC5_FRAME) break; /* we're done.*/
	/* bad file => resync*/
	memmove(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,15);
	--sh_audio->a_in_buffer_len;
	apts=0;
    }
    MSG_DBG2("dca[%08X]: len=%d  flags=0x%X  %d Hz %d bit/s frame=%u\n",*((long *)sh_audio->a_in_buffer),length,flags,sample_rate,bit_rate,flen);
    sh_audio->rate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    demux_read_data_r(sh_audio->ds,sh_audio->a_in_buffer+16,length-16,apts?&null_pts:&apts);
    priv->last_pts=*pts=apts;

    return length;
}

/* returns: number of available channels*/
static int dca_printinfo(sh_audio_t *sh_audio){
    int flags, sample_rate, bit_rate,flen,length;
    char* mode="unknown";
    int channels=0;
    length=dca_syncinfo (mpxp_dca_state,sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate,&flen);
    switch(flags&DCA_CHANNEL_MASK){
	case DCA_CHANNEL: mode="channel"; channels=2; break;
	case DCA_MONO: mode="mono"; channels=1; break;
	case DCA_STEREO: mode="stereo"; channels=2; break;
	case DCA_3F: mode="3f"; channels=3;break;
	case DCA_2F1R: mode="2f+1r"; channels=3;break;
	case DCA_3F1R: mode="3f+1r"; channels=4;break;
	case DCA_2F2R: mode="2f+2r"; channels=4;break;
	case DCA_3F2R: mode="3f+2r"; channels=5;break;
	case DCA_4F2R: mode="4f+2r"; channels=6;break;
	case DCA_DOLBY: mode="dolby"; channels=2; break;
	default: channels=0; break;
    }
    MSG_INFO("DCA: %d.%d (%s%s)  %d Hz  %3.1f kbit/s Out: %u-bit\n",
	channels, (flags&DCA_LFE)?1:0,
	mode, (flags&DCA_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f,
	afmt2bps(sh_audio->afmt)*8);
    return (flags&DCA_LFE) ? (channels+1) : channels;
}


MPXP_Rc preinit(sh_audio_t *sh)
{
    /*	DTS audio:
	however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
#ifdef WORDS_BIGENDIAN
#define DCA_FMT32 AFMT_S32_BE
#define DCA_FMT24 AFMT_S24_BE
#else
#define DCA_FMT32 AFMT_S32_LE
#define DCA_FMT24 AFMT_S24_LE
#endif
    sh->afmt=bps2afmt(2);
    if(	af_query_fmt(sh->afilter,AFMT_FLOAT32) == MPXP_Ok||
	af_query_fmt(sh->afilter,DCA_FMT32) == MPXP_Ok ||
	af_query_fmt(sh->afilter,DCA_FMT24) == MPXP_Ok)
    {
	sh->afmt=AFMT_FLOAT32;
    }
    sh->audio_out_minsize=mp_conf.ao_channels*afmt2bps(sh->afmt)*256*8;
    sh->audio_in_minsize=MAX_AC5_FRAME;
    sh->context=mp_malloc(sizeof(priv_t));
    return MPXP_Ok;
}

MPXP_Rc init(sh_audio_t *sh_audio)
{
    sample_t level=1, bias=384;
    float pts;
    int flags=0;
    /* Dolby AC3 audio:*/
    mpxp_dca_accel = MPXPCtx->mplayer_accel;
    mpxp_dca_state = dca_init(mpxp_dca_accel);
    if (mpxp_dca_state == NULL) {
	MSG_ERR("dca init failed\n");
	return MPXP_False;
    }
    if(dca_fillbuff(sh_audio,&pts)<0){
	MSG_ERR("dca sync failed\n");
	return MPXP_False;
    }
    /* 'dca cannot upmix' hotfix:*/
    dca_printinfo(sh_audio);
    sh_audio->nch=mp_conf.ao_channels;
    while(sh_audio->nch>0){
	switch(sh_audio->nch){
	    case 1: mpxp_dca_flags=DCA_MONO; break;
	    case 2: mpxp_dca_flags=DCA_STEREO; break;
/*	    case 2: mpxp_dca_flags=DCA_DOLBY; break; */
/*	    case 3: mpxp_dca_flags=DCA_3F; break; */
	    case 3: mpxp_dca_flags=DCA_2F1R; break;
	    case 4: mpxp_dca_flags=DCA_2F2R; break; /* 2+2*/
	    case 5: mpxp_dca_flags=DCA_3F2R; break;
	    case 6: mpxp_dca_flags=DCA_3F2R|DCA_LFE; break; /* 5.1*/
	}
	/* test:*/
	flags=mpxp_dca_flags|DCA_ADJUST_LEVEL;
	MSG_V("dca flags before dca_frame: 0x%X\n",flags);
	if (dca_frame (mpxp_dca_state, sh_audio->a_in_buffer, &flags, &level, bias)){
	    MSG_ERR("dca: error decoding frame -> nosound\n");
	    return MPXP_False;
	}
	MSG_V("dca flags after dca_frame: 0x%X\n",flags);
	if(afmt2bps(sh_audio->afmt)==4) {
	    if(dca_resample_init_float(mpxp_dca_state,mpxp_dca_accel,flags,sh_audio->nch)) break;
	} else {
	    if(dca_resample_init(mpxp_dca_state,mpxp_dca_accel,flags,sh_audio->nch)) break;
	}
	--sh_audio->nch; /* try to decrease no. of channels*/
    }
    if(sh_audio->nch<=0){
	MSG_ERR("dca: no resampler. try different channel setup!\n");
	return MPXP_False;
    }
    return MPXP_Ok;
}

void uninit(sh_audio_t *sh)
{
    mp_free(sh->context);
}

MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_RESYNC_STREAM:
	    sh->a_in_buffer_len=0;   // reset ACM/DShow audio buffer
	    return MPXP_True;
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    dca_fillbuff(sh,&pts); // skip AC3 frame
	    return MPXP_True;
	}
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    sample_t level=1, bias=384;
    unsigned i,nblocks,flags=mpxp_dca_flags|DCA_ADJUST_LEVEL;
    unsigned len=0;
    priv_t *priv=sh_audio->context;
    UNUSED(minlen);
    UNUSED(maxlen);
	if(!sh_audio->a_in_buffer_len) {
	    if(dca_fillbuff(sh_audio,pts)<0) return len; /* EOF */
	}
	else *pts=priv->last_pts;
	sh_audio->a_in_buffer_len=0;
	if (dca_frame (mpxp_dca_state, sh_audio->a_in_buffer, &flags, &level, bias)!=0){
	    MSG_WARN("dca: error decoding frame\n");
	    return len;
	}
//	dca_dynrng(&mpxp_dca_state, NULL, NULL);
	len=0;
	nblocks=dca_blocks_num(mpxp_dca_state);
	for (i = 0; i < nblocks; i++) {
	    if (dca_block (mpxp_dca_state)){
		MSG_WARN("dca: error at deblock\n");
		break;
	    }
	    if(afmt2bps(sh_audio->afmt)==4)
		len+=4*dca_resample32(dca_samples(mpxp_dca_state),(float *)&buf[len]);
	    else
		len+=2*dca_resample(dca_samples(mpxp_dca_state),(int16_t *)&buf[len]);
	}
  return len;
}
