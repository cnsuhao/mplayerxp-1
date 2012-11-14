/*
 * aRts (KDE analogue Real-Time synthesizer) audio output driver for MPlayerXP
 *
 * copyright (c) 2002 Michele Balistreri <brain87@gmx.net>
 *
 * This file is part of MPlayer.
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

#include <artsc.h>
#include <stdio.h>

#include "mp_config.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "ao_msg.h"

/* Feel mp_free to experiment with the following values: */
#define ARTS_PACKETS 10 /* Number of audio packets */
#define ARTS_PACKET_SIZE_LOG2 11 /* Log2 of audio packet size */


static const ao_info_t info =
{
    "aRts audio output",
    "arts",
    "Michele Balistreri <brain87@gmx.net>",
    ""
};

LIBAO_EXTERN(arts)

static MPXP_Rc control(const ao_data_t* ao,int cmd, long arg)
{
    UNUSED(ao);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static MPXP_Rc init(ao_data_t* ao,unsigned flags)
{
    int err;
    UNUSED(ao);
    UNUSED(flags);

    if( (err=arts_init()) ) {
	MSG_ERR("[aRts] init failed: %s\n", arts_error_text(err));
	/*TODO: system("artsd -l0");*/
	return MPXP_False;
    }
    MSG_INFO("[aRts] connected to server\n");
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ configure(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format)
{
    arts_stream_t stream;
    unsigned frag_spec,samplesize;
    /*
     * arts supports 8bit unsigned and 16bit signed sample formats
     * (16bit apparently in little endian format, even in the case
     * when artsd runs on a big endian cpu).
     *
     * Unsupported formats are translated to one of these two formats
     * using mplayer's audio filters.
     */
    switch (format) {
	case AFMT_U8:
	case AFMT_S8:
	    format = AFMT_U8;
	    samplesize=1;
	    break;
#if 0
	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE:
	    format = AFMT_S24_LE;
	    samplesize=3;
	    break;
	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE:
	    format = AFMT_S32_LE;
	    samplesize=4;
	    break;
#endif
	default:
	    samplesize=2;
	    format = AFMT_S16_LE;    /* artsd always expects little endian?*/
	    break;
    }

    ao->format = format;
    ao->channels = channels;
    ao->samplerate = rate;
    ao->bps = rate*channels*samplesize;

    stream=arts_play_stream(rate, samplesize*8, channels, "MPlayerXP");
    ao->priv=stream;

    if(stream == NULL) {
	MSG_ERR("[aRts] Can't open stream\n");
	arts_free();
	return MPXP_False;
    }

    /* Set the stream to blocking: it will not block anyway, but it seems */
    /* to be working better */
    arts_stream_set(stream, ARTS_P_BLOCKING, 1);
    frag_spec = ARTS_PACKET_SIZE_LOG2 | ARTS_PACKETS << 16;
    arts_stream_set(stream, ARTS_P_PACKET_SETTINGS, frag_spec);
    ao->buffersize = arts_stream_get(stream, ARTS_P_BUFFER_SIZE);
    MSG_INFO("[aRts] Stream opened\n");

    MSG_V("[aRts] buffersize=%u\n",ao->buffersize);
    MSG_V("[aRts] buffersize=%u\n", arts_stream_get(stream, ARTS_P_PACKET_SIZE));

    return MPXP_Ok;
}

static void uninit(ao_data_t* ao)
{
    arts_stream_t stream=ao->priv;
    arts_close_stream(stream);
    arts_free();
}

static unsigned play(ao_data_t* ao,const any_t* data,unsigned len,unsigned flags)
{
    arts_stream_t stream=ao->priv;
    UNUSED(flags);
    return arts_write(stream, data, len);
}

static void audio_pause(ao_data_t* ao)
{
    UNUSED(ao);
}

static void audio_resume(ao_data_t* ao) { UNUSED(ao); }
static void reset(ao_data_t* ao) { UNUSED(ao); }

static unsigned get_space(const ao_data_t* ao)
{
    arts_stream_t stream=ao->priv;
    return arts_stream_get(stream, ARTS_P_BUFFER_SPACE);
}

static float get_delay(const ao_data_t* ao)
{
    arts_stream_t stream=ao->priv;
    return ((float) (ao->buffersize - arts_stream_get(stream,
		ARTS_P_BUFFER_SPACE))) / ((float) ao->bps);
}

