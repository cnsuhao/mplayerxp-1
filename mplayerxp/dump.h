/*
    dump.h - stream dumper interface
*/
#ifndef DUMP_H_INCLUDED
#define DUMP_H_INCLUDED 1
#include "libmpdemux/demuxer_r.h"
namespace mpxp {
    int  dump_parse(const char *param);
    void dump_stream(stream_t *stream);
    void dump_mux_init(demuxer_t *demuxer,any_t*libinput);
    void dump_mux(demuxer_t *demuxer,int use_pts,const char *seek_to_sec,unsigned play_n_frames);
    void dump_mux_close(demuxer_t *demuxer);
    void __FASTCALL__ dump_stream_event_handler(struct stream_s *s,const stream_packet_t*sp);
} //namespace
#endif
