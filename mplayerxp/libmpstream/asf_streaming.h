#ifndef __ASF_STEAMING_H_INCLUDED
#define __ASF_STEAMING_H_INCLUDED 1
#include "stream.h"

extern int asf_streaming_start(libinput_t* libinput, stream_t *stream, int *demuxer_type);
extern int asf_mmst_streaming_start(libinput_t* libinput,stream_t *stream);

#endif
