#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "mpxp_help.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libao3/afmt.h"
#include "osdep/bswap.h"
#include "ad_internal.h"

namespace	usr {
    class libadv_decoder : public Audio_Decoder {
	public:
	    libadv_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~libadv_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    dv_decoder_t*		init_global_rawdv_decoder();

	    dv_decoder_t*	decoder;
	    int16_t*		audioBuffers[4];
	    sh_audio_t&		sh;
	    const audio_probe_t*probe;
};

static const audio_probe_t probes[] = {
    { "libdv", "libdv", FOURCC_TAG('R','A','D','V'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

// defined in vd_libdv.c:
dv_decoder_t* libadv_decoder::init_global_rawdv_decoder()
{
    dv_decoder_t* global_rawdv_decoder;
    global_rawdv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
    global_rawdv_decoder->quality=DV_QUALITY_BEST;
    global_rawdv_decoder->prev_frame_decoded = 0;
    return global_rawdv_decoder;
}

libadv_decoder::libadv_decoder(sh_audio_t& _sh,audio_filter_info_t& afi,uint32_t wtag)
	    :Audio_Decoder(_sh,afi,wtag)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    sh.audio_out_minsize=4*DV_AUDIO_MAX_SAMPLES*2;

    WAVEFORMATEX *h=sh.wf;

    if(!h) throw bad_format_exception();

    sh.i_bps=h->nAvgBytesPerSec;
    sh.nch=h->nChannels;
    sh.rate=h->nSamplesPerSec;
    sh.afmt=bps2afmt((h->wBitsPerSample+7)/8);
    decoder=init_global_rawdv_decoder();
    for (i=0; i < 4; i++) audioBuffers[i] = new int16_t[DV_AUDIO_MAX_SAMPLES];
}

libadv_decoder::~libadv_decoder() {
    unsigned i;
    for (i=0; i < 4; i++) delete audioBuffers[i];
}

audio_probe_t libadv_decoder::get_probe_information() const { return *probe; }

MPXP_Rc libadv_decoder::ctrl(int cmd,any_t* arg)
{
    // TODO!!!
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned libadv_decoder::run(unsigned char *buf, unsigned minlen, unsigned maxlen,float& pts)
{
    unsigned char* dv_audio_frame=NULL;
    unsigned xx;
    size_t len=0;
    float apts;
    UNUSED(maxlen);
    while(len<minlen) {
	xx=ds_get_packet_r(*sh.ds,&dv_audio_frame,len>0?apts:pts);
	if((int)xx<=0 || !dv_audio_frame) return 0; // EOF?

	dv_parse_header(decoder, dv_audio_frame);

	if(xx!=decoder->frame_size) mpxp_warn<<"AudioFramesize differs "<<xx<<" "<<decoder->frame_size<<std::endl;

	if (dv_decode_full_audio(decoder, dv_audio_frame,audioBuffers)) {
	    /* Interleave the audio into a single buffer */
	    int i=0;
	    int16_t *bufP=(int16_t*)buf;

	    for (i=0; i < decoder->audio->samples_this_frame; i++) {
		int ch;
		for (ch=0; ch < decoder->audio->num_channels; ch++)
		    bufP[len++] = audioBuffers[ch][i];
	    }
	}
	len+=decoder->audio->samples_this_frame;
	buf+=decoder->audio->samples_this_frame;
    }
    return len*2;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) libadv_decoder(sh,afi,wtag); }

extern const ad_info_t ad_libdv_info = {
    "Raw DV Audio Decoder",
    "libdv",
    "Alexander Neundorf <neundorf@kde.org>",
    "http://libdv.sourceforge.net",
    query_interface,
    options
};
} // namespace	usr
