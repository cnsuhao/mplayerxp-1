#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <string.h>
#include <stdio.h>

#include "mp_aframe.h"
#include "libao2/afmt.h"
#include "loader/wine/mmreg.h"
#include "mp_msg.h"

namespace mpxp {

enum { AFMT_AF_FLAGS=0x70000000 };

/* Decodes the format from mplayer format to libaf format */
mpaf_format_e __FASTCALL__ afmt2mpaf(unsigned ifmt)
{
    mpaf_format_e ofmt = mpaf_format_e(0);
    // Check input ifmt
    switch(ifmt){
	case AFMT_U8: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_US|1); break;
	case AFMT_S8: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_SI|1); break;
	case AFMT_S16_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_SI|2); break;
	case AFMT_S16_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_SI|2); break;
	case AFMT_U16_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_US|2); break;
	case AFMT_U16_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_US|2); break;
	case AFMT_S24_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_SI|3); break;
	case AFMT_S24_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_SI|3); break;
	case AFMT_U24_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_US|3); break;
	case AFMT_U24_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_US|3); break;
	case AFMT_S32_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_SI|4); break;
	case AFMT_S32_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_SI|4); break;
	case AFMT_U32_LE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_LE|MPAF_US|4); break;
	case AFMT_U32_BE: ofmt = mpaf_format_e(MPAF_PCM|MPAF_BE|MPAF_US|4); break;
	case AFMT_FLOAT32:ofmt = mpaf_format_e(MPAF_PCM|MPAF_F |MPAF_NE|4); break;

	case AFMT_IMA_ADPCM: ofmt = mpaf_format_e(MPAF_IMA_ADPCM|1); break;
	case AFMT_MPEG:      ofmt = mpaf_format_e(MPAF_MPEG2|1); break;
	case AFMT_AC3:       ofmt = mpaf_format_e(MPAF_AC3|1); break;
	default:
	    if ((ifmt & AFMT_AF_FLAGS) == AFMT_AF_FLAGS) {
		ofmt = mpaf_format_e((ifmt&(~AFMT_AF_FLAGS))|2);
		break;
	    }
	    //This can not happen ....
	    MSG_FATAL("[af_mp] Unrecognized input audio format %i\n",ifmt);
	break;
    }
    return ofmt;
}

/* Encodes the format from libaf format to mplayer (OSS) format */
unsigned __FASTCALL__ mpaf2afmt(mpaf_format_e fmt)
{
    switch(fmt&MPAF_SPECIAL_MASK) {
	case 0: // PCM:
	    if((fmt&MPAF_POINT_MASK)==MPAF_I) {
		if((fmt&MPAF_SIGN_MASK)==MPAF_SI){
		    // signed int PCM:
		    switch(fmt&MPAF_BPS_MASK){
			case MPAF_BPS_1: return AFMT_S8;
			default:
			case MPAF_BPS_2: return (fmt&MPAF_LE)?AFMT_S16_LE:AFMT_S16_BE;
			case MPAF_BPS_3: return (fmt&MPAF_LE)?AFMT_S24_LE:AFMT_S24_BE;
			case MPAF_BPS_4: return (fmt&MPAF_LE)?AFMT_S32_LE:AFMT_S32_BE;
		    }
		} else {
		    // unsigned int PCM:
		    switch(fmt&MPAF_BPS_MASK){
			case MPAF_BPS_1: return AFMT_U8;
			default:
			case MPAF_BPS_2: return (fmt&MPAF_LE)?AFMT_U16_LE:AFMT_U16_BE;
			case MPAF_BPS_3: return (fmt&MPAF_LE)?AFMT_U24_LE:AFMT_U24_BE;
			case MPAF_BPS_4: return (fmt&MPAF_LE)?AFMT_U32_LE:AFMT_U32_BE;
		    }
		}
	    } else {
		// float PCM:
		return AFMT_FLOAT32; // FIXME?
	    }
	    break;
	default:
	case MPAF_MPEG2:  return AFMT_MPEG;
	case MPAF_AC3:    return AFMT_AC3;
	case MPAF_IMA_ADPCM: return AFMT_IMA_ADPCM;
    }
    return fmt|AFMT_AF_FLAGS;
}

static const struct fmt_alias_s {
    const char *name;
    unsigned short wtag;
} fmt_aliases[]= {
    { "adpcm", WAVE_FORMAT_ADPCM },
    { "vselp", WAVE_FORMAT_VSELP },
    { "cvsd",  WAVE_FORMAT_IBM_CVSD },
    { "dts",   WAVE_FORMAT_DTS },
    { "oki_adpcm", WAVE_FORMAT_OKI_ADPCM },
    { "dvi_adpcm", WAVE_FORMAT_DVI_ADPCM },
    { "ima_adpcm", WAVE_FORMAT_IMA_ADPCM },
    { "mediaspace_adpcm", WAVE_FORMAT_MEDIASPACE_ADPCM },
    { "sierra_adpcm", WAVE_FORMAT_SIERRA_ADPCM },
    { "g723_adpcm", WAVE_FORMAT_G723_ADPCM },
    { "digistd",   WAVE_FORMAT_DIGISTD },
    { "digifix",   WAVE_FORMAT_DIGIFIX },
    { "dialogic_oki_adpcm", WAVE_FORMAT_DIALOGIC_OKI_ADPCM },
    { "mediavision_adpcm", WAVE_FORMAT_MEDIAVISION_ADPCM },
    { "cu_codec", WAVE_FORMAT_CU_CODEC },
    { "yamaha_adpcm", WAVE_FORMAT_YAMAHA_ADPCM },
    { "sonarc", WAVE_FORMAT_SONARC },
    { "dspgroup_truespeech", WAVE_FORMAT_DSPGROUP_TRUESPEECH },
    { "echosc1", WAVE_FORMAT_ECHOSC1 },
    { "audiofile_af36", WAVE_FORMAT_AUDIOFILE_AF36 },
    { "aptx", WAVE_FORMAT_APTX },
    { "audiofile_af10", WAVE_FORMAT_AUDIOFILE_AF10 },
    { "prosody_1612", WAVE_FORMAT_PROSODY_1612 },
    { "lrc", WAVE_FORMAT_LRC },
    { "dolby_ac2", WAVE_FORMAT_DOLBY_AC2 },
    { "gsm610", WAVE_FORMAT_GSM610 },
    { "msnaudio", WAVE_FORMAT_MSNAUDIO },
    { "antex_adpcme", WAVE_FORMAT_ANTEX_ADPCME },
    { "control_res_vqlpc", WAVE_FORMAT_CONTROL_RES_VQLPC },
    { "digireal", WAVE_FORMAT_DIGIREAL },
    { "digiadpcm", WAVE_FORMAT_DIGIADPCM },
    { "control_res_cr10", WAVE_FORMAT_CONTROL_RES_CR10 },
    { "nms_vbxadpcm", WAVE_FORMAT_NMS_VBXADPCM },
    { "cs_imaadpcm", WAVE_FORMAT_CS_IMAADPCM },
    { "echosc3", WAVE_FORMAT_ECHOSC3 },
    { "rockwell_adpcm", WAVE_FORMAT_ROCKWELL_ADPCM },
    { "rockwell_digitalk", WAVE_FORMAT_ROCKWELL_DIGITALK },
    { "xebec", WAVE_FORMAT_XEBEC },
    { "g721_adpcm", WAVE_FORMAT_G721_ADPCM },
    { "g728_celp", WAVE_FORMAT_G728_CELP },
    { "msg723", WAVE_FORMAT_MSG723 },
    { "mp2", WAVE_FORMAT_MPEG },
    { "rt24", WAVE_FORMAT_RT24 },
    { "pac", WAVE_FORMAT_PAC },
    { "mp3", WAVE_FORMAT_MPEGLAYER3 },
    { "lucent_g723", WAVE_FORMAT_LUCENT_G723 },
    { "cirrus", WAVE_FORMAT_CIRRUS },
    { "espcm", WAVE_FORMAT_ESPCM },
    { "voxware", WAVE_FORMAT_VOXWARE },
    { "canopus_atrac", WAVE_FORMAT_CANOPUS_ATRAC },
    { "g726_adpcm", WAVE_FORMAT_G726_ADPCM },
    { "g722_adpcm", WAVE_FORMAT_G722_ADPCM },
    { "dsat_display", WAVE_FORMAT_DSAT_DISPLAY },
    { "voxware_byte_aligned", WAVE_FORMAT_VOXWARE_BYTE_ALIGNED },
    { "voxware_ac8", WAVE_FORMAT_VOXWARE_AC8 },
    { "voxware_ac10", WAVE_FORMAT_VOXWARE_AC10 },
    { "voxware_ac16", WAVE_FORMAT_VOXWARE_AC16 },
    { "voxware_ac20", WAVE_FORMAT_VOXWARE_AC20 },
    { "voxware_rt24", WAVE_FORMAT_VOXWARE_RT24 },
    { "voxware_rt29", WAVE_FORMAT_VOXWARE_RT29 },
    { "voxware_rt29hw", WAVE_FORMAT_VOXWARE_RT29HW },
    { "voxware_vr12", WAVE_FORMAT_VOXWARE_VR12 },
    { "voxware_vr18", WAVE_FORMAT_VOXWARE_VR18 },
    { "voxware_tq40", WAVE_FORMAT_VOXWARE_TQ40 },
    { "softsound", WAVE_FORMAT_SOFTSOUND },
    { "voxware_tq60", WAVE_FORMAT_VOXWARE_TQ60 },
    { "msrt24", WAVE_FORMAT_MSRT24 },
    { "g729a", WAVE_FORMAT_G729A },
    { "mvi2", WAVE_FORMAT_MVI_MVI2 },
    { "df_g726", WAVE_FORMAT_DF_G726 },
    { "df_gsm610", WAVE_FORMAT_DF_GSM610 },
    { "isiaudio", WAVE_FORMAT_ISIAUDIO },
    { "onlive", WAVE_FORMAT_ONLIVE },
    { "sbc24", WAVE_FORMAT_SBC24 },
    { "dolby_ac3_spdif", WAVE_FORMAT_DOLBY_AC3_SPDIF },
    { "mediasonic_g723", WAVE_FORMAT_MEDIASONIC_G723 },
    { "prosody_8k", WAVE_FORMAT_PROSODY_8KBPS },
    { "zyxel_adpcm", WAVE_FORMAT_ZYXEL_ADPCM },
    { "philips_lpcbb", WAVE_FORMAT_PHILIPS_LPCBB },
    { "packed", WAVE_FORMAT_PACKED },
    { "malden_phonytalk", WAVE_FORMAT_MALDEN_PHONYTALK },
    { "phetorex_adpcm", WAVE_FORMAT_RHETOREX_ADPCM },
    { "irat", WAVE_FORMAT_IRAT },
    { "vivo_g723", WAVE_FORMAT_VIVO_G723 },
    { "vivo_siren", WAVE_FORMAT_VIVO_SIREN },
    { "digital_g723", WAVE_FORMAT_DIGITAL_G723 },
    { "sanyo_ld_adpcm", WAVE_FORMAT_SANYO_LD_ADPCM },
    { "siprolab_acelpnet", WAVE_FORMAT_SIPROLAB_ACEPLNET },
    { "siprolab_acelp4800", WAVE_FORMAT_SIPROLAB_ACELP4800 },
    { "siprolab_acelp8v3", WAVE_FORMAT_SIPROLAB_ACELP8V3 },
    { "siprolab_g729", WAVE_FORMAT_SIPROLAB_G729 },
    { "siprolab_g729a", WAVE_FORMAT_SIPROLAB_G729A },
    { "siprolab_kelvin", WAVE_FORMAT_SIPROLAB_KELVIN },
    { "g726adpcm", WAVE_FORMAT_G726ADPCM },
    { "qualcomm_purevoice", WAVE_FORMAT_QUALCOMM_PUREVOICE },
    { "qualcomm_halfrate", WAVE_FORMAT_QUALCOMM_HALFRATE },
    { "tubgsm", WAVE_FORMAT_TUBGSM },
    { "msaudio1", WAVE_FORMAT_MSAUDIO1 },
    { "creative_adpcm", WAVE_FORMAT_CREATIVE_ADPCM },
    { "creative_fastspeech8", WAVE_FORMAT_CREATIVE_FASTSPEECH8 },
    { "creative_fastspeech10", WAVE_FORMAT_CREATIVE_FASTSPEECH10 },
    { "uher_adpcm", WAVE_FORMAT_UHER_ADPCM },
    { "quarterdeck", WAVE_FORMAT_QUARTERDECK },
    { "ilink_vc", WAVE_FORMAT_ILINK_VC },
    { "raw_sport", WAVE_FORMAT_RAW_SPORT },
    { "ipi_hsx", WAVE_FORMAT_IPI_HSX },
    { "ipi_rpelp", WAVE_FORMAT_IPI_RPELP },
    { "cs2", WAVE_FORMAT_CS2 },
    { "sony_scx", WAVE_FORMAT_SONY_SCX },
    { "fm_towns_snd", WAVE_FORMAT_FM_TOWNS_SND },
    { "btv_digital", WAVE_FORMAT_BTV_DIGITAL },
    { "qdesign_music", WAVE_FORMAT_QDESIGN_MUSIC },
    { "vme_vmpcm", WAVE_FORMAT_VME_VMPCM },
    { "tpc", WAVE_FORMAT_TPC },
    { "oligsm", WAVE_FORMAT_OLIGSM },
    { "oliadpcm", WAVE_FORMAT_OLIADPCM },
    { "olicelp", WAVE_FORMAT_OLICELP },
    { "olisbc", WAVE_FORMAT_OLISBC },
    { "oliopr", WAVE_FORMAT_OLIOPR },
    { "lh_codec", WAVE_FORMAT_LH_CODEC },
    { "norris", WAVE_FORMAT_NORRIS },
    { "soundspace_musicompress", WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS },
    { "ac3", WAVE_FORMAT_DVM }
};

// Convert from string to format
mpaf_format_e mpaf_str2fmt(const char *str)
{
    unsigned i,fmt;
    char val[3];
    fmt=0;
    if(strlen(str)<3) goto bad_fmt;
    /* check for special cases */
    if(strncmp(str,"float",5)==0 || strncmp(str,"FLOAT",5)==0) { fmt |= MPAF_F; str = &str[4]; }
    else
    if(str[0]=='S' || str[0]=='s') fmt|=MPAF_SI;
    else
    if(str[0]=='U' || str[0]=='u') fmt|=MPAF_US;
    else goto try_special;
    val[0]=str[1];
    val[1]=str[2];
    val[2]='\0';
    if(strcmp(val,"08")==0) fmt|=1;
    else
    if(strcmp(val,"16")==0) fmt|=2;
    else
    if(strcmp(val,"24")==0) fmt|=3;
    else
    if(strcmp(val,"32")==0) fmt|=4;
    else
    if(strcmp(val,"64")==0) fmt|=8;
    else goto try_special;
    if(str[3]=='\0') {
#ifdef WORDS_BIGENDIAN
	fmt|=MPAF_BE;
#else
	fmt|=MPAF_LE;
#endif
    }
    else if(strcmp(&str[3],"LE")==0 || strcmp(&str[3],"le")==0) fmt |= MPAF_LE;
    else if(strcmp(&str[3],"BE")==0 || strcmp(&str[3],"be")==0) fmt |= MPAF_BE;
    else goto try_special;
    return mpaf_format_e(fmt);

    try_special:
    for(i=0;i<sizeof(fmt_aliases)/sizeof(struct fmt_alias_s);i++) {
	if(strcasecmp(str,fmt_aliases[i].name)==0) return mpaf_format_e(fmt_aliases[i].wtag<<16);
    }
    bad_fmt:
    MSG_ERR("[af_format] Bad value %s. Examples: S08LE U24BE S32 MP3 AC3\n",str);
    return mpaf_format_e(MPAF_BE);
}

/* Convert format to str input str is a buffer for the
   converted string, size is the size of the buffer */
char* mpaf_fmt2str(mpaf_format_e format, char* str, size_t size)
{
    int i=0;
    // Print endinaness

    if(format & MPAF_SPECIAL_MASK) {
	unsigned short wtag;
	unsigned j;
	wtag = format >> 16;
	for(j=0;j<sizeof(fmt_aliases)/sizeof(struct fmt_alias_s);j++) {
	    if(fmt_aliases[j].wtag==wtag) {
		i+=snprintf(&str[i],size-i,fmt_aliases[j].name);
		break;
	    }
	}
    } else {
	// Type
	if(MPAF_F == (format & MPAF_POINT_MASK)) i+=snprintf(&str[i],size,"FLOAT");
	else {
	    if(MPAF_US == (format & MPAF_SIGN_MASK)) i+=snprintf(&str[i],size-i,"U");
	    else i+=snprintf(&str[i],size-i,"S");
	}
	// size
	i+=snprintf(&str[i],size,"%d",(format&MPAF_BPS_MASK)*8);
	// endian
	if(MPAF_LE == (format & MPAF_END_MASK)) i+=snprintf(&str[i],size,"LE");
	else i+=snprintf(&str[i],size,"BE");
    }
    return str;
}


mp_aframe_t* new_mp_aframe(unsigned rate,unsigned nch,mpaf_format_e format,unsigned xp_idx) {
    mp_aframe_t*  mpaf = new(zeromem) mp_aframe_t;
    if(!mpaf) return NULL;
    mpaf->rate = rate;
    mpaf->nch = nch;
    mpaf->format = format;
    mpaf->xp_idx = xp_idx;
    return mpaf;
}

int free_mp_aframe(mp_aframe_t* mpaf) {
    if(!mpaf) return 0;
    if(mpaf->audio) delete mpaf->audio;
    delete mpaf;
    return 1;
}

mp_aframe_t* new_mp_aframe_genome(const mp_aframe_t* in) {
    mp_aframe_t* out = new(zeromem) mp_aframe_t;
    memcpy(out,in,sizeof(mp_aframe_t));
    out->audio = NULL;
    return out;
}

void mp_alloc_aframe(mp_aframe_t* it) { it->audio = mp_malloc(it->len); }

} // namespace mpxp