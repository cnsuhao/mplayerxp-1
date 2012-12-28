#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a mp_free video player.
 *
 * xine is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: pnm.c,v 1.3 2007/12/12 08:20:02 nickols_k Exp $
 *
 * pnm protocol implementation
 * based upon code from joschka
 */

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>

#include "osdep/bswap.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
#else
#include <winsock2.h>
#endif

#include "tcp.h"
#include "pnm.h"
#include "stream_msg.h"

namespace mpxp {
#define FOURCC_TAG( ch0, ch1, ch2, ch3 ) \
	(((long)(unsigned char)(ch3)       ) | \
	( (long)(unsigned char)(ch2) << 8  ) | \
	( (long)(unsigned char)(ch1) << 16 ) | \
	( (long)(unsigned char)(ch0) << 24 ) )


static const uint32_t RMF_TAG  =FOURCC_TAG('.', 'R', 'M', 'F');
static const uint32_t PROP_TAG =FOURCC_TAG('P', 'R', 'O', 'P');
static const uint32_t MDPR_TAG =FOURCC_TAG('M', 'D', 'P', 'R');
static const uint32_t CONT_TAG =FOURCC_TAG('C', 'O', 'N', 'T');
static const uint32_t DATA_TAG =FOURCC_TAG('D', 'A', 'T', 'A');
static const uint32_t INDX_TAG =FOURCC_TAG('I', 'N', 'D', 'X');
static const uint32_t PNA_TAG  =FOURCC_TAG('P', 'N', 'A',  0 );

/*
#define LOG
*/

/*
 * utility macros
 */

static inline uint16_t BE_16(uint8_t* x) { return be2me_16(*((uint16_t *)x)); }
static inline uint32_t BE_32(uint8_t* x) { return be2me_32(*((uint32_t *)x)); }

/* D means direct (no pointer) */
static inline uint16_t BE_16D(uint8_t* x) { return BE_16(x); }

/* sizes */
static const int PREAMBLE_SIZE=8;
static const int CHECKSUM_SIZE=3;

/* header of rm files */
static const int RM_HEADER_SIZE=0x12;
static const unsigned char rm_header[]={
	0x2e, 0x52, 0x4d, 0x46, /* object_id      ".RMF" */
	0x00, 0x00, 0x00, 0x12, /* header_size    0x12   */
	0x00, 0x00,             /* object_version 0x00   */
	0x00, 0x00, 0x00, 0x00, /* file_version   0x00   */
	0x00, 0x00, 0x00, 0x06  /* num_headers    0x06   */
};

/* data chunk header */
static const int PNM_DATA_HEADER_SIZE=18;
static const unsigned char pnm_data_header[]={
	'D','A','T','A',
	 0,0,0,0,       /* data chunk size  */
	 0,0,           /* object version   */
	 0,0,0,0,       /* num packets      */
	 0,0,0,0};      /* next data header */

/* pnm request chunk ids */

static const int PNA_CLIENT_CAPS      =0x03;
static const int PNA_CLIENT_CHALLANGE =0x04;
static const int PNA_BANDWIDTH        =0x05;
static const int PNA_GUID             =0x13;
static const int PNA_TIMESTAMP        =0x17;
static const int PNA_TWENTYFOUR       =0x18;

static const int PNA_CLIENT_STRING    =0x63;
static const int PNA_PATH_REQUEST     =0x52;

static const char pnm_challenge[] = "0990f6b4508b51e801bd6da011ad7b56";
static const char pnm_timestamp[] = "[15/06/1999:22:22:49 00:00]";
static const char pnm_guid[]      = "3eac2411-83d5-11d2-f3ea-d7c3a51aa8b0";
static const char pnm_response[]  = "97715a899cbe41cee00dd434851535bf";
static const char client_string[] = "WinNT_4.0_6.0.6.45_plus32_MP60_en-US_686l";

static const int PNM_HEADER_SIZE=11;
static const unsigned char pnm_header[] = {
	'P','N','A',
	0x00, 0x0a,
	0x00, 0x14,
	0x00, 0x02,
	0x00, 0x01 };

static const int PNM_CLIENT_CAPS_SIZE=126;
static const unsigned char pnm_client_caps[] = {
    0x07, 0x8a, 'p','n','r','v',
       0, 0x90, 'p','n','r','v',
       0, 0x64, 'd','n','e','t',
       0, 0x46, 'p','n','r','v',
       0, 0x32, 'd','n','e','t',
       0, 0x2b, 'p','n','r','v',
       0, 0x28, 'd','n','e','t',
       0, 0x24, 'p','n','r','v',
       0, 0x19, 'd','n','e','t',
       0, 0x18, 'p','n','r','v',
       0, 0x14, 's','i','p','r',
       0, 0x14, 'd','n','e','t',
       0, 0x24, '2','8','_','8',
       0, 0x12, 'p','n','r','v',
       0, 0x0f, 'd','n','e','t',
       0, 0x0a, 's','i','p','r',
       0, 0x0a, 'd','n','e','t',
       0, 0x08, 's','i','p','r',
       0, 0x06, 's','i','p','r',
       0, 0x12, 'l','p','c','J',
       0, 0x07, '0','5','_','6' };

static const uint32_t pnm_default_bandwidth=10485800;
static const uint32_t pnm_available_bandwidths[]={14400,19200,28800,33600,34430,57600,
				  115200,262200,393216,524300,1544000,10485800};

static const int PNM_TWENTYFOUR_SIZE=16;
static unsigned char pnm_twentyfour[]={
    0xd5, 0x42, 0xa3, 0x1b, 0xef, 0x1f, 0x70, 0x24,
    0x85, 0x29, 0xb3, 0x8d, 0xba, 0x11, 0xf3, 0xd6 };

/* now other data follows. marked with 0x0000 at the beginning */
static int after_chunks_length=6;
static const unsigned char after_chunks[]={
    0x00, 0x00, /* mark */

    0x50, 0x84, /* seems to be fixated */
    0x1f, 0x3a  /* varies on each request (checksum ?)*/
    };

static void hexdump (char *buf, int length);

Pnm::Pnm(Tcp& _tcp):tcp(_tcp) {}
Pnm::~Pnm() {}

static int rm_write(Tcp& tcp, const char *buf, int len) {
  int total, timeout;

  total = 0; timeout = 30;
  while (total < len){
    int n;

    n = tcp.write ((const uint8_t*)&buf[total], len - total);

    if (n > 0)
      total += n;
    else if (n < 0) {
#ifndef HAVE_WINSOCK2
      if ((timeout>0) && ((errno == EAGAIN) || (errno == EINPROGRESS))) {
#else
      if ((timeout>0) && ((errno == EAGAIN) || (WSAGetLastError() == WSAEINPROGRESS))) {
#endif
	sleep (1); timeout--;
      } else
	return -1;
    }
  }

  return total;
}

static ssize_t rm_read(Tcp& tcp, any_t*buf, size_t count) {

  ssize_t ret, total;

  total = 0;

  while (total < count) {

    ret=tcp.read (((uint8_t*)buf)+total, count-total);

    if (ret<=0) {
      mpxp_info<<"input_pnm: read error"<<std::endl;
      return ret;
    } else
      total += ret;
  }

  return total;
}

/*
 * debugging utilities
 */

static void hexdump (char *buf, int length) {

  int i;

  mpxp_info<<"input_pnm: ascii>";
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    if ((c >= 32) && (c <= 128))
      mpxp_info<<c;
    else
      mpxp_info<<".";
  }
  mpxp_info<<std::endl;
}

/*
 * pnm_get_chunk gets a chunk from stream
 * and returns number of bytes read
 */

unsigned int Pnm::get_chunk(unsigned int max,
			 unsigned int *chunk_type,
			 char *data, int *need_response) const {

  unsigned int chunk_size;
  unsigned int n;
  char *ptr;

  if (max < PREAMBLE_SIZE)
    return -1;

  /* get first PREAMBLE_SIZE bytes and ignore checksum */
  rm_read (tcp, data, CHECKSUM_SIZE);
  if (data[0] == 0x72)
    rm_read (tcp, data, PREAMBLE_SIZE);
  else
    rm_read (tcp, data+CHECKSUM_SIZE, PREAMBLE_SIZE-CHECKSUM_SIZE);

  max -= PREAMBLE_SIZE;

  *chunk_type = BE_32((uint8_t*)data);
  chunk_size = BE_32((uint8_t*)(data+4));

  switch (*chunk_type) {
    case PNA_TAG:
      *need_response=0;
      ptr=data+PREAMBLE_SIZE;
      if (max < 1)
	return -1;
      rm_read (tcp, ptr++, 1);
      max -= 1;

      while(1) {
	/* expecting following chunk format: 0x4f <chunk size> <data...> */

	if (max < 2)
	  return -1;
	rm_read (tcp, ptr, 2);
	max -= 2;
	if (*ptr == 'X') /* checking for server message */
	{
	  mpxp_info<<"input_pnm: got a message from server:"<<std::endl;
	  if (max < 1)
	    return -1;
	  rm_read (tcp, ptr+2, 1);
	  max = -1;
	  n=BE_16((uint8_t*)(ptr+1));
	  if (max < n)
	    return -1;
	  rm_read (tcp, ptr+3, n);
	  max -= n;
	  ptr[3+n]=0;
	  mpxp_info<<(ptr+3)<<std::endl;
	  return -1;
	}

	if (*ptr == 'F') /* checking for server error */
	{
	  mpxp_info<<"input_pnm: server error"<<std::endl;
	  return -1;
	}
	if (*ptr == 'i')
	{
	  ptr+=2;
	  *need_response=1;
	  continue;
	}
	if (*ptr != 0x4f) break;
	n=ptr[1];
	if (max < n)
	  return -1;
	rm_read (tcp, ptr+2, n);
	max -= n;
	ptr+=(n+2);
      }
      /* the checksum of the next chunk is ignored here */
      if (max < 1)
	return -1;
      rm_read (tcp, ptr+2, 1);
      ptr+=3;
      chunk_size=ptr-data;
      break;
    case RMF_TAG:
    case DATA_TAG:
    case PROP_TAG:
    case MDPR_TAG:
    case CONT_TAG:
      if (chunk_size > max || chunk_size < PREAMBLE_SIZE) {
	mpxp_info<<"error: max chunk size exeeded (max was 0x"<<std::hex<<max<<")"<<std::endl;
#ifdef LOG
	n=rm_read (tcp, &data[PREAMBLE_SIZE], 0x100 - PREAMBLE_SIZE);
	hexdump(data,n+PREAMBLE_SIZE);
#endif
	return -1;
      }
      rm_read (tcp, &data[PREAMBLE_SIZE], chunk_size-PREAMBLE_SIZE);
      break;
    default:
      *chunk_type = 0;
      chunk_size  = PREAMBLE_SIZE;
      break;
  }

  return chunk_size;
}

/*
 * writes a chunk to a buffer, returns number of bytes written
 */

int Pnm::write_chunk(uint16_t chunk_id, uint16_t length,
    const char *chunk, char *data) const {

  data[0]=(chunk_id>>8)%0xff;
  data[1]=chunk_id%0xff;
  data[2]=(length>>8)%0xff;
  data[3]=length%0xff;
  memcpy(&data[4],chunk,length);

  return length+4;
}

/*
 * constructs a request and sends it
 */

void Pnm::send_request(uint32_t bandwidth) {
  UNUSED(bandwidth);
  uint16_t i16;
  int c=PNM_HEADER_SIZE;
  char fixme[]={0,1};

  memcpy(buffer,pnm_header,PNM_HEADER_SIZE);
  c+=write_chunk(PNA_CLIENT_CHALLANGE,strlen(pnm_challenge),
	  pnm_challenge,&buffer[c]);
  c+=write_chunk(PNA_CLIENT_CAPS,PNM_CLIENT_CAPS_SIZE,
	  reinterpret_cast<const char*>(pnm_client_caps),&buffer[c]);
  c+=write_chunk(0x0a,0,NULL,&buffer[c]);
  c+=write_chunk(0x0c,0,NULL,&buffer[c]);
  c+=write_chunk(0x0d,0,NULL,&buffer[c]);
  c+=write_chunk(0x16,2,fixme,&buffer[c]);
  c+=write_chunk(PNA_TIMESTAMP,strlen(pnm_timestamp),
	  pnm_timestamp,&buffer[c]);
  c+=write_chunk(PNA_BANDWIDTH,4,
	  (const char *)&pnm_default_bandwidth,&buffer[c]);
  c+=write_chunk(0x08,0,NULL,&buffer[c]);
  c+=write_chunk(0x0e,0,NULL,&buffer[c]);
  c+=write_chunk(0x0f,0,NULL,&buffer[c]);
  c+=write_chunk(0x11,0,NULL,&buffer[c]);
  c+=write_chunk(0x10,0,NULL,&buffer[c]);
  c+=write_chunk(0x15,0,NULL,&buffer[c]);
  c+=write_chunk(0x12,0,NULL,&buffer[c]);
  c+=write_chunk(PNA_GUID,strlen(pnm_guid),
	  pnm_guid,&buffer[c]);
  c+=write_chunk(PNA_TWENTYFOUR,PNM_TWENTYFOUR_SIZE,
	  reinterpret_cast<char*>(pnm_twentyfour),&buffer[c]);

  /* data after chunks */
  memcpy(&buffer[c],after_chunks,after_chunks_length);
  c+=after_chunks_length;

  /* client id string */
  buffer[c]=PNA_CLIENT_STRING;
  i16=BE_16D((uint8_t*)((strlen(client_string)-1))); /* dont know why do we have -1 here */
  memcpy(&buffer[c+1],&i16,2);
  memcpy(&buffer[c+3],client_string,strlen(client_string)+1);
  c=c+3+strlen(client_string)+1;

  /* file path */
  buffer[c]=0;
  buffer[c+1]=PNA_PATH_REQUEST;
  i16=BE_16D((uint8_t*)path.length());
  memcpy(&buffer[c+2],&i16,2);
  memcpy(&buffer[c+4],path.c_str(),path.length());
  c=c+4+path.length();

  /* some trailing bytes */
  buffer[c]='y';
  buffer[c+1]='B';

  rm_write(tcp,buffer,c+2);
}

/*
 * pnm_send_response sends a response of a challenge
 */

void Pnm::send_response(const char *response) {

  int size=strlen(response);

  buffer[0]=0x23;
  buffer[1]=0;
  buffer[2]=(unsigned char) size;

  memcpy(&buffer[3], response, size);

  rm_write (tcp, buffer, size+3);

}

/*
 * get headers and challenge and fix headers
 * write headers to header
 * write challenge to buffer
 *
 * return 0 on error.  != 0 on success
 */
int Pnm::get_headers(int *need_response) {

  uint32_t chunk_type;
  char     *ptr=reinterpret_cast<char*>(header);
  uint8_t  *prop_hdr=NULL;
  int      chunk_size,size=0;
  int      nr;
/*  rmff_header_t *h; */

  *need_response=0;

  while(1) {
    if (Pnm::HEADER_SIZE-size<=0)
    {
      mpxp_info<<"input_pnm: header buffer overflow. exiting"<<std::endl;
      return 0;
    }
    chunk_size=get_chunk(Pnm::HEADER_SIZE-size,&chunk_type,ptr,&nr);
    if (chunk_size < 0) return 0;
    if (chunk_type == 0) break;
    if (chunk_type == PNA_TAG)
    {
      memcpy(ptr, rm_header, RM_HEADER_SIZE);
      chunk_size=RM_HEADER_SIZE;
      *need_response=nr;
    }
    if (chunk_type == DATA_TAG)
      chunk_size=0;
    if (chunk_type == RMF_TAG)
      chunk_size=0;
    if (chunk_type == PROP_TAG)
	prop_hdr=reinterpret_cast<uint8_t*>(ptr);
    size+=chunk_size;
    ptr+=chunk_size;
  }

  if (!prop_hdr) {
    mpxp_info<<"input_pnm: error while parsing headers"<<std::endl;
    return 0;
  }

  /* set data offset */
  size--;
  prop_hdr[42]=(size>>24)%0xff;
  prop_hdr[43]=(size>>16)%0xff;
  prop_hdr[44]=(size>>8)%0xff;
  prop_hdr[45]=(size)%0xff;
  size++;

  /* read challenge */
  memcpy (buffer, ptr, PREAMBLE_SIZE);
  rm_read (tcp, &buffer[PREAMBLE_SIZE], 64);

  /* now write a data header */
  memcpy(ptr, pnm_data_header, PNM_DATA_HEADER_SIZE);
  size+=PNM_DATA_HEADER_SIZE;
/*
  h=rmff_scan_header(header);
  rmff_fix_header(h);
  header_len=rmff_get_header_size(h);
  rmff_dump_header(h, header, HEADER_SIZE);
*/
  header_len=size;

  return 1;
}

/*
 * determine correct stream number by looking at indices
 */

int Pnm::calc_stream() {

  char str0=0,str1=0;

  /* looking at the first index to
   * find possible stream types
   */
  if (seq_current[0]==seq_num[0]) str0=1;
  if (seq_current[0]==seq_num[2]) str1=1;

  switch (str0+str1) {
    case 1: /* one is possible, good. */
      if (str0)
      {
	seq_num[0]++;
	seq_num[1]=seq_current[1]+1;
	return 0;
      } else
      {
	seq_num[2]++;
	seq_num[3]=seq_current[1]+1;
	return 1;
      }
      break;
    case 0:
    case 2: /* both types or none possible, not so good */
      /* try to figure out by second index */
      if (  (seq_current[1] == seq_num[1])
	  &&(seq_current[1] != seq_num[3]))
      {
	/* ok, only stream0 matches */
	seq_num[0]=seq_current[0]+1;
	seq_num[1]++;
	return 0;
      }
      if (  (seq_current[1] == seq_num[3])
	  &&(seq_current[1] != seq_num[1]))
      {
	/* ok, only stream1 matches */
	seq_num[2]=seq_current[0]+1;
	seq_num[3]++;
	return 1;
      }
      /* wow, both streams match, or not.   */
      /* now we try to decide by timestamps */
      if (ts_current < ts_last[1])
	return 0;
      if (ts_current < ts_last[0])
	return 1;
      /* does not help, we guess type 0     */
#ifdef LOG
      mpxp_info<<"guessing stream# 0"<<std::endl;
#endif
      seq_num[0]=seq_current[0]+1;
      seq_num[1]=seq_current[1]+1;
      return 0;
      break;
  }
  mpxp_info<<"input_pnm: wow, something very nasty happened in pnm_calc_stream"<<std::endl;
  return 2;
}

/*
 * gets a stream chunk and writes it to a recieve buffer
 */

int Pnm::get_stream_chunk() {

  int  n;
  char keepalive='!';
  unsigned int fof1, fof2, stream;

  /* send a keepalive                               */
  /* realplayer seems to do that every 43th package */
  if ((packet%43) == 42)
  {
    rm_write(tcp,&keepalive,1);
  }

  /* data chunks begin with: 'Z' <o> <o> <i1> 'Z' <i2>
   * where <o> is the offset to next stream chunk,
   * <i1> is a 16 bit index
   * <i2> is a 8 bit index which counts from 0x10 to somewhere
   */

  n = rm_read (tcp, buffer, 8);
  if (n<8) return 0;

  /* skip 8 bytes if 0x62 is read */
  if (buffer[0] == 0x62)
  {
    n = rm_read (tcp, buffer, 8);
    if (n<8) return 0;
#ifdef LOG
    mpxp_info<<"input_pnm: had to seek 8 bytes on 0x62"<<std::endl;
#endif
  }

  /* a server message */
  if (buffer[0] == 'X')
  {
    int size=BE_16((uint8_t*)(&buffer[1]));

    rm_read (tcp, &buffer[8], size-5);
    buffer[size+3]=0;
    mpxp_info<<"input_pnm: got message from server while reading stream:"<<&buffer[3]<<std::endl;
    return 0;
  }
  if (buffer[0] == 'F')
  {
    mpxp_info<<"input_pnm: server error"<<std::endl;
    return 0;
  }

  /* skip bytewise to next chunk.
   * seems, that we dont need that, if we send enough
   * keepalives
   */
  n=0;
  while (buffer[0] != 0x5a) {
    int i;
    for (i=1; i<8; i++) {
      buffer[i-1]=buffer[i];
    }
    rm_read (tcp, &buffer[7], 1);
    n++;
  }

#ifdef LOG
  if (n) mpxp_info<<"input_pnm: had to seek "<<n<<" bytes to next chunk"<<std::endl;
#endif

  /* check for 'Z's */
  if ((buffer[0] != 0x5a)||(buffer[7] != 0x5a))
  {
    mpxp_info<<"input_pnm: bad boundaries"<<std::endl;
    hexdump(buffer, 8);
    return 0;
  }

  /* check offsets */
  fof1=BE_16((uint8_t*)(&buffer[1]));
  fof2=BE_16((uint8_t*)(&buffer[3]));
  if (fof1 != fof2)
  {
    mpxp_info<<"input_pnm: frame offsets are different: 0x"<<std::hex<<fof1<<" 0x"<<std::hex<<fof2<<std::endl;
    return 0;
  }

  /* get first index */
  seq_current[0]=BE_16((uint8_t*)(&buffer[5]));

  /* now read the rest of stream chunk */
  n = rm_read (tcp, &recv[5], fof1-5);
  if (n<(fof1-5)) return 0;

  /* get second index */
  seq_current[1]=recv[5];

  /* get timestamp */
  ts_current=BE_32(&recv[6]);

  /* get stream number */
  stream=calc_stream();

  /* saving timestamp */
  ts_last[stream]=ts_current;

  /* constructing a data packet header */

  recv[0]=0;        /* object version */
  recv[1]=0;

  fof2=BE_16((uint8_t*)(&fof2));
  memcpy(&recv[2], &fof2, 2);
  /*recv[2]=(fof2>>8)%0xff;*/   /* length */
  /*recv[3]=(fof2)%0xff;*/

  recv[4]=0;         /* stream number */
  recv[5]=stream;

  recv[10]=recv[10] & 0xfe; /* streambox seems to do that... */

  packet++;

  recv_size=fof1;

  return fof1;
}

// Pnm *pnm_connect(const char *mrl) {
MPXP_Rc Pnm::connect(const std::string& _path) {

  int need_response=0;

  path=_path;

  send_request(pnm_available_bandwidths[10]);
  if (!get_headers(&need_response)) {
    mpxp_info<<"input_pnm: failed to set up stream"<<std::endl;
    return MPXP_False;
  }
  if (need_response) send_response(pnm_response);
  ts_last[0]=0;
  ts_last[1]=0;

  /* copy header to recv */

  memcpy(recv, header, header_len);
  recv_size = header_len;
  recv_read = 0;

  return MPXP_Ok;
}

int Pnm::read (char *data, int len) {

    int to_copy=len;
    char *dest=data;
    char *source=reinterpret_cast<char*>(recv) + recv_read;
    int fill=recv_size - recv_read;

    if (len < 0) return 0;
    while (to_copy > fill) {

	memcpy(dest, source, fill);
	to_copy -= fill;
	dest += fill;
	recv_read=0;

	if (!get_stream_chunk ()) {
#ifdef LOG
	    mpxp_info<<"input_pnm: "<<(len-to_copy)<<" of "<<len<<" bytes provided"<<std::endl;
#endif
	    return len-to_copy;
	}
	source = reinterpret_cast<char*>(recv);
	fill = recv_size - recv_read;
    }
    memcpy(dest, source, to_copy);
    recv_read += to_copy;
#ifdef LOG
    mpxp_info<<"input_pnm: "<<len<<" bytes provided"<<std::endl;
#endif
    return len;
}

int Pnm::peek_header (char *data) const {

  memcpy (data, header, header_len);
  return header_len;
}

void Pnm::close() {
  if (tcp.established()) tcp.close();
}
} // namespace mpxp
