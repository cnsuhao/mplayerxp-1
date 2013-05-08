#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * Some code freely inspired from VobSub <URL:http://vobsub.edensrising.com>,
 * with kind permission from Gabest <gabest@freemail.hu>
 */
/* #define HAVE_GETLINE */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mplayerxp.h"
#include "libmpstream2/stream.h"
#include "vobsub.h"
#include "spudec.h"
#include "mpsub_msg.h"

#ifdef HAVE_GETLINE
extern ssize_t getline(char **, size_t *, FILE *);
#else
/* FIXME This should go into a general purpose library or even a
   separate file. */
static ssize_t __FASTCALL__ __getline (char **lineptr, size_t *n, FILE *stream)
{
    size_t res = 0;
    int c;
    if (*lineptr == NULL) {
	*lineptr = new char [4096];
	if (*lineptr)
	    *n = 4096;
    }
    else if (*n == 0) {
	char *tmp = (char*)mp_realloc(*lineptr, 4096);
	if (tmp) {
	    *lineptr = tmp;
	    *n = 4096;
	}
    }
    if (*lineptr == NULL || *n == 0)
	return -1;

    for (c = fgetc(stream); c != EOF; c = fgetc(stream)) {
	if (res + 1 >= *n) {
	    char *tmp = (char*)mp_realloc(*lineptr, *n * 2);
	    if (tmp == NULL)
		return -1;
	    *lineptr = tmp;
	    *n *= 2;
	}
	(*lineptr)[res++] = c;
	if (c == '\n') {
	    (*lineptr)[res] = 0;
	    return res;
	}
    }
    if (res == 0)
	return -1;
    (*lineptr)[res] = 0;
    return res;
}
#endif

/**********************************************************************
 * MPEG parsing
 **********************************************************************/

struct mpeg_t {
    Stream *stream;
    unsigned int pts;
    int aid;
    unsigned char *packet;
    unsigned int packet_reserve;
    unsigned int packet_size;
    int fd;
};

static mpeg_t *  __FASTCALL__ mpeg_open(const std::string& filename)
{
    mpeg_t *res = new(zeromem) mpeg_t;
    int err = res == NULL;
    if (!err) {
	int fd;
	res->pts = 0;
	res->aid = -1;
	res->packet = NULL;
	res->packet_size = 0;
	res->packet_reserve = 0;
	fd = ::open(filename.c_str(), O_RDONLY);
	err = fd < 0;
	if (!err) {
	    res->stream = new(zeromem) Stream(Stream::Type_Seekable);
	    res->fd = fd;
	    err = res->stream == NULL;
	    if (err)
		close(fd);
	}
	else {
	    delete res;
	    close(fd);
	}
    }
    return err ? NULL : res;
}

static void __FASTCALL__ mpeg_free(mpeg_t *mpeg)
{
    int fd;
    if (mpeg->packet)
	delete mpeg->packet;
    fd = mpeg->fd;
    delete mpeg->stream;
    close(fd);
    delete mpeg;
}

static int __FASTCALL__ mpeg_eof(mpeg_t *mpeg)
{
    return mpeg->stream->eof();
}

static off_t __FASTCALL__ mpeg_tell(mpeg_t *mpeg)
{
    return mpeg->stream->tell();
}

static int __FASTCALL__ mpeg_run(mpeg_t *mpeg)
{
    unsigned int len, idx, version;
    int c;
    /* Goto start of a packet, it starts with 0x000001?? */
    const unsigned char wanted[] = { 0, 0, 1 };
    unsigned char buf[5];

    mpeg->aid = -1;
    mpeg->packet_size = 0;
    if (mpeg->stream->read( buf, 4) != 4)
	return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
	c = mpeg->stream->read_char();
	if (c < 0)
	    return -1;
	memmove(buf, buf + 1, 3);
	buf[3] = c;
    }
    switch (buf[3]) {
    case 0xb9:			/* System End Code */
	break;
    case 0xba:			/* Packet start code */
	c = mpeg->stream->read_char();
	if (c < 0)
	    return -1;
	if ((c & 0xc0) == 0x40)
	    version = 4;
	else if ((c & 0xf0) == 0x20)
	    version = 2;
	else {
	    MSG_ERR( "Unsupported MPEG version: 0x%02x", c);
	    return -1;
	}
	if (version == 4) {
	    if (!mpeg->stream->skip( 9))
		return -1;
	}
	else if (version == 2) {
	    if (!mpeg->stream->skip( 7))
		return -1;
	}
	else
	    abort();
	break;
    case 0xbd:			/* packet */
	if (mpeg->stream->read( buf, 2) != 2)
	    return -1;
	len = buf[0] << 8 | buf[1];
	idx = mpeg_tell(mpeg);
	c = mpeg->stream->read_char();
	if (c < 0)
	    return -1;
	if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
	    if (mpeg->stream->read_char() < 0)
		return -1;
	    c = mpeg->stream->read_char();
	    if (c < 0)
		return -1;
	}
	if ((c & 0xf0) == 0x20) { /* System-1 stream timestamp */
	    /* Do we need this? */
	    abort();
	}
	else if ((c & 0xf0) == 0x30) {
	    /* Do we need this? */
	    abort();
	}
	else if ((c & 0xc0) == 0x80) { /* System-2 (.VOB) stream */
	    unsigned int pts_flags, hdrlen, dataidx;
	    c = mpeg->stream->read_char();
	    if (c < 0)
		return -1;
	    pts_flags = c;
	    c = mpeg->stream->read_char();
	    if (c < 0)
		return -1;
	    hdrlen = c;
	    dataidx = mpeg_tell(mpeg) + hdrlen;
	    if (dataidx > idx + len) {
		MSG_ERR( "Invalid header length: %d (total length: %d, idx: %d, dataidx: %d)\n",
			hdrlen, len, idx, dataidx);
		return -1;
	    }
	    if ((pts_flags & 0xc0) == 0x80) {
		if (mpeg->stream->read( buf, 5) != 5)
		    return -1;
		if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&  (buf[4] & 1))) {
		    MSG_ERR( "vobsub PTS error: 0x%02x %02x%02x %02x%02x \n",
			    buf[0], buf[1], buf[2], buf[3], buf[4]);
		    mpeg->pts = 0;
		}
		else
		    mpeg->pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 | (buf[2] & 0xfe) << 14
			| buf[3] << 7 | (buf[4] >> 1)) / 900;
	    }
	    else /* if ((pts_flags & 0xc0) == 0xc0) */ {
		/* what's this? */
		/* abort(); */
	    }
	    mpeg->stream->seek( dataidx);
	    mpeg->aid = mpeg->stream->read_char();
	    if (mpeg->aid < 0) {
		MSG_ERR( "Bogus aid %d\n", mpeg->aid);
		return -1;
	    }
	    mpeg->packet_size = len - ((unsigned int) mpeg_tell(mpeg) - idx);
	    if (mpeg->packet_reserve < mpeg->packet_size) {
		if (mpeg->packet)
		    delete mpeg->packet;
		mpeg->packet = new unsigned char [mpeg->packet_size];
		if (mpeg->packet)
		    mpeg->packet_reserve = mpeg->packet_size;
	    }
	    if (mpeg->packet == NULL) {
		MSG_ERR("mp_malloc failure");
		mpeg->packet_reserve = 0;
		mpeg->packet_size = 0;
		return -1;
	    }
	    if ((unsigned)mpeg->stream->read( mpeg->packet, mpeg->packet_size) != mpeg->packet_size) {
		MSG_ERR("stream_read failure");
		mpeg->packet_size = 0;
		return -1;
	    }
	    idx = len;
	}
	break;
    case 0xbe:			/* Padding */
	if (mpeg->stream->read( buf, 2) != 2)
	    return -1;
	len = buf[0] << 8 | buf[1];
	if (len > 0 && !mpeg->stream->skip( len))
	    return -1;
	break;
    default:
	if (0xc0 <= buf[3] && buf[3] < 0xf0) {
	    /* MPEG audio or video */
	    if (mpeg->stream->read( buf, 2) != 2)
		return -1;
	    len = buf[0] << 8 | buf[1];
	    if (len > 0 && !mpeg->stream->skip( len))
		return -1;

	}
	else {
	    MSG_ERR( "unknown header 0x%02X%02X%02X%02X\n",
		    buf[0], buf[1], buf[2], buf[3]);
	    return -1;
	}
    }
    return 0;
}

/**********************************************************************
 * Packet queue
 **********************************************************************/

struct packet_t {
    unsigned int pts100;
    off_t filepos;
    unsigned int size;
    unsigned char *data;
};

struct packet_queue_t {
    std::string id;
    packet_t *packets;
    unsigned int packets_reserve;
    unsigned int packets_size;
    unsigned int current_index;
};

static void __FASTCALL__ packet_construct(packet_t *pkt)
{
    pkt->pts100 = 0;
    pkt->filepos = 0;
    pkt->size = 0;
    pkt->data = NULL;
}

static void __FASTCALL__ packet_destroy(packet_t *pkt)
{
    if (pkt->data)
	delete pkt->data;
}

static void __FASTCALL__ packet_queue_construct(packet_queue_t *queue)
{
    queue->id = "";
    queue->packets = NULL;
    queue->packets_reserve = 0;
    queue->packets_size = 0;
    queue->current_index = 0;
}

static void __FASTCALL__ packet_queue_destroy(packet_queue_t *queue)
{
    if (queue->packets) {
	while (queue->packets_size--)
	    packet_destroy(queue->packets + queue->packets_size);
	delete queue->packets;
    }
    return;
}

/* Make sure there is enough room for needed_size packets in the
   packet queue. */
static int __FASTCALL__ packet_queue_ensure(packet_queue_t *queue, unsigned int needed_size)
{
    if (queue->packets_reserve < needed_size) {
	if (queue->packets) {
	    packet_t *tmp = (packet_t*)mp_realloc(queue->packets, 2 * queue->packets_reserve * sizeof(packet_t));
	    if (tmp == NULL) {
		MSG_ERR("mp_realloc failure");
		return -1;
	    }
	    queue->packets = tmp;
	    queue->packets_reserve *= 2;
	}
	else {
	    queue->packets = new(zeromem) packet_t;
	    if (queue->packets == NULL) {
		MSG_ERR("mp_malloc failure");
		return -1;
	    }
	    queue->packets_reserve = 1;
	}
    }
    return 0;
}

/* add one more packet */
static int __FASTCALL__ packet_queue_grow(packet_queue_t *queue)
{
    if (packet_queue_ensure(queue, queue->packets_size + 1) < 0)
	return -1;
    packet_construct(queue->packets + queue->packets_size);
    ++queue->packets_size;
    return 0;
}

/* insert a new packet, duplicating pts from the current one */
static int __FASTCALL__ packet_queue_insert(packet_queue_t *queue)
{
    packet_t *pkts;
    if (packet_queue_ensure(queue, queue->packets_size + 1) < 0)
	return -1;
    /* XXX packet_size does not reflect the real thing here, it will be updated a bit later */
    memmove(queue->packets + queue->current_index + 2,
	    queue->packets + queue->current_index + 1,
	    sizeof(packet_t) * (queue->packets_size - queue->current_index - 1));
    pkts = queue->packets + queue->current_index;
    ++queue->packets_size;
    ++queue->current_index;
    packet_construct(pkts + 1);
    pkts[1].pts100 = pkts[0].pts100;
    pkts[1].filepos = pkts[0].filepos;
    return 0;
}

/**********************************************************************
 * Vosub
 **********************************************************************/

struct vobsub_t {
    unsigned int palette[16];
    unsigned int cuspal[4];
    int delay;
    unsigned int custom;
    unsigned int have_palette;
    unsigned int orig_frame_width, orig_frame_height;
    unsigned int origin_x, origin_y;
    unsigned int forced_subs;
    /* index */
    packet_queue_t *spu_streams;
    unsigned int spu_streams_size;
    unsigned int spu_streams_current;
};

/* Make sure that the spu stream idx exists. */
static int __FASTCALL__ vobsub_ensure_spu_stream(vobsub_t *vob, unsigned int _index)
{
    if (_index >= vob->spu_streams_size) {
	/* This is a new stream */
	if (vob->spu_streams) {
	    packet_queue_t *tmp = (packet_queue_t*)mp_realloc(vob->spu_streams, (_index + 1) * sizeof(packet_queue_t));
	    if (tmp == NULL) {
		MSG_ERR("vobsub_ensure_spu_stream: mp_realloc failure");
		return -1;
	    }
	    vob->spu_streams = tmp;
	}
	else {
	    vob->spu_streams = new packet_queue_t[_index + 1];
	    if (vob->spu_streams == NULL) {
		MSG_ERR("vobsub_ensure_spu_stream: mp_malloc failure");
		return -1;
	    }
	}
	while (vob->spu_streams_size <= _index) {
	    packet_queue_construct(vob->spu_streams + vob->spu_streams_size);
	    ++vob->spu_streams_size;
	}
    }
    return 0;
}

static int __FASTCALL__ vobsub_add_id(vobsub_t *vob, const std::string& id, const unsigned int _index)
{
    if (vobsub_ensure_spu_stream(vob, _index) < 0)
	return -1;
    if (!id.empty()) {
	vob->spu_streams[_index].id=id;
    }
    vob->spu_streams_current = _index;
    if (mp_conf.verbose)
	mpxp_err<<"[vobsub] subtitle (vobsubid): "<<_index<<" language "<<vob->spu_streams[_index].id<<std::endl;
    return 0;
}

static int __FASTCALL__ vobsub_add_timestamp(vobsub_t *vob, off_t filepos, unsigned int ms)
{
    packet_queue_t *queue;
    packet_t *pkt;
    if (vob->spu_streams == 0) {
	MSG_WARN( "[vobsub] warning, binning some index entries.  Check your index file\n");
	return -1;
    }
    queue = vob->spu_streams + vob->spu_streams_current;
    if (packet_queue_grow(queue) >= 0) {
	pkt = queue->packets + (queue->packets_size - 1);
	pkt->filepos = filepos;
	pkt->pts100 = ms * 90;
	return 0;
    }
    return -1;
}

static int __FASTCALL__ vobsub_parse_id(vobsub_t *vob, const char *line)
{
    // id: xx, index: n
    size_t idlen;
    const char *p, *q;
    p  = line;
    while (isspace(*p))
	++p;
    q = p;
    while (isalpha(*q))
	++q;
    idlen = q - p;
    if (idlen == 0)
	return -1;
    ++q;
    while (isspace(*q))
	++q;
    if (strncmp("index:", q, 6))
	return -1;
    q += 6;
    while (isspace(*q))
	++q;
    if (!isdigit(*q))
	return -1;
    return vobsub_add_id(vob, p, atoi(q));
}

static int __FASTCALL__ vobsub_parse_timestamp(vobsub_t *vob, const char *line)
{
    // timestamp: HH:MM:SS.mmm, filepos: 0nnnnnnnnn
    const char *p;
    int h, m, s, ms;
    off_t filepos;
    while (isspace(*line))
	++line;
    p = line;
    while (isdigit(*p))
	++p;
    if (p - line != 2)
	return -1;
    h = atoi(line);
    if (*p != ':')
	return -1;
    line = ++p;
    while (isdigit(*p))
	++p;
    if (p - line != 2)
	return -1;
    m = atoi(line);
    if (*p != ':')
	return -1;
    line = ++p;
    while (isdigit(*p))
	++p;
    if (p - line != 2)
	return -1;
    s = atoi(line);
    if (*p != ':')
	return -1;
    line = ++p;
    while (isdigit(*p))
	++p;
    if (p - line != 3)
	return -1;
    ms = atoi(line);
    if (*p != ',')
	return -1;
    line = p + 1;
    while (isspace(*line))
	++line;
    if (strncmp("filepos:", line, 8))
	return -1;
    line += 8;
    while (isspace(*line))
	++line;
    if (! isxdigit(*line))
	return -1;
    filepos = strtol(line, NULL, 16);
    return vobsub_add_timestamp(vob, filepos, vob->delay + ms + 1000 * (s + 60 * (m + 60 * h)));
}

static int __FASTCALL__ vobsub_parse_size(vobsub_t *vob, const char *line)
{
    // size: WWWxHHH
    char *p;
    while (isspace(*line))
	++line;
    if (!isdigit(*line))
	return -1;
    vob->orig_frame_width = strtoul(line, &p, 10);
    if (*p != 'x')
	return -1;
    ++p;
    vob->orig_frame_height = strtoul(p, NULL, 10);
    return 0;
}

static int __FASTCALL__ vobsub_parse_origin(vobsub_t *vob, const char *line)
{
    // org: X,Y
    char *p;
    while (isspace(*line))
	++line;
    if (!isdigit(*line))
	return -1;
    vob->origin_x = strtoul(line, &p, 10);
    if (*p != ',')
	return -1;
    ++p;
    vob->origin_y = strtoul(p, NULL, 10);
    return 0;
}

static inline uint8_t clip_uint8(int a)
{
    if (a&(~255)) return (-a)>>31;
    else          return a;
}

unsigned int vobsub_palette_to_yuv(unsigned int pal)
{
    int r, g, b, y, u, v;
    // Palette in idx file is not rgb value, it was calculated by wrong forumla.
    // Here's reversed forumla of the one used to generate palette in idx file.
    r = pal >> 16 & 0xff;
    g = pal >> 8 & 0xff;
    b = pal & 0xff;
    y = clip_uint8( 0.1494  * r + 0.6061 * g + 0.2445 * b);
    u = clip_uint8( 0.6066  * r - 0.4322 * g - 0.1744 * b + 128);
    v = clip_uint8(-0.08435 * r - 0.3422 * g + 0.4266 * b + 128);
    y = y * 219 / 255 + 16;
    return y << 16 | u << 8 | v;
}

unsigned int vobsub_rgb_to_yuv(unsigned int rgb)
{
    int r, g, b, y, u, v;
    r = rgb >> 16 & 0xff;
    g = rgb >> 8 & 0xff;
    b = rgb & 0xff;
    y = ( 0.299   * r + 0.587   * g + 0.114   * b) * 219 / 255 + 16.5;
    u = (-0.16874 * r - 0.33126 * g + 0.5     * b) * 224 / 255 + 128.5;
    v = ( 0.5     * r - 0.41869 * g - 0.08131 * b) * 224 / 255 + 128.5;
    return y << 16 | u << 8 | v;
}

static int __FASTCALL__ vobsub_parse_palette(vobsub_t *vob, const char *line)
{
    // palette: XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX
    unsigned int n;
    n = 0;
    while (1) {
	const char *p;
	unsigned tmp;
	while (isspace(*line))
	    ++line;
	p = line;
	while (isxdigit(*p))
	    ++p;
	if (p - line != 6)
	    return -1;
	tmp = strtoul(line, NULL, 16);
	vob->palette[n++] = vobsub_palette_to_yuv(tmp);
	if (n == 16)
	    break;
	if (*p == ',')
	    ++p;
	line = p;
    }
    vob->have_palette = 1;
    return 0;
}

static int __FASTCALL__ vobsub_parse_custom(vobsub_t *vob, const char *line)
{
    //custom colors: OFF/ON(0/1)
    if ((strncmp("ON", line + 15, 2) == 0)||strncmp("1", line + 15, 1) == 0)
	vob->custom=1;
    else if ((strncmp("OFF", line + 15, 3) == 0)||strncmp("0", line + 15, 1) == 0)
	vob->custom=0;
    else
	return -1;
    return 0;
}

static int __FASTCALL__ vobsub_parse_cuspal(vobsub_t *vob, const char *line)
{
    //colors: XXXXXX, XXXXXX, XXXXXX, XXXXXX
    unsigned int n,tmp;
    n = 0;
    line += 40;
    while(1){
	const char *p;
	while (isspace(*line))
	    ++line;
	p=line;
	while (isxdigit(*p))
	    ++p;
	if (p - line !=6)
	    return -1;
	tmp = strtoul(line, NULL,16);
	vob->cuspal[n++] = vobsub_rgb_to_yuv(tmp);
	if (n==4)
	    break;
	if(*p == ',')
	    ++p;
	line = p;
    }
    return 0;
}

/* don't know how to use tridx */
static int __FASTCALL__ vobsub_parse_tridx(vobsub_t *vob, const char *line)
{
    //tridx: XXXX
    int tridx;
    UNUSED(vob);
    tridx = strtoul((line + 26), NULL, 16);
    tridx = ((tridx&0x1000)>>12) | ((tridx&0x100)>>7) | ((tridx&0x10)>>2) | ((tridx&1)<<3);
    return tridx;
}

static int __FASTCALL__ vobsub_parse_delay(vobsub_t *vob, const char *line)
{
    int h, m, s, ms;
    int forward = 1;
    if (*(line + 7) == '+'){
	forward = 1;
	line++;
    }
    else if (*(line + 7) == '-'){
	forward = -1;
	line++;
    }
    MSG_V( "forward=%d", forward);
    h = atoi(line + 7);
    MSG_V( "h=%d," ,h);
    m = atoi(line + 10);
    MSG_V( "m=%d,", m);
    s = atoi(line + 13);
    MSG_V( "s=%d,", s);
    ms = atoi(line + 16);
    MSG_V( "ms=%d", ms);
    vob->delay = ms + 1000 * (s + 60 * (m + 60 * h)) * forward;
    return 0;
}

static int __FASTCALL__ vobsub_set_lang(vobsub_t *vob, const char *line)
{
    UNUSED(vob);
    if (mp_conf.vobsub_id == -1)
	mp_conf.vobsub_id = atoi(line + 8);
    return 0;
}

static int __FASTCALL__ vobsub_parse_one_line(vobsub_t *vob, FILE *fd)
{
    ssize_t line_size;
    int res = -1;
    do {
	size_t line_reserve = 0;
	char *line = NULL;
	line_size = __getline(&line, &line_reserve, fd);
	if (line_size < 0) {
	    if (line)
		delete line;
	    break;
	}
	if (*line == 0 || *line == '\r' || *line == '\n' || *line == '#')
	    continue;
	else if (strncmp("langidx:", line, 8) == 0)
	    res = vobsub_set_lang(vob, line);
	else if (strncmp("delay:", line, 6) == 0)
	    res = vobsub_parse_delay(vob, line);
	else if (strncmp("id:", line, 3) == 0)
	    res = vobsub_parse_id(vob, line + 3);
	else if (strncmp("palette:", line, 8) == 0)
	    res = vobsub_parse_palette(vob, line + 8);
	else if (strncmp("size:", line, 5) == 0)
	    res = vobsub_parse_size(vob, line + 5);
	else if (strncmp("org:", line, 4) == 0)
	    res = vobsub_parse_origin(vob, line + 4);
	else if (strncmp("timestamp:", line, 10) == 0)
	    res = vobsub_parse_timestamp(vob, line + 10);
	else if (strncmp("custom colors:", line, 14) == 0)
	    //custom colors: ON/OFF, tridx: XXXX, colors: XXXXXX, XXXXXX, XXXXXX,XXXXXX
	    res = vobsub_parse_cuspal(vob, line) + vobsub_parse_tridx(vob, line) + vobsub_parse_custom(vob, line);
	else {
	    if (mp_conf.verbose)
		MSG_ERR( "vobsub: ignoring %s", line);
	    continue;
	}
	if (res < 0)
	    MSG_ERR( "ERROR in %s", line);
	break;
    } while (1);
    return res;
}

int __FASTCALL__ vobsub_parse_ifo(any_t* _vob, const std::string& name, unsigned int *palette, unsigned int *width, unsigned int *height, int force, int sid, std::string& langid)
{
    vobsub_t *vob = (vobsub_t*)_vob;
    int res = -1;
    FILE *fd = ::fopen(name.c_str(), "rb");
    if (fd == NULL) {
	if (force)
	    mpxp_warn<<"VobSub: Can't open IFO file"<<std::endl;
    } else {
	// parse IFO header
	unsigned char block[0x800];
	const char *const ifo_magic = "DVDVIDEO-VTS";
	if (fread(block, sizeof(block), 1, fd) != 1) {
	    if (force)
		mpxp_err<<"VobSub: Can't read IFO header"<<std::endl;
	} else if (memcmp(block, ifo_magic, strlen(ifo_magic) + 1))
	    mpxp_err<<"VobSub: Bad magic in IFO header"<<std::endl;
	else {
	    unsigned long pgci_sector = block[0xcc] << 24 | block[0xcd] << 16
		| block[0xce] << 8 | block[0xcf];
	    int standard = (block[0x200] & 0x30) >> 4;
	    int resolution = (block[0x201] & 0x0c) >> 2;
	    *height = standard ? 576 : 480;
	    *width = 0;
	    switch (resolution) {
	    case 0x0:
		*width = 720;
		break;
	    case 0x1:
		*width = 704;
		break;
	    case 0x2:
		*width = 352;
		break;
	    case 0x3:
		*width = 352;
		*height /= 2;
		break;
	    default:
		mpxp_warn<<"Vobsub: Unknown resolution "<<resolution<<std::endl;
	    }
	    if (!langid.empty() && 0 <= sid && sid < 32) {
		char *tmp = (char *)block + 0x256 + sid * 6 + 2;
		langid.assign(tmp,2);
	    }
	    if (fseek(fd, pgci_sector * sizeof(block), SEEK_SET)
		|| fread(block, sizeof(block), 1, fd) != 1)
		mpxp_err<<"VobSub: Can't read IFO PGCI"<<std::endl;
	    else {
		unsigned long idx;
		unsigned long pgc_offset = block[0xc] << 24 | block[0xd] << 16
		    | block[0xe] << 8 | block[0xf];
		for (idx = 0; idx < 16; ++idx) {
		    unsigned char *p = block + pgc_offset + 0xa4 + 4 * idx;
		    palette[idx] = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
		}
		if(vob)
		  vob->have_palette = 1;
		res = 0;
	    }
	}
	fclose(fd);
    }
    return res;
}

any_t* __FASTCALL__ vobsub_open(const std::string& name,const char *const ifo,const int force,any_t** spu)
{
    vobsub_t *vob = new(zeromem) vobsub_t;
    if(spu)
      *spu = NULL;
    if (vob) {
	vob->custom = 0;
	vob->have_palette = 0;
	vob->orig_frame_width = 0;
	vob->orig_frame_height = 0;
	vob->spu_streams = NULL;
	vob->spu_streams_size = 0;
	vob->spu_streams_current = 0;
	vob->delay = 0;
	vob->forced_subs=0;
	std::string buf;
	FILE *fd;
	mpeg_t *mpg;
	/* read in the info file */
	std::string stmp="";
	if(!ifo) {
	    buf=name+".ifo";
	    vobsub_parse_ifo(vob,buf, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, stmp);
	} else
	    vobsub_parse_ifo(vob,ifo, vob->palette, &vob->orig_frame_width, &vob->orig_frame_height, force, -1, stmp);
	/* read in the index */
	buf=name+".idx";
	fd = ::fopen(buf.c_str(), "rb");
	if (fd == NULL) {
	    if(force)
		mpxp_err<<"VobSub: Can't open IDX file"<<std::endl;
	    else {
		delete vob;
		return NULL;
	    }
	} else {
	    while (vobsub_parse_one_line(vob, fd) >= 0) /* NOOP */ ;
	    ::fclose(fd);
	}
	/* if no palette in .idx then use custom colors */
	if ((vob->custom == 0)&&(vob->have_palette!=1))
	    vob->custom = 1;
	if (spu && vob->orig_frame_width && vob->orig_frame_height)
	    *spu = spudec_new_scaled_vobsub(vob->palette, vob->cuspal, vob->custom, vob->orig_frame_width, vob->orig_frame_height);

	/* read the indexed mpeg_stream */
	buf=name+".sub";
	mpg = mpeg_open(buf);
	if (mpg == NULL) {
	    if(force) mpxp_err<<"VobSub: Can't open SUB file"<<std::endl;
	    else {
		delete vob;
		return NULL;
	    }
	} else {
	    long last_pts_diff = 0;
	    while (!mpeg_eof(mpg)) {
		off_t pos = mpeg_tell(mpg);
		if (mpeg_run(mpg) < 0) {
		    if (!mpeg_eof(mpg)) mpxp_err<<"VobSub: mpeg_run error"<<std::endl;
		    break;
		}
		if (mpg->packet_size) {
		    if ((mpg->aid & 0xe0) == 0x20) {
			unsigned int sid = mpg->aid & 0x1f;
			if (vobsub_ensure_spu_stream(vob, sid) >= 0)  {
			    packet_queue_t *queue = vob->spu_streams + sid;
			    /* get the packet to fill */
			    if (queue->packets_size == 0 && packet_queue_grow(queue)  < 0)
				abort();
			    while (queue->current_index + 1 < queue->packets_size
				    && queue->packets[queue->current_index + 1].filepos <= pos)
				++queue->current_index;
			    if (queue->current_index < queue->packets_size) {
				packet_t *pkt;
				if (queue->packets[queue->current_index].data) {
				    /* insert a new packet and fix the PTS ! */
				    packet_queue_insert(queue);
				    queue->packets[queue->current_index].pts100 =
						mpg->pts + last_pts_diff;
				}
				pkt = queue->packets + queue->current_index;
				if (pkt->pts100 != UINT_MAX) {
				if (queue->packets_size > 1)
				    last_pts_diff = pkt->pts100 - mpg->pts;
				else
				    pkt->pts100 = mpg->pts;
				/* FIXME: should not use mpg_sub internal informations, make a copy */
				pkt->data = mpg->packet;
				pkt->size = mpg->packet_size;
				mpg->packet = NULL;
				mpg->packet_reserve = 0;
				mpg->packet_size = 0;
				}
			    }
			}
			else mpxp_warn<<"don't know what to do with subtitle #"<<sid<<std::endl;
		    }
		}
	    }
	    vob->spu_streams_current = vob->spu_streams_size;
	    while (vob->spu_streams_current-- > 0)
		vob->spu_streams[vob->spu_streams_current].current_index = 0;
	    mpeg_free(mpg);
	}
    }
    return vob;
}

void __FASTCALL__ vobsub_close(any_t*self)
{
    vobsub_t *vob = (vobsub_t *)self;
    if (vob->spu_streams) {
	while (vob->spu_streams_size--)
	    packet_queue_destroy(vob->spu_streams + vob->spu_streams_size);
	delete vob->spu_streams;
    }
    delete vob;
}

void __FASTCALL__ vobsub_reset(any_t*vobhandle)
{
    vobsub_t *vob = (vobsub_t *)vobhandle;
    if (vob->spu_streams) {
	unsigned int n = vob->spu_streams_size;
	while (n-- > 0)
	    vob->spu_streams[n].current_index = 0;
    }
}

std::string __FASTCALL__ vobsub_get_id(any_t*vobhandle, unsigned int _index)
{
    vobsub_t *vob = (vobsub_t *) vobhandle;
    return (_index < vob->spu_streams_size) ? vob->spu_streams[_index].id : "";
}

unsigned int __FASTCALL__ vobsub_get_forced_subs_flag(void const * const vobhandle)
{
  if (vobhandle)
    return ((vobsub_t*) vobhandle)->forced_subs;
  else
    return 0;
}

int __FASTCALL__ vobsub_set_from_lang(any_t*vobhandle,const std::string& lang)
{
    unsigned i;
    size_t pos=0;
    vobsub_t *vob= (vobsub_t *) vobhandle;
    while(!lang.empty() && lang.length() >= 2){
      for(i=0; i < vob->spu_streams_size; i++)
	if (!vob->spu_streams[i].id.empty())
	  if (vob->spu_streams[i].id==lang.substr(pos,2)){
	    mp_conf.vobsub_id=i;
	    mpxp_info<<"Selected VOBSUB language: "<<i<<" language: "<<vob->spu_streams[i].id<<std::endl;
	    return 0;
	  }
      pos+=2;while (lang[pos]==',' || lang[pos]==' ') ++pos;
    }
    mpxp_warn<<"No matching VOBSUB language found!"<<std::endl;
    return -1;
}

void __FASTCALL__ vobsub_seek(any_t* vobhandle,const seek_args_t* seeka)
{
    vobsub_t * vob = (vobsub_t *)vobhandle;
    packet_queue_t * queue;
    unsigned seek_pts100;
    if(seeka->flags&DEMUX_SEEK_SET) seek_pts100 = (unsigned)seeka->secs * 90000;
    else {
	int cur_pts;
	queue = vob->spu_streams + mp_conf.vobsub_id;
	cur_pts=(queue->packets + queue->current_index)->pts100;
	seek_pts100 = cur_pts+(unsigned)seeka->secs*90000;
    }
    if (vob->spu_streams && 0 <= mp_conf.vobsub_id && (unsigned) mp_conf.vobsub_id < vob->spu_streams_size) {
	/* do not seek if we don't know the id */
	if (vobsub_get_id(vob, mp_conf.vobsub_id).empty()) return;
	queue = vob->spu_streams + mp_conf.vobsub_id;
	queue->current_index = 0;
	while (queue->current_index < queue->packets_size
	    && (queue->packets + queue->current_index)->pts100 < seek_pts100)
	    ++queue->current_index;
	if (queue->current_index > 0)
	    --queue->current_index;
    }
}

int __FASTCALL__ vobsub_get_packet(any_t*vobhandle, float pts,any_t** data, int* timestamp) {
  vobsub_t *vob = (vobsub_t *)vobhandle;
  unsigned int pts100 = 90000 * pts;
  if (vob->spu_streams && 0 <= mp_conf.vobsub_id && (unsigned) mp_conf.vobsub_id < vob->spu_streams_size) {
    packet_queue_t *queue = vob->spu_streams + mp_conf.vobsub_id;
    while (queue->current_index < queue->packets_size) {
      packet_t *pkt = queue->packets + queue->current_index;
      if (pkt->pts100 != UINT_MAX)
      if (pkt->pts100 <= pts100) {
	++queue->current_index;
	*data = pkt->data;
	*timestamp = pkt->pts100;
	return pkt->size;
      } else break;
      else
	++queue->current_index;
    }
  }
  return -1;
}
