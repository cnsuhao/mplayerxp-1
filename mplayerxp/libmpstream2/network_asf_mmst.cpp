#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
// mmst implementation taken from the xine-mms plugin made by majormms (http://geocities.com/majormms/)
//
// ported to mplayer by Abhijeet Phatak <abhijeetphatak@yahoo.com>
// date : 16 April 2002
//
// information about the mms protocol can be find at http://get.to/sdp
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include "mplayerxp.h"
#include "tcp.h"
#include "url.h"
#include "stream.h"

#include "network.h"
#include "network_asf_mmst.h"

#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#endif
#ifndef USE_SETLOCALE
#undef USE_ICONV
#endif

#ifdef USE_ICONV
#ifdef HAVE_GICONV
#include <giconv.h>
#else
#include <iconv.h>
#endif
#ifdef USE_LANGINFO
#include <langinfo.h>
#endif
#endif
#include "stream_msg.h"

namespace mpxp {
#define BUF_SIZE 102400
#define HDR_BUF_SIZE 8192
#define MAX_STREAMS 20

typedef struct {
  uint8_t buf[BUF_SIZE];
  int     num_bytes;

} command_t;

static int seq_num;
static int num_stream_ids;
static int stream_ids[MAX_STREAMS];

static int get_data (Tcp& s,unsigned char *buf, size_t count);

static void put_32 (command_t *cmd, uint32_t value)
{
  cmd->buf[cmd->num_bytes  ] = value % 256;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+1] = value % 256 ;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+2] = value % 256 ;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+3] = value % 256 ;

  cmd->num_bytes += 4;
}

static uint32_t get_32 (unsigned char *cmd, int offset)
{
  uint32_t ret;

  ret = cmd[offset] ;
  ret |= cmd[offset+1]<<8 ;
  ret |= cmd[offset+2]<<16 ;
  ret |= cmd[offset+3]<<24 ;

  return ret;
}

static void send_command (Tcp& tcp, int command, uint32_t switches,
			  uint32_t extra, int length,
			  unsigned char *data)
{
  command_t  cmd;
  int        len8;

  len8 = (length + 7) / 8;

  cmd.num_bytes = 0;

  put_32 (&cmd, 0x00000001); /* start sequence */
  put_32 (&cmd, 0xB00BFACE); /* #-)) */
  put_32 (&cmd, len8*8 + 32);
  put_32 (&cmd, 0x20534d4d); /* protocol type "MMS " */
  put_32 (&cmd, len8 + 4);
  put_32 (&cmd, seq_num);
  seq_num++;
  put_32 (&cmd, 0x0);        /* unknown */
  put_32 (&cmd, 0x0);
  put_32 (&cmd, len8+2);
  put_32 (&cmd, 0x00030000 | command); /* dir | command */
  put_32 (&cmd, switches);
  put_32 (&cmd, extra);

  memcpy (&cmd.buf[48], data, length);
  if (length & 7)
    memset(&cmd.buf[48 + length], 0, 8 - (length & 7));

  if (tcp.write (cmd.buf, len8*8+48) != (len8*8+48)) {
    mpxp_err<<"write error"<<std::endl;
  }
}

#ifdef USE_ICONV
static iconv_t url_conv;
#endif

static void string_utf16(unsigned char *dest,const char *src, int len)
{
    int i;
#ifdef USE_ICONV
    size_t len1, len2;
    char *ip, *op;

    if (url_conv != (iconv_t)(-1))
    {
    memset(dest, 0, 1000);
    len1 = len; len2 = 1000;
    ip = src; op = dest;

    iconv(url_conv, &ip, &len1, &op, &len2);
    }
    else
    {
#endif
	if (len > 499) len = 499;
	for (i=0; i<len; i++) {
	    dest[i*2] = src[i];
	    dest[i*2+1] = 0;
	}
	/* trailing zeroes */
	dest[i*2] = 0;
	dest[i*2+1] = 0;
#ifdef USE_ICONV
    }
#endif
}

static void get_answer (Tcp& tcp)
{
  unsigned char data[BUF_SIZE];
  int   command = 0x1b;

  while (command == 0x1b) {
    int len;

    len = tcp.read(data, BUF_SIZE);
    if (!len) {
      mpxp_err<<std::endl<<"alert! eof"<<std::endl;
      return;
    }

    command = get_32 (data, 36) & 0xFFFF;

    if (command == 0x1b)
      send_command (tcp, 0x1b, 0, 0, 0, data);
  }
}

static int get_data (Tcp& tcp,unsigned char *buf, size_t count)
{
  ssize_t  len;
  size_t total = 0;

  while (total < count) {

    len = tcp.read(&buf[total], count-total);

    if (len<=0) {
      mpxp_err<<"read error:"<<std::endl;
      return 0;
    }

    total += len;

    if (len != 0) {
      fflush (stdout);
    }

  }

  return 1;

}

int Asf_Mmst_Networking::get_header (Tcp& tcp, uint8_t *header)
{
  unsigned char  pre_header[8];
  int            header_len;

  header_len = 0;

  while (1) {
    if (!get_data (tcp, pre_header, 8)) {
      mpxp_err<<"pre-header read failed"<<std::endl;
      return 0;
    }
    if (pre_header[4] == 0x02) {

      int packet_len;

      packet_len = (pre_header[7] << 8 | pre_header[6]) - 8;

      if (packet_len < 0 || packet_len > HDR_BUF_SIZE - header_len) {
	mpxp_fatal<<"Invalid header size, giving up"<<std::endl;
	return 0;
      }

      if (!get_data (tcp, &header[header_len], packet_len)) {
	mpxp_err<<"header data read failed"<<std::endl;
	return 0;
      }

      header_len += packet_len;

    if ( (header[header_len-1] == 1) && (header[header_len-2]==1)) {


    if( bufferize(header, header_len )<0 ) return -1;

    return header_len;

      }

    } else {

      int32_t packet_len;
      int command;
      unsigned char _data[BUF_SIZE];

      if (!get_data (tcp, (unsigned char*)&packet_len, 4)) {
	mpxp_err<<"packet_len read failed"<<std::endl;
	return 0;
      }

      packet_len = get_32 ((unsigned char*)&packet_len, 0) + 4;

      if (packet_len < 0 || packet_len > BUF_SIZE) {
	mpxp_fatal<<"Invalid rtsp packet size, giving up"<<std::endl;
	return 0;
      }

      if (!get_data (tcp, _data, packet_len)) {
	mpxp_err<<"command data read failed"<<std::endl;
	return 0;
      }

      command = get_32 (_data, 24) & 0xFFFF;

      if (command == 0x1b)
	send_command (tcp, 0x1b, 0, 0, 0, _data);

    }

  }
}

static int interp_header (uint8_t *header, int header_len)
{
  int i;
  int packet_length=-1;

  /*
   * parse header
   */

  i = 30;
  while (i<header_len) {

    uint64_t  guid_1, guid_2, length;

    guid_2 = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8)
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);
    i += 8;

    guid_1 = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8)
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);
    i += 8;

    length = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8)
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);

    i += 8;

    if ( (guid_1 == 0x6cce6200aa00d9a6ULL) && (guid_2 == 0x11cf668e75b22630ULL) ) {
      mpxp_v<<"header object"<<std::endl;
    } else if ((guid_1 == 0x6cce6200aa00d9a6ULL) && (guid_2 == 0x11cf668e75b22636ULL)) {
      mpxp_v<<"data object"<<std::endl;
    } else if ((guid_1 == 0x6553200cc000e48eULL) && (guid_2 == 0x11cfa9478cabdca1ULL)) {

      packet_length = get_32(header, i+92-24);

      mpxp_v<<"file object, packet length = "<<packet_length<<" ("<<get_32(header, i+96-24)<<")"<<std::endl;

    } else if ((guid_1 == 0x6553200cc000e68eULL) && (guid_2 == 0x11cfa9b7b7dc0791ULL)) {

      int stream_id = header[i+48] | header[i+49] << 8;

      mpxp_v<<"stream object, stream id: "<<stream_id<<std::endl;

      if (num_stream_ids < MAX_STREAMS) {
	stream_ids[num_stream_ids] = stream_id;
	num_stream_ids++;
      } else mpxp_err<<"asf_mmst: too many id, stream skipped"<<std::endl;
    } else mpxp_v<<"unknown object"<<std::endl;
    i += length-24;
  }
  return packet_length;
}

int Asf_Mmst_Networking::get_media_packet (Tcp& tcp, int padding) {
  unsigned char  pre_header[8];
  unsigned char  _data[BUF_SIZE];

  if (!get_data (tcp, pre_header, 8)) {
    mpxp_err<<"pre-header read failed"<<std::endl;
    return 0;
  }

  if (pre_header[4] == 0x04) {

    int packet_len;

    packet_len = (pre_header[7] << 8 | pre_header[6]) - 8;

    if (packet_len < 0 || packet_len > BUF_SIZE) {
      mpxp_fatal<<"Invalid rtsp packet size, giving up"<<std::endl;
      return 0;
    }

    if (!get_data (tcp, _data, packet_len)) {
      mpxp_err<<"media data read failed"<<std::endl;
      return 0;
    }

    bufferize(_data, padding);

  } else {

    int32_t packet_len;
    int command;

    if (!get_data (tcp, (unsigned char*)&packet_len, 4)) {
      mpxp_err<<"packet_len read failed"<<std::endl;
      return 0;
    }

    packet_len = get_32 ((unsigned char*)&packet_len, 0) + 4;

    if (packet_len < 0 || packet_len > BUF_SIZE) {
	mpxp_fatal<<"Invalid rtsp packet size, giving up"<<std::endl;
	return 0;
    }

    if (!get_data (tcp, _data, packet_len)) {
      mpxp_err<<"command data read failed"<<std::endl;
      return 0;
    }

    if ( (pre_header[7] != 0xb0) || (pre_header[6] != 0x0b)
	 || (pre_header[5] != 0xfa) || (pre_header[4] != 0xce) ) {

      mpxp_err<<"missing signature"<<std::endl;
      return -1;
    }

    command = get_32 (_data, 24) & 0xFFFF;

    if (command == 0x1b)
      send_command (tcp, 0x1b, 0, 0, 0, _data);
    else if (command == 0x1e) {
      mpxp_ok<<"everything done. Thank you for downloading a media file containing proprietary and patentend technology"<<std::endl;
      return 0;
    }
    else if (command == 0x21 ) {
	// Looks like it's new in WMS9
	// Unknown command, but ignoring it seems to work.
	return 0;
    }
    else if (command != 0x05) {
      mpxp_err<<"unknown command "<<std::hex<<command<<std::endl;
      return -1;
    }
  }

  return 1;
}


static int packet_length1;

int Asf_Mmst_Networking::read(Tcp& tcp, char *_buffer, int size)
{
    int len;

    while( buffer_size==0 ) {
	// buffer is empty - fill it!
	int ret = get_media_packet(tcp, packet_length1);
	if( ret<0 ) {
	    mpxp_err<<"get_media_packet error: "<<strerror(errno)<<std::endl;
	    return -1;
	} else if (ret==0) return ret; // EOF?
    }
    len = buffer_size-buffer_pos;
    if(len>size) len=size;
    memcpy( _buffer, buffer+buffer_pos, len );
    buffer_pos += len;
    if( buffer_pos>=buffer_size ) {
	delete buffer ;
	buffer = NULL;
	buffer_size = 0;
	buffer_pos = 0;
    }
    return len;
}

int Asf_Mmst_Networking::seek(Tcp& tcp, off_t pos)
{
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

Networking* Asf_Mmst_Networking::start(Tcp& tcp, network_protocol_t& protocol)
{
    char	str[1024];
    uint8_t	data[BUF_SIZE];
    uint8_t	asf_header[HDR_BUF_SIZE];
    int		asf_header_len;
    int		len, i, packet_length;
    const char*	path;
    char*	unescpath;
    URL		url1 = protocol.url;

    // Is protocol even valid mms,mmsu,mmst,http,http_proxy?
    if (!(protocol.url.protocol2lower()=="mmst" ||
	 protocol.url.protocol2lower()=="mmsu" ||
	 protocol.url.protocol2lower()=="mms")) {
	mpxp_err<<"Unknown protocol: "<<protocol.url.protocol()<<std::endl;
	return NULL;
    }

    // Is protocol mms or mmsu?
    if (protocol.url.protocol2lower()=="mmsu" || protocol.url.protocol2lower()=="mms") {
	mpxp_v<<"Trying ASF/UDP..."<<std::endl;
	//fd = asf_mmsu_networking_start( stream );
	//mmsu support is not implemented yet - using this code
	mpxp_v<<"  ===> ASF/UDP failed"<<std::endl;
	return NULL;
    }

    tcp.close();

    /* parse url */
    path = strchr(url1.file().c_str(),'/') + 1;

    /* mmst filename are not url_escaped by MS MediaPlayer and are expected as
    * "plain text" by the server, so need to decode it here
    */
    unescpath=new char [strlen(path)+1];
    if (!unescpath) {
	mpxp_fatal<<"Memory allocation failed!"<<std::endl;
	return NULL;
    }
    url2string(unescpath,path);
    path=unescpath;

    url1.assign_port(1755);
    tcp.open(url1, Tcp::IP4);
    if( !tcp.established()) {
	delete path;
	return NULL;
    }
    mpxp_info<<"connected"<<std::endl;

    seq_num=0;

    /*
    * Send the initial connect info including player version no. Client GUID (random) and the host address being connected to.
    * This command is sent at the very start of protocol initiation. It sends local information to the serve
    * cmd 1 0x01
    * */

    /* prepare for the url encoding conversion */
#ifdef USE_ICONV
#ifdef USE_LANGINFO
    url_conv = iconv_open("UTF-16LE",nl_langinfo(CODESET));
#else
    url_conv = iconv_open("UTF-16LE", NULL);
#endif
#endif

    snprintf (str, 1023, "\034\003NSPlayer/7.0.0.1956; {33715801-BAB3-9D85-24E9-03B90328270A}; Host: %s", url1.host().c_str());
    string_utf16 (data, str, strlen(str));
// send_command(s, commandno ....)
    send_command (tcp, 1, 0, 0x0004000b, strlen(str) * 2+2, data);

    len = tcp.read (data, BUF_SIZE);

    /*This sends details of the local machine IP address to a Funnel system at the server.
    * Also, the TCP or UDP transport selection is sent.
    *
    * here 192.168.0.129 is local ip address TCP/UDP states the tronsport we r using
    * and 1037 is the  local TCP or UDP socket number
    * cmd 2 0x02
    *  */

    string_utf16 (&data[8], "\002\000\\\\192.168.0.1\\TCP\\1037", 24);
    memset (data, 0, 8);
    send_command (tcp, 2, 0, 0, 24*2+10, data);

    len = tcp.read(data, BUF_SIZE);

    /* This command sends file path (at server) and file name request to the server.
    * 0x5 */

    string_utf16 (&data[8], path, strlen(path));
    memset (data, 0, 8);
    send_command (tcp, 5, 0, 0, strlen(path)*2+10, data);
    delete path;

    get_answer (tcp);

    /* The ASF header chunk request. Includes ?session' variable for pre header value.
    * After this command is sent,
    * the server replies with 0x11 command and then the header chunk with header data follows.
    * 0x15 */

    memset (data, 0, 40);
    data[32] = 2;

    send_command (tcp, 0x15, 1, 0, 40, data);

    num_stream_ids = 0;
    /* get_headers(s, asf_header);  */

    Asf_Mmst_Networking* rv = new(zeromem) Asf_Mmst_Networking;
    asf_header_len = rv->get_header (tcp, asf_header);
    if (asf_header_len==0) { //error reading header
	tcp.close();
	delete rv;
	return NULL;
    }
    packet_length = interp_header (asf_header, asf_header_len);

    /*
    * This command is the media stream MBR selector. Switches are always 6 bytes in length.
    * After all switch elements, data ends with bytes [00 00] 00 20 ac 40 [02].
    * Where:
    * [00 00] shows 0x61 0x00 (on the first 33 sent) or 0xff 0xff for ASF files, and with no ending data for WMV files.
    * It is not yet understood what all this means.
    * And the last [02] byte is probably the header ?session' value.
    *
    *  0x33 */

    memset (data, 0, 40);

    if (mp_conf.audio_id > 0) {
	data[2] = 0xFF;
	data[3] = 0xFF;
	data[4] = mp_conf.audio_id;
	send_command(tcp, 0x33, num_stream_ids, 0xFFFF | mp_conf.audio_id << 16, 8, data);
    } else {
	for (i=1; i<num_stream_ids; i++) {
	    data [ (i-1) * 6 + 2 ] = 0xFF;
	    data [ (i-1) * 6 + 3 ] = 0xFF;
	    data [ (i-1) * 6 + 4 ] = stream_ids[i];
	    data [ (i-1) * 6 + 5 ] = 0x00;
	}
	send_command (tcp, 0x33, num_stream_ids, 0xFFFF | stream_ids[0] << 16, (num_stream_ids-1)*6+2 , data);
    }

    get_answer (tcp);

    /* Start sending file from packet xx.
    * This command is also used for resume downloads or requesting a lost packet.
    * Also used for seeking by sending a play point value which seeks to the media time point.
    * Includes ?session' value in pre header and the maximum media stream time.
    * 0x07 */

    memset (data, 0, 40);

    for (i=8; i<16; i++) data[i] = 0xFF;
    data[20] = 0x04;

    send_command (tcp, 0x07, 1, 0xFFFF | stream_ids[0] << 16, 24, data);

    rv->buffering = 1;
    rv->status = networking_playing_e;

    packet_length1 = packet_length;
    mpxp_v<<"mmst packet_length = "<<packet_length<<std::endl;

#ifdef USE_ICONV
    if (url_conv != (iconv_t)(-1)) iconv_close(url_conv);
#endif
    return rv;
}
Asf_Mmst_Networking::Asf_Mmst_Networking() {}
Asf_Mmst_Networking::~Asf_Mmst_Networking() {}
} // namespace mpxp
