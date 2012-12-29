#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "ao_msg.h"

namespace mpxp {
/* NAS_FRAG_SIZE must be a power-of-two value */
static const int NAS_FRAG_SIZE=4096;

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

class Nas_AO_Interface : public AO_Interface {
    public:
	Nas_AO_Interface(const std::string& subdevice);
	virtual ~Nas_AO_Interface();

	virtual MPXP_Rc		open(unsigned flags);
	virtual MPXP_Rc		configure(unsigned rate,unsigned channels,unsigned format);
	virtual unsigned	samplerate() const;
	virtual unsigned	channels() const;
	virtual unsigned	format() const;
	virtual unsigned	buffersize() const;
	virtual unsigned	outburst() const;
	virtual MPXP_Rc		test_rate(unsigned r) const;
	virtual MPXP_Rc		test_channels(unsigned c) const;
	virtual MPXP_Rc		test_format(unsigned f) const;
	virtual void		reset();
	virtual unsigned	get_space();
	virtual float		get_delay();
	virtual unsigned	play(const any_t* data,unsigned len,unsigned flags);
	virtual void		pause();
	virtual void		resume();
	virtual MPXP_Rc		ctrl(int cmd,long arg) const;
	static any_t*		nas_event_thread_start(any_t*data);
	static AuBool		nas_event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd);
    private:
	unsigned	_channels,_samplerate,_format;
	unsigned	_buffersize,_outburst;
	unsigned	bps() const { return _channels*_samplerate*afmt2bps(_format); }
	const char*	nas_reason(unsigned int reason) const;
	const char*	nas_elementnotify_kind(unsigned int kind) const;
	const char*	nas_event_type(unsigned int type) const ;
	const char*	nas_state(unsigned int state) const;
	void		nas_print_error(const char *prefix, AuStatus as) const;
	int		nas_readBuffer(unsigned int num);
	int		nas_writeBuffer(const any_t*data, unsigned int len);
	int		nas_empty_event_queue() const;
	AuDeviceID	nas_find_device(int nch) const;
	unsigned	nas_aformat_to_auformat(unsigned* format) const;

	AuServer	*aud;
	AuFlowID	flow;
	AuDeviceID	dev;
	AuFixedPoint	gain;

	unsigned	state;
	int		expect_underrun;

	char*		client_buffer;
	char*		server_buffer;
	unsigned	client_buffer_size;
	unsigned	client_buffer_used;
	unsigned	server_buffer_size;
	unsigned	server_buffer_used;
	pthread_mutex_t	buffer_mutex;

	pthread_t	event_thread;
	int		stop_thread;
};

Nas_AO_Interface::Nas_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {
    pthread_mutex_init(&buffer_mutex, NULL);
    pthread_create(&event_thread, NULL, &nas_event_thread_start, this);
}
Nas_AO_Interface::~Nas_AO_Interface() {
    mpxp_dbg3<<"ao_nas: uninit()"<<std::endl;

    expect_underrun = 1;
    while (state != AuStateStop) yield_timeslice();
    stop_thread = 1;
    pthread_join(event_thread, NULL);
    AuCloseServer(aud);
    aud = 0;
    delete client_buffer;
    delete server_buffer;
}

const char* Nas_AO_Interface::nas_reason(unsigned int reason) const {
    if (reason > 6) reason = 6;
    return nas_reasons[reason];
}

const char* Nas_AO_Interface::nas_elementnotify_kind(unsigned int kind) const {
    if (kind > 2) kind = 3;
    return nas_elementnotify_kinds[kind];
}

const char* Nas_AO_Interface::nas_event_type(unsigned int type) const {
    if (type > 6) type = 0;
    return nas_event_types[type];
}

const char* Nas_AO_Interface::nas_state(unsigned int _state) const {
    if (_state>3) _state = 3;
    return nas_states[_state];
}

void Nas_AO_Interface::nas_print_error(const char *prefix, AuStatus as) const {
    char s[100];
    AuGetErrorText(aud, as, s, 100);
    mpxp_err<<"ao_nas: "<<prefix<<": returned status "<<as<<" ("<<s<<")"<<std::endl;
}

int Nas_AO_Interface::nas_readBuffer(unsigned int num) {
    AuStatus as;

    pthread_mutex_lock(&buffer_mutex);
    mpxp_dbg2<<"ao_nas: nas_readBuffer(): num="<<num<<" client="<<client_buffer_used<<"/"<<client_buffer_size<<" server="<<server_buffer_used<<"/"<<server_buffer_size<<std::endl;

    if (client_buffer_used == 0) {
	mpxp_dbg2<<"ao_nas: buffer is empty, nothing read."<<std::endl;
	pthread_mutex_unlock(&buffer_mutex);
	return 0;
    }
    if (num > client_buffer_used)
	num = client_buffer_used;

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
    if (num > server_buffer_size)
	num = server_buffer_size;
    memcpy(server_buffer, client_buffer, num);

    client_buffer_used -= num;
    server_buffer_used += num;
    memmove(client_buffer, client_buffer + num, client_buffer_used);
    pthread_mutex_unlock(&buffer_mutex);

    /*
     * Now write the new buffer to the network.
     */
    AuWriteElement(aud, flow, 0, num, server_buffer, AuFalse, &as);
    if (as != AuSuccess)
	nas_print_error("nas_readBuffer(): AuWriteElement", as);

    return num;
}

int Nas_AO_Interface::nas_writeBuffer(const any_t*data, unsigned int len) {
    pthread_mutex_lock(&buffer_mutex);
    mpxp_dbg2<<"ao_nas: nas_writeBuffer(): len="<<len<<" client="<<client_buffer_used<<"/"<<client_buffer_size<<" server="<<server_buffer_used<<"/%d"<<server_buffer_size<<std::endl;

    /* make sure we don't overflow the buffer */
    if (len > client_buffer_size - client_buffer_used)
	len = client_buffer_size - client_buffer_used;
    memcpy(client_buffer + client_buffer_used, data, len);
    client_buffer_used += len;

    pthread_mutex_unlock(&buffer_mutex);

    return len;
}

int Nas_AO_Interface::nas_empty_event_queue() const
{
    AuEvent ev;
    int result = 0;

    while (AuScanForTypedEvent(aud, AuEventsQueuedAfterFlush,
				AuTrue, AuEventTypeElementNotify, &ev)) {
	AuDispatchEvent(aud, &ev);
	result = 1;
    }
    return result;
}

any_t* Nas_AO_Interface::nas_event_thread_start(any_t*data) {
    Nas_AO_Interface& _this=*reinterpret_cast<Nas_AO_Interface*>(data);
    do {
	mpxp_dbg2<<"ao_nas: event thread heartbeat (state="<<_this.nas_state(_this.state)<<")"<<std::endl;
	_this.nas_empty_event_queue();
	yield_timeslice();
    } while (!_this.stop_thread);
    return NULL;
}

static AuBool nas_error_handler_callback(AuServer* aud,AuErrorEvent* ev) {
    char s[100];
    AuGetErrorText(aud, ev->error_code, s, 100);
    mpxp_err<<"ao_nas: error ["<<s<<"]"<<std::endl;
    mpxp_err<<"error_code: "<<ev->error_code<<std::endl;
    mpxp_err<<"request_code: "<<ev->request_code<<std::endl;
    mpxp_err<<"minor_code: "<<ev->minor_code<<std::endl;

    return AuTrue;
}

AuBool Nas_AO_Interface::nas_event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd) {
    AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;
    Nas_AO_Interface& _this = *reinterpret_cast<Nas_AO_Interface*>(hnd->data);

    mpxp_dbg2<<"ao_nas: event_handler(): type "<<_this.nas_event_type(event->type)
	    <<" kind "<<_this.nas_elementnotify_kind(event->kind)
	    <<" state "<<_this.nas_state(event->prev_state)<<"->"<<_this.nas_state(event->cur_state)
	    <<" reason "<<_this.nas_reason(event->reason)
	    <<" numbytes "<<event->num_bytes<<" expect_underrun"<<_this.expect_underrun<<std::endl;
    if (event->num_bytes > INT_MAX) {
	mpxp_err<<"ao_nas: num_bytes > 2GB, server buggy?"<<std::endl;
    }

    if (event->num_bytes > _this.server_buffer_used)
	event->num_bytes = _this.server_buffer_used;
    _this.server_buffer_used -= event->num_bytes;

    switch (event->reason) {
	case AuReasonWatermark:
	    _this.nas_readBuffer(event->num_bytes);
	    break;
	case AuReasonUnderrun:
	    // buffer underrun -> refill buffer
	    _this.server_buffer_used = 0;
	    if (_this.expect_underrun) {
		_this.expect_underrun = 0;
	    } else {
		static int hint = 1;
		mpxp_warn<<"ao_nas: Buffer underrun."<<std::endl;
		if (hint) {
		    hint = 0;
		    mpxp_hint<<"Possible reasons are:"<<std::endl
			    <<"1) Network congestion."<<std::endl
			    <<"2) Your NAS server is too slow."<<std::endl
			    <<"Try renicing your nasd to e.g. -15."<<std::endl;
		}
	    }
	    if (_this.nas_readBuffer(_this.server_buffer_size - _this.server_buffer_used) != 0) {
		event->cur_state = AuStateStart;
		break;
	    }
	    mpxp_dbg2<<"ao_nas: Can't refill buffer, stopping flow."<<std::endl;
	    AuStopFlow(_this.aud, _this.flow, NULL);
	    break;
	default:
	    break;
    }
    _this.state=event->cur_state;
    return AuTrue;
}

AuDeviceID Nas_AO_Interface::nas_find_device(int nch) const {
    int i;
    for (i = 0; i < AuServerNumDevices(aud); i++) {
	AuDeviceAttributes* _dev = AuServerDevice(aud, i);
	if ((AuDeviceKind(_dev) == AuComponentKindPhysicalOutput) &&
	     AuDeviceNumTracks(_dev) == nch) {
		return AuDeviceIdentifier(_dev);
	}
    }
    return AuNone;
}

unsigned Nas_AO_Interface::nas_aformat_to_auformat(unsigned* f) const {
    switch (*f) {
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
	    break;
    }
    *f=AFMT_S16_NE;
    return nas_aformat_to_auformat(f);
}

// to set/get/query special features/parameters
MPXP_Rc Nas_AO_Interface::ctrl(int cmd, long arg) const {
    AuElementParameters aep;
    AuStatus as;
    MPXP_Rc retval = MPXP_Unknown;
    AuFixedPoint g;

    ao_control_vol_t *vol = (ao_control_vol_t *)arg;

    switch (cmd) {
	case AOCONTROL_GET_VOLUME:
	    vol->right = (float)gain/AU_FIXED_POINT_SCALE*50;
	    vol->left = vol->right;
	    mpxp_dbg2<<"ao_nas: AOCONTROL_GET_VOLUME: "<<vol->right<<std::endl;
	    retval = MPXP_Ok;
	    break;
	case AOCONTROL_SET_VOLUME:
	    /*
	     * kn: we should have vol->left == vol->right but i don't
	     * know if something can change it outside of ao_nas
	     * so i take the mean of both values.
	     */
	    g = AU_FIXED_POINT_SCALE*((vol->left+vol->right)/2)/50;
	    mpxp_dbg2<<"ao_nas: AOCONTROL_SET_VOLUME: "<<((vol->left+vol->right)/2)<<std::endl;

	    aep.parameters[AuParmsMultiplyConstantConstant]=g;
	    aep.flow = flow;
	    aep.element_num = 1;
	    aep.num_parameters = AuParmsMultiplyConstant;

	    AuSetElementParameters(aud, 1, &aep, &as);
	    if (as != AuSuccess) {
		nas_print_error("control_ao(): AuSetElementParameters", as);
		retval = MPXP_Error;
	    } else retval = MPXP_Ok;
	    break;
    };
    return retval;
}

MPXP_Rc Nas_AO_Interface::open(unsigned flags) {
    UNUSED(flags);
    return MPXP_Ok;
}
// open & setup audio device
// return: 1=success 0=fail
MPXP_Rc Nas_AO_Interface::configure(unsigned r,unsigned c,unsigned f)
{
    AuElement elms[3];
    AuStatus as;
    unsigned char auformat = nas_aformat_to_auformat(&f);
    unsigned bytes_per_sample = _channels * AuSizeofFormat(auformat);
    unsigned buffer_size;
    std::string server;

    _format = f;
    _samplerate = r;
    _channels = c;
    _outburst = NAS_FRAG_SIZE;
    buffer_size = bps(); /* buffer 1 second */

    mpxp_v<<"ao3: "<<r<<" Hz  "<<c<<" chans "<<afmt2str(f)<<std::endl;

    /*
     * round up to multiple of NAS_FRAG_SIZE
     * divide by 3 first because of 2:1 split
     */
    buffer_size = (buffer_size/3 + NAS_FRAG_SIZE-1) & ~(NAS_FRAG_SIZE-1);
    _buffersize = buffer_size*3;

    client_buffer_size = buffer_size*2;
    client_buffer = new char [client_buffer_size];
    server_buffer_size = buffer_size;
    server_buffer = new char [server_buffer_size];

    if (!bytes_per_sample) {
	mpxp_err<<"ao_nas: init(): Zero bytes per sample -> nosound"<<std::endl;
	return MPXP_False;
    }

    const std::map<std::string,std::string>& envm=mpxp_get_environment();
    std::map<std::string,std::string>::const_iterator it;
    it = envm.find("AUDIOSERVER");
    if(it!=envm.end()) server = (*it).second;
    if(server.empty()) {
	it = envm.find("DISPLAY");
	if(it!=envm.end()) server = (*it).second;
    }
    if (server.empty()) {
	mpxp_err<<"ao_nas: init(): AUDIOSERVER environment variable not set -> nosound"<<std::endl;
	return MPXP_False;
    }

    mpxp_v<<"ao_nas: init(): Using audioserver "<<server<<std::endl;

    aud = AuOpenServer(server.c_str(), 0, NULL, 0, NULL, NULL);
    if (!aud) {
	mpxp_err<<"ao_nas: init(): Can't open nas audio server -> nosound"<<std::endl;
	return MPXP_False;
    }

    while (c>0) {
	dev = nas_find_device(c);
	if (dev != AuNone &&
	    ((flow = AuCreateFlow(aud, NULL)) != 0))
		break;
	c--;
    }

    if (flow == 0) {
	mpxp_err<<"ao_nas: init(): Can't find a suitable output device -> nosound"<<std::endl;
	AuCloseServer(aud);
	aud = 0;
	return MPXP_False;
    }

    AuMakeElementImportClient(elms, r, auformat, c, AuTrue,
				buffer_size / bytes_per_sample,
				(buffer_size - NAS_FRAG_SIZE) /
				bytes_per_sample, 0, NULL);
    gain = AuFixedPointFromFraction(1, 1);
    AuMakeElementMultiplyConstant(elms+1, 0, gain);
    AuMakeElementExportDevice(elms+2, 1, dev, r,
				AuUnlimitedSamples, 0, NULL);
    AuSetElements(aud, flow, AuTrue, sizeof(elms)/sizeof(*elms), elms, &as);
    if (as != AuSuccess) {
	nas_print_error("init(): AuSetElements", as);
	AuCloseServer(aud);
	aud = 0;
	return MPXP_False;
    }
    AuRegisterEventHandler(aud, AuEventHandlerIDMask |
				AuEventHandlerTypeMask,
				AuEventTypeElementNotify, flow,
				nas_event_handler, (AuPointer) this);
    AuSetErrorHandler(aud, nas_error_handler_callback);
    state=AuStateStop;
    expect_underrun=0;

    return MPXP_Ok;
}

// stop playing and empty buffers (for seeking/pause)
void Nas_AO_Interface::reset() {
    AuStatus as;

    mpxp_dbg3<<"ao_nas: reset()"<<std::endl;

    pthread_mutex_lock(&buffer_mutex);
    client_buffer_used = 0;
    pthread_mutex_unlock(&buffer_mutex);
    while (state != AuStateStop) {
	AuStopFlow(aud, flow, &as);
	if (as != AuSuccess)
		nas_print_error("reset(): AuStopFlow", as);
	yield_timeslice();
    }
}

// stop playing, keep buffers (for pause)
void Nas_AO_Interface::pause() {
    AuStatus as;
    mpxp_dbg3<<"ao_nas: audio_pause()"<<std::endl;

    AuStopFlow(aud, flow, &as);
}

// resume playing, after audio_pause()
void Nas_AO_Interface::resume()
{
    AuStatus as;

    mpxp_dbg3<<"ao_nas: audio_resume()"<<std::endl;

    AuStartFlow(aud, flow, &as);
    if (as != AuSuccess) nas_print_error("play(): AuStartFlow", as);
}


// return: how many bytes can be played without blocking
unsigned Nas_AO_Interface::get_space()
{
    unsigned result;

    mpxp_dbg3<<"ao_nas: get_space()"<<std::endl;

    pthread_mutex_lock(&buffer_mutex);
    result = client_buffer_size - client_buffer_used;
    pthread_mutex_unlock(&buffer_mutex);

    return result;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
unsigned Nas_AO_Interface::play(const any_t* data,unsigned len,unsigned flags) {
    unsigned written, maxbursts = 0, playbursts = 0;
    AuStatus as;
    UNUSED(flags);
    mpxp_dbg3<<"ao_nas: play("<<data<<", "<<len<<", "<<flags<<")"<<std::endl;

    if (len == 0) return 0;
    /*
     * If AOPLAY_FINAL_CHUNK is set, we did not actually check len fits
     * into the available buffer space, but mplayer.c shouldn't give us
     * more to play than we report to it by get_space(), so this should be
     * fine.
     */
    written = nas_writeBuffer(data, len);

    if (state != AuStateStart &&
	(maxbursts == playbursts /*|| flags & AOPLAY_FINAL_CHUNK*/)) {
	    mpxp_dbg2<<"ao_nas: play(): Starting flow."<<std::endl;
	    expect_underrun = 1;
	    AuStartFlow(aud, flow, &as);
	    if (as != AuSuccess) nas_print_error("play(): AuStartFlow", as);
    }
    return written;
}

// return: delay in seconds between first and last sample in buffer
float Nas_AO_Interface::get_delay() {
    float result;

    mpxp_dbg3<<"ao_nas: get_delay()"<<std::endl;

    pthread_mutex_lock(&buffer_mutex);
    result = ((float)(client_buffer_used + server_buffer_used)) / (float)bps();
    pthread_mutex_unlock(&buffer_mutex);

    return result;
}

unsigned Nas_AO_Interface::samplerate() const { return _samplerate; }
unsigned Nas_AO_Interface::channels() const { return _channels; }
unsigned Nas_AO_Interface::format() const { return _format; }
unsigned Nas_AO_Interface::buffersize() const { return _buffersize; }
unsigned Nas_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Nas_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Nas_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Nas_AO_Interface::test_format(unsigned f) const { UNUSED(f); return MPXP_Ok; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) Nas_AO_Interface(sd); }

extern const ao_info_t audio_out_nas = {
    "NAS audio output",
    "nas",
    "Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
    "",
    query_interface
};
} // namespace mpxp