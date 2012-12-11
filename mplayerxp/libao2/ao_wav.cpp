#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * PCM audio output driver
 *
 * This file is part of MPlayerXP.
 *
 * MPlayer is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmpcore/xmp_core.h"

#include "osdep/bswap.h"
#include "postproc/af.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libvo2/video_out.h"
#include "help_mp.h"
#include "ao_msg.h"

#ifdef __MINGW32__
// for GetFileType to detect pipes
#include <windows.h>
#endif

static const ao_info_t info =
{
    "RAW WAVE file writer audio output",
    "wav",
    "Atmosfear",
    ""
};

LIBAO_EXTERN(wav)

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001
#define WAV_ID_FLOAT_PCM  0x0003

struct WaveHeader
{
    uint32_t riff;
    uint32_t file_length;
    uint32_t wave;
    uint32_t fmt;
    uint32_t fmt_length;
    uint16_t fmt_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t block_align;
    uint16_t bits;
    uint32_t data;
    uint32_t data_length;
};

typedef struct priv_s {
    char *		out_filename;
    int			pcm_waveheader;
    int			fast;

    uint64_t		data_length;
    FILE *		fp;
    struct WaveHeader	wavhdr;
}priv_t;

/* init with default values */

// to set/get/query special features/parameters
static MPXP_Rc control_ao(const ao_data_t* ao,int cmd,long arg){
    UNUSED(ao);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_False;
}

// open & setup audio device
// return: 1=success 0=fail
static MPXP_Rc init(ao_data_t* ao,unsigned flags) {
    // set defaults
    UNUSED(flags);
    priv_t* priv;
    priv=new(zeromem) priv_t;
    ao->priv=priv;
    priv->pcm_waveheader=1;
    return MPXP_Ok;
}

static MPXP_Rc config_ao(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format){
    priv_t* priv=reinterpret_cast<priv_t*>(ao->priv);
    unsigned bits;

    if(ao->subdevice)	priv->out_filename = ao->subdevice;
    else		priv->out_filename = mp_strdup("mpxp_adump.wav");

    bits=8;
    switch(format){
    case AFMT_S32_BE:
	format=AFMT_S32_LE;
    case AFMT_S32_LE:
	bits=32;
	break;
    case AFMT_FLOAT32:
	bits=32;
	break;
    case AFMT_S8:
	format=AFMT_U8;
    case AFMT_U8:
	break;
    case AFMT_AC3:
	bits=16;
	break;
    default:
	format=AFMT_S16_LE;
	bits=16;
	break;
    }

    ao->outburst = 65536;
    ao->buffersize= 2*65536;
    ao->channels=channels;
    ao->samplerate=rate;
    ao->format=format;
    ao->bps=channels*rate*(bits/8);

    priv->wavhdr.riff = le2me_32(WAV_ID_RIFF);
    priv->wavhdr.wave = le2me_32(WAV_ID_WAVE);
    priv->wavhdr.fmt = le2me_32(WAV_ID_FMT);
    priv->wavhdr.fmt_length = le2me_32(16);
    priv->wavhdr.fmt_tag = le2me_16(format == AFMT_FLOAT32 ? WAV_ID_FLOAT_PCM : WAV_ID_PCM);
    priv->wavhdr.channels = le2me_16(ao->channels);
    priv->wavhdr.sample_rate = le2me_32(ao->samplerate);
    priv->wavhdr.bytes_per_second = le2me_32(ao->bps);
    priv->wavhdr.bits = le2me_16(bits);
    priv->wavhdr.block_align = le2me_16(ao->channels * (bits / 8));

    priv->wavhdr.data = le2me_32(WAV_ID_DATA);
    priv->wavhdr.data_length=le2me_32(0x7ffff000);
    priv->wavhdr.file_length = priv->wavhdr.data_length + sizeof(priv->wavhdr) - 8;

    MSG_INFO("ao_wav: %s %d-%s %s\n"
		,priv->out_filename
		,rate
		,(channels > 1) ? "Stereo" : "Mono"
		,afmt2str(format));

    priv->fp = fopen(priv->out_filename, "wb");
    if(priv->fp) {
	if(priv->pcm_waveheader){ /* Reserve space for wave header */
	    fwrite(&priv->wavhdr,sizeof(priv->wavhdr),1,priv->fp);
	}
	return MPXP_Ok;
    }
    MSG_ERR("ao_wav: can't open output file: %s\n", priv->out_filename);
    return MPXP_False;
}

// close audio device
static void uninit(ao_data_t* ao){
    priv_t* priv=reinterpret_cast<priv_t*>(ao->priv);
    if(priv->pcm_waveheader){ /* Rewrite wave header */
	int broken_seek = 0;
#ifdef __MINGW32__
	// Windows, in its usual idiocy "emulates" seeks on pipes so it always looks
	// like they work. So we have to detect them brute-force.
	broken_seek = GetFileType((HANDLE)_get_osfhandle(_fileno(priv->fp))) != FILE_TYPE_DISK;
#endif
	if (broken_seek || fseek(priv->fp, 0, SEEK_SET) != 0)
	    MSG_ERR("Could not seek to start, WAV size headers not updated!\n");
	else if (priv->data_length > 0x7ffff000)
	    MSG_ERR("File larger than allowed for WAV files, may play truncated!\n");
	else {
	    priv->wavhdr.file_length = priv->data_length + sizeof(priv->wavhdr) - 8;
	    priv->wavhdr.file_length = le2me_32(priv->wavhdr.file_length);
	    priv->wavhdr.data_length = le2me_32(priv->data_length);
	    fwrite(&priv->wavhdr,sizeof(priv->wavhdr),1,priv->fp);
	}
    }
    fclose(priv->fp);
    if (priv->out_filename)
	delete priv->out_filename;
    delete priv;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(ao_data_t* ao){
    UNUSED(ao);
}

// stop playing, keep buffers (for pause)
static void audio_pause(ao_data_t* ao)
{
    // for now, just call reset();
    reset(ao);
}

// resume playing, after audio_pause()
static void audio_resume(ao_data_t* ao)
{
    UNUSED(ao);
}

// return: how many bytes can be played without blocking
static unsigned get_space(const ao_data_t* ao){
    priv_t* priv=reinterpret_cast<priv_t*>(ao->priv);
    float pts=dae_played_frame(mpxp_context().engine().xp_core->video).v_pts;
    if(pts)
	return ao->pts < pts + priv->fast * 30000 ? ao->outburst : 0;
    return ao->outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned play(ao_data_t* ao,const any_t* data,unsigned len,unsigned flags){
    priv_t* priv=reinterpret_cast<priv_t*>(ao->priv);
    UNUSED(flags);
    fwrite(data,len,1,priv->fp);
    if(priv->pcm_waveheader)
	priv->data_length += len;

    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(const ao_data_t* ao){
    UNUSED(ao);
    return 0.0;
}
