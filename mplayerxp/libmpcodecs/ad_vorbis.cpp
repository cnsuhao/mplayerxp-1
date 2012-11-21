#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include "mp_config.h"
#include "codecs_ld.h"
#include "ad_internal.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "osdep/mplib.h"
#include "osdep/bswap.h"

extern ao_data_t* ao_data;

static const ad_info_t info = {
    "Ogg/Vorbis audio decoder",
    "vorbis",
    "Felix Buenemann, A'rpi",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(vorbis)

static const audio_probe_t probes[] = {
    { "vorbis", "vorbis", 0x566F, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "vorbis", "vorbis", FOURCC_TAG('V','R','B','S'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(sh_audio_t* sh,uint32_t wtag) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}

#include <math.h>
#include <vorbis/codec.h>

// This struct is also defined in demux_ogg.c => common header ?
typedef struct priv_s {
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			    settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */
} priv_t;

static MPXP_Rc preinit(sh_audio_t *sh)
{
    if(!(sh->context=mp_malloc(sizeof(priv_t)))) return MPXP_False;
    sh->audio_out_minsize=1024*4; // 1024 samples/frame
    return MPXP_Ok;
}

static MPXP_Rc init(sh_audio_t *sh)
{
    ogg_packet op;
    vorbis_comment vc;
    priv_t *priv;
    float pts;

    /// Init the decoder with the 3 header packets
    priv = reinterpret_cast<priv_t*>(sh->context);
    vorbis_info_init(&priv->vi);
    vorbis_comment_init(&vc);
    op.bytes = ds_get_packet_r(sh->ds,&op.packet,&pts);
    op.b_o_s  = 1;
    /// Header
    if(vorbis_synthesis_headerin(&priv->vi,&vc,&op) <0) {
	MSG_ERR("OggVorbis: initial (identification) header broken!\n");
	mp_free(priv);
	return MPXP_False;
    }
    op.bytes = ds_get_packet_r(sh->ds,&op.packet,&pts);
    op.b_o_s  = 0;
    /// Comments
    if(vorbis_synthesis_headerin(&priv->vi,&vc,&op) <0) {
	MSG_ERR("OggVorbis: comment header broken!\n");
	mp_free(priv);
	return MPXP_False;
    }
    op.bytes = ds_get_packet_r(sh->ds,&op.packet,&pts);
    //// Codebook
    if(vorbis_synthesis_headerin(&priv->vi,&vc,&op)<0) {
	MSG_WARN("OggVorbis: codebook header broken!\n");
	mp_free(priv);
	return MPXP_False;
    } else { /// Print the infos
	char **ptr=vc.user_comments;
	while(*ptr) {
	    MSG_V("OggVorbisComment: %s\n",*ptr);
	    ++ptr;
	}
	MSG_V("OggVorbis: Bitstream is %d channel, %dHz, %dbit/s %cBR\n",(int)priv->vi.channels,(int)priv->vi.rate,(int)priv->vi.bitrate_nominal,
	    (priv->vi.bitrate_lower!=priv->vi.bitrate_nominal)||(priv->vi.bitrate_upper!=priv->vi.bitrate_nominal)?'V':'C');
	MSG_V("OggVorbis: Encoded by: %s\n",vc.vendor);
    }
    // Setup the decoder
    sh->nch=priv->vi.channels;
    sh->rate=priv->vi.rate;
#ifdef WORDS_BIGENDIAN
#define OGG_FMT32 AFMT_S32_BE
#define OGG_FMT24 AFMT_S24_BE
#define OGG_FMT16 AFMT_S16_BE
#else
#define OGG_FMT32 AFMT_S32_LE
#define OGG_FMT24 AFMT_S24_LE
#define OGG_FMT16 AFMT_S16_LE
#endif
    sh->afmt=OGG_FMT16;
    if(ao_control(ao_data,AOCONTROL_QUERY_FORMAT,OGG_FMT32) == MPXP_Ok) {
	sh->afmt=OGG_FMT32;
    }
    // assume 128kbit if bitrate not specified in the header
    sh->i_bps=((priv->vi.bitrate_nominal>0) ? priv->vi.bitrate_nominal : 128000)/8;
    sh->context = priv;

    /// Finish the decoder init
    vorbis_synthesis_init(&priv->vd,&priv->vi);
    vorbis_block_init(&priv->vd,&priv->vb);
    MSG_V("OggVorbis: Init OK!\n");

    return MPXP_Ok;
}

static void uninit(sh_audio_t *sh)
{
    mp_free(sh->context);
}

static MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(sh);
    UNUSED(cmd);
    UNUSED(arg);
    switch(cmd) {
#if 0
	case ADCTRL_RESYNC_STREAM:  return MPXP_True;
	case ADCTRL_SKIP_FRAME:  return MPXP_True;
#endif
    }
    return MPXP_Unknown;
}

static unsigned decode(sh_audio_t *sh,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
	unsigned len = 0;
	int samples;
	float **pcm;
	ogg_packet op;
	priv_t *priv = reinterpret_cast<priv_t*>(sh->context);
	op.b_o_s =  op.e_o_s = 0;
	while(len < minlen) {
	  /* if file contains audio only steam there is no pts */
	  op.bytes = ds_get_packet_r(sh->ds,&op.packet,pts);
	  if(!op.packet)
	    break;
	  if(vorbis_synthesis(&priv->vb,&op)==0) /* test for success! */
	    vorbis_synthesis_blockin(&priv->vd,&priv->vb);
	  while((samples=vorbis_synthesis_pcmout(&priv->vd,&pcm))>0){
	    unsigned i,j;
	    int clipflag=0;
	    int convsize=(maxlen-len)/(2*priv->vi.channels); // max size!
	    int bout=(samples<convsize?samples:convsize);

	    if(bout<=0) break;

	    if(afmt2bps(sh->afmt)==4) {
	    /* convert floats to 32 bit signed ints (host order) and
	       interleave */
	    for(i=0;i<(unsigned)priv->vi.channels;i++){
	      ogg_int32_t *convbuffer=(ogg_int32_t *)(&buf[len]);
	      ogg_int32_t *ptr=convbuffer+i;
	      float  *mono=pcm[i];
	      for(j=0;j<(unsigned)bout;j++){
#if 1
		int val=mono[j]*2147483647.f;
#else /* optional dither */
		int val=mono[j]*2147483647.f+drand48()-0.5f;
#endif
		/* might as well guard against clipping */
		if(val>2147483647){
		  val=2147483647;
		  clipflag=1;
		}
		if(val<-2147483647){
		  val=-2147483647;
		  clipflag=1;
		}
		*ptr=val;
		ptr+=priv->vi.channels;
	      }
	    }
	    }
	    else
	    {
	    /* convert floats to 16 bit signed ints (host order) and
	       interleave */
	    for(i=0;i<(unsigned)priv->vi.channels;i++){
	      ogg_int16_t *convbuffer=(ogg_int16_t *)(&buf[len]);
	      ogg_int16_t *ptr=convbuffer+i;
	      float  *mono=pcm[i];
	      for(j=0;j<(unsigned)bout;j++){
#if 1
		int val=mono[j]*32767.f;
#else /* optional dither */
		int val=mono[j]*32767.f+drand48()-0.5f;
#endif
		/* might as well guard against clipping */
		if(val>32767){
		  val=32767;
		  clipflag=1;
		}
		if(val<-32768){
		  val=-32768;
		  clipflag=1;
		}
		*ptr=val;
		ptr+=priv->vi.channels;
	      }
	    }
	    }

	    if(clipflag) { MSG_DBG2("Clipping in frame %ld\n",(long)(priv->vd.sequence)); }
	    len+=afmt2bps(sh->afmt)*priv->vi.channels*bout;
	    MSG_DBG2("\n[decoded: %d / %d ]\n",bout,samples);
	    vorbis_synthesis_read(&priv->vd,bout); /* tell libvorbis how
						    many samples we
						    actually consumed */
	  }
	}

  return len;
}

