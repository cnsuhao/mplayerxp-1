#ifndef PARSE_ES_INCLUDED
#define PARSE_ES_INCLUDED 1

enum {
    MAX_VIDEO_PACKET_SIZE=(224*1024+4),
    VIDEOBUFFER_SIZE=0x100000
};
extern unsigned char* videobuffer;
extern int videobuf_len;
extern unsigned char videobuf_code[4];
extern int videobuf_code_len;

// sync video stream, and returns next packet code
int sync_video_packet(Demuxer_Stream *ds);

// return: packet length
int read_video_packet(Demuxer_Stream *ds);

// return: next packet code
int skip_video_packet(Demuxer_Stream *ds);
#endif
