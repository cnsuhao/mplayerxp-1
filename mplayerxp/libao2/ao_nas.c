/*
 * NAS audio output driver
 *
 * copyright (c) 2001 Tobias Diedrich <ranma@gmx.at>
 *
 * Based on the libaudiooss parts rewritten by me, which were
 * originally based on the NAS output plugin for XMMS.
 *
 * XMMS plugin by Willem Monsuwe
 * adapted for libaudiooss by Jon Trulson
 * further modified by Erik Inge Bolsø
 * largely rewritten and used for this ao driver by Tobias Diedrich
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

/*
 * Theory of operation:
 *
 * The NAS consists of two parts, a server daemon and a client.
 * We setup the server to use a buffer of size bytes_per_second
 * with a low watermark of buffer_size - NAS_FRAG_SIZE.
 * Upon starting the flow the server will generate a buffer underrun
 * event and the event handler will fill the buffer for the first time.
 * Now the server will generate a lowwater event when the server buffer
 * falls below the low watermark value. The event handler gets called
 * again and refills the buffer by the number of bytes requested by the
 * server (usually a multiple of 4096). To prevent stuttering on
 * startup (start of playing, seeks, unpausing) the client buffer should
 * be bigger than the server buffer. (For debugging we also do some
 * accounting of what we think how much of the server buffer is filled)
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <audio/audiolib.h>

#include "mp_config.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "osdep/mplib.h"
#include "afmt.h"
#include "ao_msg.h"

/* NAS_FRAG_SIZE must be a power-of-two value */
#define NAS_FRAG_SIZE 4096

static const char * const nas_event_types[] = {
    "Undefined",
    "Undefined",
    "ElementNotify",
    "GrabNotify",
    "MonitorNotify",
    "BucketNotify",
    "DeviceNotify"
};

static const char * const nas_elementnotify_kinds[] = {
    "LowWater",
    "HighWater",
    "State",
    "Unknown"
};

static const char * const nas_states[] = {
    "Stop",
    "Start",
    "Pause",
    "Any"
};

static const char * const nas_reasons[] = {
    "User",
    "Underrun",
    "Overrun",
    "EOF",
    "Watermark",
    "Hardware",
    "Any"
};

static const char* nas_reason(unsigned int reason)
{
    if (reason > 6) reason = 6;
    return nas_reasons[reason];
}

static const char* nas_elementnotify_kind(unsigned int kind)
{
    if (kind > 2) kind = 3;
    return nas_elementnotify_kinds[kind];
}

static const char* nas_event_type(unsigned int type) {
    if (type > 6) type = 0;
    return nas_event_types[type];
}

static const char* nas_state(unsigned int state) {
    if (state>3) state = 3;
    return nas_states[state];
}

static const ao_info_t info = {
    "NAS audio output",
    "nas",
    "Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
    ""
};

typedef struct priv_s {
    AuServer	*aud;
    AuFlowID	flow;
    AuDeviceID	dev;
    AuFixedPoint	gain;

    unsigned int state;
    int expect_underrun;

    char *client_buffer;
    char *server_buffer;
    unsigned int client_buffer_size;
    unsigned int client_buffer_used;
    unsigned int server_buffer_size;
    unsigned int server_buffer_used;
    pthread_mutex_t buffer_mutex;

    pthread_t event_thread;
    int stop_thread;
}priv_t;

LIBAO_EXTERN(nas)

static void nas_print_error(AuServer *aud, const char *prefix, AuStatus as)
{
    char s[100];
    AuGetErrorText(aud, as, s, 100);
    MSG_ERR("ao_nas: %s: returned status %d (%s)\n", prefix, as, s);
}

static int nas_readBuffer(priv_t *priv, unsigned int num)
{
    AuStatus as;

    pthread_mutex_lock(&priv->buffer_mutex);
    MSG_DBG2("ao_nas: nas_readBuffer(): num=%d client=%d/%d server=%d/%d\n",
			num,
			priv->client_buffer_used, priv->client_buffer_size,
			priv->server_buffer_used, priv->server_buffer_size);

    if (priv->client_buffer_used == 0) {
	MSG_DBG2("ao_nas: buffer is empty, nothing read.\n");
	pthread_mutex_unlock(&priv->buffer_mutex);
	return 0;
    }
    if (num > priv->client_buffer_used)
	num = priv->client_buffer_used;

    /*
     * It is not appropriate to call AuWriteElement() here because the
     * buffer is locked and delays writing to the network will cause
     * other threads to block waiting for buffer_mutex.  Instead the
     * data is copied to "server_buffer" and written to the network
     * outside of the locked section of code.
     *
     * (Note: Rather than these two buffers, a single circular buffer
     *  could eliminate the memcpy/memmove steps.)
     */
    /* make sure we don't overflow the buffer */
    if (num > priv->server_buffer_size)
	num = priv->server_buffer_size;
    memcpy(priv->server_buffer, priv->client_buffer, num);

    priv->client_buffer_used -= num;
    priv->server_buffer_used += num;
    memmove(priv->client_buffer, priv->client_buffer + num, priv->client_buffer_used);
    pthread_mutex_unlock(&priv->buffer_mutex);

    /*
     * Now write the new buffer to the network.
     */
    AuWriteElement(priv->aud, priv->flow, 0, num, priv->server_buffer, AuFalse, &as);
    if (as != AuSuccess)
	nas_print_error(priv->aud, "nas_readBuffer(): AuWriteElement", as);

    return num;
}

static int nas_writeBuffer(priv_t *priv,const any_t*data, unsigned int len)
{
    pthread_mutex_lock(&priv->buffer_mutex);
    MSG_DBG2("ao_nas: nas_writeBuffer(): len=%d client=%d/%d server=%d/%d\n",
		len, priv->client_buffer_used, priv->client_buffer_size,
		priv->server_buffer_used, priv->server_buffer_size);

    /* make sure we don't overflow the buffer */
    if (len > priv->client_buffer_size - priv->client_buffer_used)
	len = priv->client_buffer_size - priv->client_buffer_used;
    memcpy(priv->client_buffer + priv->client_buffer_used, data, len);
    priv->client_buffer_used += len;

    pthread_mutex_unlock(&priv->buffer_mutex);

    return len;
}

static int nas_empty_event_queue(priv_t *priv)
{
    AuEvent ev;
    int result = 0;

    while (AuScanForTypedEvent(priv->aud, AuEventsQueuedAfterFlush,
				   AuTrue, AuEventTypeElementNotify, &ev)) {
	AuDispatchEvent(priv->aud, &ev);
	result = 1;
    }
    return result;
}

static any_t*nas_event_thread_start(any_t*data)
{
    priv_t *priv = data;

    do {
	MSG_DBG2(
	       "ao_nas: event thread heartbeat (state=%s)\n",
	       nas_state(priv->state));
	nas_empty_event_queue(priv);
	usleep(1000);
    } while (!priv->stop_thread);

    return NULL;
}

static AuBool nas_error_handler(AuServer* aud, AuErrorEvent* ev)
{
    char s[100];
    AuGetErrorText(aud, ev->error_code, s, 100);
    MSG_ERR( "ao_nas: error [%s]\n"
		"error_code: %d\n"
		"request_code: %d\n"
		"minor_code: %d\n",
		s,
		ev->error_code,
		ev->request_code,
		ev->minor_code);

    return AuTrue;
}

static AuBool nas_event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd)
{
	AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;
	priv_t *priv = hnd->data;

	MSG_DBG2("ao_nas: event_handler(): type %s kind %s state %s->%s reason %s numbytes %d expect_underrun %d\n",
		nas_event_type(event->type),
		nas_elementnotify_kind(event->kind),
		nas_state(event->prev_state),
		nas_state(event->cur_state),
		nas_reason(event->reason),
		(int)event->num_bytes,
		priv->expect_underrun);

	if (event->num_bytes > INT_MAX) {
		MSG_ERR( "ao_nas: num_bytes > 2GB, server buggy?\n");
	}

	if (event->num_bytes > priv->server_buffer_used)
		event->num_bytes = priv->server_buffer_used;
	priv->server_buffer_used -= event->num_bytes;

	switch (event->reason) {
	case AuReasonWatermark:
		nas_readBuffer(priv, event->num_bytes);
		break;
	case AuReasonUnderrun:
		// buffer underrun -> refill buffer
		priv->server_buffer_used = 0;
		if (priv->expect_underrun) {
			priv->expect_underrun = 0;
		} else {
			static int hint = 1;
			MSG_WARN(
			       "ao_nas: Buffer underrun.\n");
			if (hint) {
				hint = 0;
				MSG_HINT(
				       "Possible reasons are:\n"
				       "1) Network congestion.\n"
				       "2) Your NAS server is too slow.\n"
				       "Try renicing your nasd to e.g. -15.\n");
			}
		}
		if (nas_readBuffer(priv,
				   priv->server_buffer_size -
				   priv->server_buffer_used) != 0) {
			event->cur_state = AuStateStart;
			break;
		}
		MSG_DBG2(
			"ao_nas: Can't refill buffer, stopping flow.\n");
		AuStopFlow(aud, priv->flow, NULL);
		break;
	default:
		break;
	}
	priv->state=event->cur_state;
	return AuTrue;
}

static AuDeviceID nas_find_device(AuServer *aud, int nch)
{
	int i;
	for (i = 0; i < AuServerNumDevices(aud); i++) {
		AuDeviceAttributes *dev = AuServerDevice(aud, i);
		if ((AuDeviceKind(dev) == AuComponentKindPhysicalOutput) &&
		     AuDeviceNumTracks(dev) == nch) {
			return AuDeviceIdentifier(dev);
		}
	}
	return AuNone;
}

static unsigned int nas_aformat_to_auformat(unsigned int *format)
{
	switch (*format) {
	case	AFMT_U8:
		return AuFormatLinearUnsigned8;
	case	AFMT_S8:
		return AuFormatLinearSigned8;
	case	AFMT_U16_LE:
		return AuFormatLinearUnsigned16LSB;
	case	AFMT_U16_BE:
		return AuFormatLinearUnsigned16MSB;
	case	AFMT_S16_LE:
		return AuFormatLinearSigned16LSB;
	case	AFMT_S16_BE:
		return AuFormatLinearSigned16MSB;
	case	AFMT_MU_LAW:
		return AuFormatULAW8;
	default:
		*format=AFMT_S16_NE;
		return nas_aformat_to_auformat(format);
	}
}

// to set/get/query special features/parameters
static MPXP_Rc control(const ao_data_t* ao,int cmd, long arg)
{
    priv_t*priv=ao->priv;
	AuElementParameters aep;
	AuStatus as;
	int retval = MPXP_Unknown;

	ao_control_vol_t *vol = (ao_control_vol_t *)arg;

	switch (cmd) {
	case AOCONTROL_GET_VOLUME:

		vol->right = (float)priv->gain/AU_FIXED_POINT_SCALE*50;
		vol->left = vol->right;

		MSG_DBG2( "ao_nas: AOCONTROL_GET_VOLUME: %.2f\n", vol->right);
		retval = MPXP_Ok;
		break;

	case AOCONTROL_SET_VOLUME:
		/*
		 * kn: we should have vol->left == vol->right but i don't
		 * know if something can change it outside of ao_nas
		 * so i take the mean of both values.
		 */
		priv->gain = AU_FIXED_POINT_SCALE*((vol->left+vol->right)/2)/50;
		MSG_DBG2( "ao_nas: AOCONTROL_SET_VOLUME: %.2f\n", (vol->left+vol->right)/2);

		aep.parameters[AuParmsMultiplyConstantConstant]=priv->gain;
		aep.flow = priv->flow;
		aep.element_num = 1;
		aep.num_parameters = AuParmsMultiplyConstant;

		AuSetElementParameters(priv->aud, 1, &aep, &as);
		if (as != AuSuccess) {
			nas_print_error(priv->aud,
					"control(): AuSetElementParameters", as);
			retval = MPXP_Error;
		} else retval = MPXP_Ok;
		break;
	};

	return retval;
}


static MPXP_Rc init(ao_data_t* ao,unsigned flags)
{
    ao->priv=mp_mallocz(sizeof(priv_t));
    UNUSED(flags);
    return MPXP_Ok;
}
// open & setup audio device
// return: 1=success 0=fail
static MPXP_Rc configure(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format)
{
    priv_t* priv=ao->priv;
    AuElement elms[3];
    AuStatus as;
    char str[256];
    unsigned char auformat = nas_aformat_to_auformat(&format);
    unsigned bytes_per_sample = channels * AuSizeofFormat(auformat);
    unsigned buffer_size;
    char *server;

    ao->format = format;
    ao->samplerate = rate;
    ao->channels = channels;
    ao->outburst = NAS_FRAG_SIZE;
    ao->bps = rate * bytes_per_sample;
    buffer_size = ao->bps; /* buffer 1 second */

    MSG_V("ao2: %d Hz  %d chans  %s\n",rate,channels,
		mpaf_fmt2str(format,str,256));

    /*
     * round up to multiple of NAS_FRAG_SIZE
     * divide by 3 first because of 2:1 split
     */
    buffer_size = (buffer_size/3 + NAS_FRAG_SIZE-1) & ~(NAS_FRAG_SIZE-1);
    ao->buffersize = buffer_size*3;

    priv->client_buffer_size = buffer_size*2;
    priv->client_buffer = mp_malloc(priv->client_buffer_size);
    priv->server_buffer_size = buffer_size;
    priv->server_buffer = mp_malloc(priv->server_buffer_size);

    if (!bytes_per_sample) {
	MSG_ERR("ao_nas: init(): Zero bytes per sample -> nosound\n");
	return MPXP_False;
    }

    if (!(server = getenv("AUDIOSERVER")) &&
	    !(server = getenv("DISPLAY"))) {
	MSG_ERR("ao_nas: init(): AUDIOSERVER environment variable not set -> nosound\n");
	return MPXP_False;
    }

    MSG_V("ao_nas: init(): Using audioserver %s\n", server);

    priv->aud = AuOpenServer(server, 0, NULL, 0, NULL, NULL);
    if (!priv->aud) {
	MSG_ERR("ao_nas: init(): Can't open nas audio server -> nosound\n");
	return MPXP_False;
    }

    while (channels>0) {
	priv->dev = nas_find_device(priv->aud, channels);
	if (priv->dev != AuNone &&
	    ((priv->flow = AuCreateFlow(priv->aud, NULL)) != 0))
		break;
	channels--;
    }

    if (priv->flow == 0) {
	MSG_ERR("ao_nas: init(): Can't find a suitable output device -> nosound\n");
	AuCloseServer(priv->aud);
	priv->aud = 0;
	return MPXP_False;
    }

    AuMakeElementImportClient(elms, rate, auformat, channels, AuTrue,
				buffer_size / bytes_per_sample,
				(buffer_size - NAS_FRAG_SIZE) /
				bytes_per_sample, 0, NULL);
    priv->gain = AuFixedPointFromFraction(1, 1);
    AuMakeElementMultiplyConstant(elms+1, 0, priv->gain);
    AuMakeElementExportDevice(elms+2, 1, priv->dev, rate,
				AuUnlimitedSamples, 0, NULL);
    AuSetElements(priv->aud, priv->flow, AuTrue, sizeof(elms)/sizeof(*elms), elms, &as);
    if (as != AuSuccess) {
	nas_print_error(priv->aud, "init(): AuSetElements", as);
	AuCloseServer(priv->aud);
	priv->aud = 0;
	return MPXP_False;
    }
    AuRegisterEventHandler(priv->aud, AuEventHandlerIDMask |
				AuEventHandlerTypeMask,
				AuEventTypeElementNotify, priv->flow,
				nas_event_handler, (AuPointer) priv);
    AuSetErrorHandler(priv->aud, nas_error_handler);
    priv->state=AuStateStop;
    priv->expect_underrun=0;

    pthread_mutex_init(&priv->buffer_mutex, NULL);
    pthread_create(&priv->event_thread, NULL, &nas_event_thread_start, priv);

    return MPXP_Ok;
}

// close audio device
static void uninit(ao_data_t* ao){

    priv_t* priv=ao->priv;
    MSG_DBG3("ao_nas: uninit()\n");

    priv->expect_underrun = 1;
    while (priv->state != AuStateStop) usleep(1000);
    priv->stop_thread = 1;
    pthread_join(priv->event_thread, NULL);
    AuCloseServer(priv->aud);
    priv->aud = 0;
    mp_free(priv->client_buffer);
    mp_free(priv->server_buffer);
    mp_free(priv);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(ao_data_t* ao){
    priv_t* priv=ao->priv;
	AuStatus as;

	MSG_DBG3("ao_nas: reset()\n");

	pthread_mutex_lock(&priv->buffer_mutex);
	priv->client_buffer_used = 0;
	pthread_mutex_unlock(&priv->buffer_mutex);
	while (priv->state != AuStateStop) {
		AuStopFlow(priv->aud, priv->flow, &as);
		if (as != AuSuccess)
			nas_print_error(priv->aud, "reset(): AuStopFlow", as);
		usleep(1000);
	}
}

// stop playing, keep buffers (for pause)
static void audio_pause(ao_data_t* ao)
{
    priv_t* priv=ao->priv;
	AuStatus as;
	MSG_DBG3("ao_nas: audio_pause()\n");

	AuStopFlow(priv->aud, priv->flow, &as);
}

// resume playing, after audio_pause()
static void audio_resume(ao_data_t* ao)
{
    priv_t* priv=ao->priv;
	AuStatus as;

	MSG_DBG3("ao_nas: audio_resume()\n");

	AuStartFlow(priv->aud, priv->flow, &as);
	if (as != AuSuccess)
		nas_print_error(priv->aud,
				"play(): AuStartFlow", as);
}


// return: how many bytes can be played without blocking
static unsigned get_space(const ao_data_t* ao)
{
    priv_t* priv=ao->priv;
	unsigned result;

	MSG_DBG3("ao_nas: get_space()\n");

	pthread_mutex_lock(&priv->buffer_mutex);
	result = priv->client_buffer_size - priv->client_buffer_used;
	pthread_mutex_unlock(&priv->buffer_mutex);

	return result;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned play(ao_data_t* ao,const any_t* data,unsigned len,unsigned flags)
{
    priv_t* priv=ao->priv;
	unsigned written, maxbursts = 0, playbursts = 0;
	AuStatus as;
	UNUSED(flags);
	MSG_DBG3(
	       "ao_nas: play(%p, %d, %d)\n",
	       data, len, flags);

	if (len == 0)
		return 0;
#if 0
	if (!(flags & AOPLAY_FINAL_CHUNK)) {
		pthread_mutex_lock(&priv->buffer_mutex);
		maxbursts = (priv->client_buffer_size -
			     priv->client_buffer_used) / ao->outburst;
		playbursts = len / ao->outburst;
		len = (playbursts > maxbursts ? maxbursts : playbursts) *
			   ao->outburst;
		pthread_mutex_unlock(&priv->buffer_mutex);
	}
#endif
	/*
	 * If AOPLAY_FINAL_CHUNK is set, we did not actually check len fits
	 * into the available buffer space, but mplayer.c shouldn't give us
	 * more to play than we report to it by get_space(), so this should be
	 * fine.
	 */
	written = nas_writeBuffer(priv, data, len);

	if (priv->state != AuStateStart &&
	    (maxbursts == playbursts /*|| flags & AOPLAY_FINAL_CHUNK*/)) {
		MSG_DBG2("ao_nas: play(): Starting flow.\n");
		priv->expect_underrun = 1;
		AuStartFlow(priv->aud, priv->flow, &as);
		if (as != AuSuccess)
			nas_print_error(priv->aud, "play(): AuStartFlow", as);
	}

	return written;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(const ao_data_t* ao)
{
    priv_t* priv=ao->priv;
	float result;

	MSG_DBG3( "ao_nas: get_delay()\n");

	pthread_mutex_lock(&priv->buffer_mutex);
	result = ((float)(priv->client_buffer_used +
			  priv->server_buffer_used)) /
		 (float)ao->bps;
	pthread_mutex_unlock(&priv->buffer_mutex);

	return result;
}
