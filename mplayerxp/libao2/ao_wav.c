/*
 * PCM audio output driver
 *
 * This file is part of MPlayerXP.
 *
 * MPlayer is free software; you can redistribute it and/or modify
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

#include "mp_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bswap.h"
#include "postproc/af_format.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
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

extern int vo_pts;

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

typedef struct wav_priv_s {
    char *		out_filename;
    int			pcm_waveheader;
    int			fast;

    uint64_t		data_length;
    FILE *		fp;
}wav_priv_t;
static wav_priv_t wav = { NULL, 1, 0, 0, NULL  };

/* init with default values */
static struct WaveHeader wavhdr;

// to set/get/query special features/parameters
static int control(int cmd,long arg){
    UNUSED(cmd);
    UNUSED(arg);
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(unsigned flags) {
    // set defaults
    UNUSED(flags);
    wav.pcm_waveheader = 1;
    return 1;
}

static int configure(unsigned rate,unsigned channels,unsigned format){
    unsigned bits;
    char str[256];

    if(ao_subdevice)	wav.out_filename = ao_subdevice;
    else		wav.out_filename = strdup("mpxp_adump.wav");

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

    ao_data.outburst = 65536;
    ao_data.buffersize= 2*65536;
    ao_data.channels=channels;
    ao_data.samplerate=rate;
    ao_data.format=format;
    ao_data.bps=channels*rate*(bits/8);

    wavhdr.riff = le2me_32(WAV_ID_RIFF);
    wavhdr.wave = le2me_32(WAV_ID_WAVE);
    wavhdr.fmt = le2me_32(WAV_ID_FMT);
    wavhdr.fmt_length = le2me_32(16);
    wavhdr.fmt_tag = le2me_16(format == AFMT_FLOAT32 ? WAV_ID_FLOAT_PCM : WAV_ID_PCM);
    wavhdr.channels = le2me_16(ao_data.channels);
    wavhdr.sample_rate = le2me_32(ao_data.samplerate);
    wavhdr.bytes_per_second = le2me_32(ao_data.bps);
    wavhdr.bits = le2me_16(bits);
    wavhdr.block_align = le2me_16(ao_data.channels * (bits / 8));

    wavhdr.data = le2me_32(WAV_ID_DATA);
    wavhdr.data_length=le2me_32(0x7ffff000);
    wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

    MSG_INFO("ao_wav: %s %d-%s %s\n"
		,wav.out_filename
		,rate, (channels > 1) ? "Stereo" : "Mono", fmt2str(format,ao_data.bps,str,sizeof(str)));

    wav.fp = fopen(wav.out_filename, "wb");
    if(wav.fp) {
	if(wav.pcm_waveheader){ /* Reserve space for wave header */
	    fwrite(&wavhdr,sizeof(wavhdr),1,wav.fp);
	}
	return 1;
    }
    MSG_ERR("ao_wav: can't open output file: %s\n", wav.out_filename);
    return 0;
}

// close audio device
static void uninit(void){
    if(wav.pcm_waveheader){ /* Rewrite wave header */
	int broken_seek = 0;
#ifdef __MINGW32__
	// Windows, in its usual idiocy "emulates" seeks on pipes so it always looks
	// like they work. So we have to detect them brute-force.
	broken_seek = GetFileType((HANDLE)_get_osfhandle(_fileno(wav.fp))) != FILE_TYPE_DISK;
#endif
	if (broken_seek || fseek(wav.fp, 0, SEEK_SET) != 0)
	    MSG_ERR("Could not seek to start, WAV size headers not updated!\n");
	else if (wav.data_length > 0x7ffff000)
	    MSG_ERR("File larger than allowed for WAV files, may play truncated!\n");
	else {
	    wavhdr.file_length = wav.data_length + sizeof(wavhdr) - 8;
	    wavhdr.file_length = le2me_32(wavhdr.file_length);
	    wavhdr.data_length = le2me_32(wav.data_length);
	    fwrite(&wavhdr,sizeof(wavhdr),1,wav.fp);
	}
    }
    fclose(wav.fp);
    if (wav.out_filename)
	free(wav.out_filename);
    wav.out_filename = NULL;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static unsigned get_space(void){
    if(vo_pts)
	return ao_data.pts < vo_pts + wav.fast * 30000 ? ao_data.outburst : 0;
    return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned play(void* data,unsigned len,unsigned flags){
    UNUSED(flags);
    fwrite(data,len,1,wav.fp);
    if(wav.pcm_waveheader)
	wav.data_length += len;

    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){
    return 0.0;
}
