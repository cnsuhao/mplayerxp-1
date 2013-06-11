#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libmpdemux/demuxer_r.h"
#include "ad.h"
#include "ad_msg.h"

namespace	usr {
    class anull_decoder : public Audio_Decoder {
	public:
	    anull_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~anull_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    sh_audio_t& sh;
    };

anull_decoder::anull_decoder(sh_audio_t& _sh,audio_filter_info_t& afi,uint32_t wtag)
	    :Audio_Decoder(_sh,afi,wtag)
	    ,sh(_sh)
{
    throw bad_format_exception();
}

anull_decoder::~anull_decoder() {}

audio_probe_t anull_decoder::get_probe_information() const { audio_probe_t probe = { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}; return probe; }

MPXP_Rc anull_decoder::ctrl(int cmd,any_t* arg)
{
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned anull_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    UNUSED(buf);
    UNUSED(minlen);
    UNUSED(maxlen);
    pts=0;
    return 0;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) anull_decoder(sh,afi,wtag); }

extern const ad_info_t ad_null_info = {
    "Null audio decoder",
    "null",
    "Nickols_K",
    "build-in",
    query_interface,
    options
};
} // namespace	usr
