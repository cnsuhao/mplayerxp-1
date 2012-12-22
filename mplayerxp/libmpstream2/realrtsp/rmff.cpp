#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * This file was ported to MPlayer from xine CVS rmff.c,v 1.3 2002/12/24 01:30:22
 */

/*
 * Copyright (C) 2002 the xine project
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
 *
 * functions for real media file format
 * adopted from joschkas real tools
 */

#include "rmff.h"
#include "xbuffer.h"
#include "osdep/bswap.h"
#include "mplayerxp.h"
#include "md5.h"
#include "stream_msg.h"

/*
#define LOG
*/

static void hexdump (const char *buf, int length) {

  int i;

  mpxp_info<<"rmff: ascii>";
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    if ((c >= 32) && (c <= 128))
      mpxp_info<<c;
    else
      mpxp_info<<'<'<<std::hex<<(unsigned)c<<'>';
  }
  mpxp_info<<std::endl;
}

/*
 * writes header data to a buffer
 */

static int rmff_dump_fileheader(rmff_fileheader_t *fileheader, char *buffer, int bufsize) {

  if (!fileheader) return 0;

  if (bufsize < RMFF_FILEHEADER_SIZE)
    return -1;

  AV_WB32(buffer, fileheader->object_id);
  AV_WB32(buffer+4, fileheader->size);
  AV_WB16(buffer+8, fileheader->object_version);
  AV_WB32(buffer+10, fileheader->file_version);
  AV_WB32(buffer+14, fileheader->num_headers);

  return RMFF_FILEHEADER_SIZE;
}

static int rmff_dump_prop(rmff_prop_t *prop, char *buffer, int bufsize) {

  if (!prop) return 0;

  if (bufsize < RMFF_PROPHEADER_SIZE)
    return -1;

  AV_WB32(buffer, prop->object_id);
  AV_WB32(buffer+4, prop->size);
  AV_WB16(buffer+8, prop->object_version);
  AV_WB32(buffer+10, prop->max_bit_rate);
  AV_WB32(buffer+14, prop->avg_bit_rate);
  AV_WB32(buffer+18, prop->max_packet_size);
  AV_WB32(buffer+22, prop->avg_packet_size);
  AV_WB32(buffer+26, prop->num_packets);
  AV_WB32(buffer+30, prop->duration);
  AV_WB32(buffer+34, prop->preroll);
  AV_WB32(buffer+38, prop->index_offset);
  AV_WB32(buffer+42, prop->data_offset);
  AV_WB16(buffer+46, prop->num_streams);
  AV_WB16(buffer+48, prop->flags);

  return RMFF_PROPHEADER_SIZE;
}

static int rmff_dump_mdpr(rmff_mdpr_t *mdpr, char *buffer, int bufsize) {

  int s1, s2, s3;

  if (!mdpr) return 0;

  if (!(bufsize > RMFF_MDPRHEADER_SIZE + mdpr->stream_name_size + mdpr->mime_type_size &&
	(unsigned)bufsize - RMFF_MDPRHEADER_SIZE - mdpr->stream_name_size - mdpr->mime_type_size > mdpr->type_specific_len))
    return -1;

  AV_WB32(buffer, mdpr->object_id);
  AV_WB32(buffer+4, mdpr->size);
  AV_WB16(buffer+8, mdpr->object_version);
  AV_WB16(buffer+10, mdpr->stream_number);
  AV_WB32(buffer+12, mdpr->max_bit_rate);
  AV_WB32(buffer+16, mdpr->avg_bit_rate);
  AV_WB32(buffer+20, mdpr->max_packet_size);
  AV_WB32(buffer+24, mdpr->avg_packet_size);
  AV_WB32(buffer+28, mdpr->start_time);
  AV_WB32(buffer+32, mdpr->preroll);
  AV_WB32(buffer+36, mdpr->duration);

  buffer[40] = mdpr->stream_name_size;
  s1=mdpr->stream_name_size;
  memcpy(&buffer[41], mdpr->stream_name, s1);

  buffer[41+s1] = mdpr->mime_type_size;
  s2=mdpr->mime_type_size;
  memcpy(&buffer[42+s1], mdpr->mime_type, s2);

  AV_WB32(buffer+42+s1+s2, mdpr->type_specific_len);
  s3=mdpr->type_specific_len;
  memcpy(&buffer[46+s1+s2], mdpr->type_specific_data, s3);

  return RMFF_MDPRHEADER_SIZE + s1 + s2 + s3;
}

static int rmff_dump_cont(rmff_cont_t *cont, char *buffer, int bufsize) {

  int p;

  if (!cont) return 0;

  if (bufsize < RMFF_CONTHEADER_SIZE + cont->title_len + cont->author_len +
      cont->copyright_len + cont->comment_len)
    return -1;

  AV_WB32(buffer, cont->object_id);
  AV_WB32(buffer+4, cont->size);
  AV_WB16(buffer+8, cont->object_version);

  AV_WB16(buffer+10, cont->title_len);
  memcpy(&buffer[12], cont->title, cont->title_len);
  p=12+cont->title_len;

  AV_WB16(buffer+p, cont->author_len);
  memcpy(&buffer[p+2], cont->author, cont->author_len);
  p+=2+cont->author_len;

  AV_WB16(buffer+p, cont->copyright_len);
  memcpy(&buffer[p+2], cont->copyright, cont->copyright_len);
  p+=2+cont->copyright_len;

  AV_WB16(buffer+p, cont->comment_len);
  memcpy(&buffer[p+2], cont->comment, cont->comment_len);

  return RMFF_CONTHEADER_SIZE + cont->title_len + cont->author_len +
	 cont->copyright_len + cont->comment_len;
}

static int rmff_dump_dataheader(rmff_data_t *data, char *buffer, int bufsize) {

  if (!data) return 0;

  if (bufsize < RMFF_DATAHEADER_SIZE)
    return -1;

  AV_WB32(buffer, data->object_id);
  AV_WB32(buffer+4, data->size);
  AV_WB16(buffer+8, data->object_version);
  AV_WB32(buffer+10, data->num_packets);
  AV_WB32(buffer+14, data->next_data_header);

  return RMFF_DATAHEADER_SIZE;
}

int rmff_dump_header(rmff_header_t *h, char *buffer, int max) {

  int written=0, size;
  rmff_mdpr_t **stream=h->streams;

  if ((size=rmff_dump_fileheader(h->fileheader, &buffer[written], max)) < 0)
    goto buftoosmall;
  written+=size;
  max -= size;
  if ((size=rmff_dump_prop(h->prop, &buffer[written], max)) < 0)
    goto buftoosmall;
  written+=size;
  max -= size;
  if ((size=rmff_dump_cont(h->cont, &buffer[written], max)) < 0)
    goto buftoosmall;
  written+=size;
  max -= size;
  if (stream)
  {
    while(*stream)
    {
      if ((size=rmff_dump_mdpr(*stream, &buffer[written], max)) < 0)
	goto buftoosmall;
      written+=size;
      max -= size;
      stream++;
    }
  }

  if ((size=rmff_dump_dataheader(h->data, &buffer[written], max)) < 0)
    goto buftoosmall;
  written+=size;

  return written;

buftoosmall:
  mpxp_err<<"rmff_dumpheader: buffer too small, aborting. Please report"<<std::endl;
  return -1;
}

void rmff_dump_pheader(rmff_pheader_t *h, char *data) {

  data[0]=(h->object_version>>8) & 0xff;
  data[1]=h->object_version & 0xff;
  data[2]=(h->length>>8) & 0xff;
  data[3]=h->length & 0xff;
  data[4]=(h->stream_number>>8) & 0xff;
  data[5]=h->stream_number & 0xff;
  data[6]=(h->timestamp>>24) & 0xff;
  data[7]=(h->timestamp>>16) & 0xff;
  data[8]=(h->timestamp>>8) & 0xff;
  data[9]=h->timestamp & 0xff;
  data[10]=h->reserved;
  data[11]=h->flags;
}

static rmff_fileheader_t *rmff_scan_fileheader(const char *data) {

  rmff_fileheader_t *fileheader=new rmff_fileheader_t;

  fileheader->object_id=AV_RB32(data);
  fileheader->size=AV_RB32(&data[4]);
  fileheader->object_version=AV_RB16(&data[8]);
  if (fileheader->object_version != 0) mpxp_warn<<"warning: unknown object version in .RMF: 0x"<<std::hex<<fileheader->object_version<<std::endl;
  fileheader->file_version=AV_RB32(&data[10]);
  fileheader->num_headers=AV_RB32(&data[14]);

  return fileheader;
}

static rmff_prop_t *rmff_scan_prop(const char *data) {

  rmff_prop_t *prop=new rmff_prop_t;

  prop->object_id=AV_RB32(data);
  prop->size=AV_RB32(&data[4]);
  prop->object_version=AV_RB16(&data[8]);
  if (prop->object_version != 0) mpxp_warn<<"warning: unknown object version in PROP: 0x"<<std::hex<<prop->object_version<<std::endl;
  prop->max_bit_rate=AV_RB32(&data[10]);
  prop->avg_bit_rate=AV_RB32(&data[14]);
  prop->max_packet_size=AV_RB32(&data[18]);
  prop->avg_packet_size=AV_RB32(&data[22]);
  prop->num_packets=AV_RB32(&data[26]);
  prop->duration=AV_RB32(&data[30]);
  prop->preroll=AV_RB32(&data[34]);
  prop->index_offset=AV_RB32(&data[38]);
  prop->data_offset=AV_RB32(&data[42]);
  prop->num_streams=AV_RB16(&data[46]);
  prop->flags=AV_RB16(&data[48]);

  return prop;
}

static rmff_mdpr_t *rmff_scan_mdpr(const char *data) {

  rmff_mdpr_t *mdpr=new rmff_mdpr_t;

  mdpr->object_id=AV_RB32(data);
  mdpr->size=AV_RB32(&data[4]);
  mdpr->object_version=AV_RB16(&data[8]);
  if (mdpr->object_version != 0) mpxp_warn<<"warning: unknown object version in MDPR: 0x"<<std::hex<<mdpr->object_version<<std::endl;
  mdpr->stream_number=AV_RB16(&data[10]);
  mdpr->max_bit_rate=AV_RB32(&data[12]);
  mdpr->avg_bit_rate=AV_RB32(&data[16]);
  mdpr->max_packet_size=AV_RB32(&data[20]);
  mdpr->avg_packet_size=AV_RB32(&data[24]);
  mdpr->start_time=AV_RB32(&data[28]);
  mdpr->preroll=AV_RB32(&data[32]);
  mdpr->duration=AV_RB32(&data[36]);

  mdpr->stream_name_size=data[40];
  mdpr->stream_name=new char [mdpr->stream_name_size+1];
  memcpy(mdpr->stream_name, &data[41], mdpr->stream_name_size);
  mdpr->stream_name[mdpr->stream_name_size]=0;

  mdpr->mime_type_size=data[41+mdpr->stream_name_size];
  mdpr->mime_type=new char [mdpr->mime_type_size+1];
  memcpy(mdpr->mime_type, &data[42+mdpr->stream_name_size], mdpr->mime_type_size);
  mdpr->mime_type[mdpr->mime_type_size]=0;

  mdpr->type_specific_len=AV_RB32(&data[42+mdpr->stream_name_size+mdpr->mime_type_size]);
  mdpr->type_specific_data=new char [mdpr->type_specific_len];
  memcpy(mdpr->type_specific_data,
      &data[46+mdpr->stream_name_size+mdpr->mime_type_size], mdpr->type_specific_len);

  return mdpr;
}

static rmff_cont_t *rmff_scan_cont(const char *data) {

  rmff_cont_t *cont=new rmff_cont_t;
  int pos;

  cont->object_id=AV_RB32(data);
  cont->size=AV_RB32(&data[4]);
  cont->object_version=AV_RB16(&data[8]);
  if (cont->object_version != 0) mpxp_warn<<"warning: unknown object version in CONT: 0x"<<std::hex<<cont->object_version<<std::endl;
  cont->title_len=AV_RB16(&data[10]);
  cont->title=new char [cont->title_len+1];
  memcpy(cont->title, &data[12], cont->title_len);
  cont->title[cont->title_len]=0;
  pos=cont->title_len+12;
  cont->author_len=AV_RB16(&data[pos]);
  cont->author=new char [cont->author_len+1];
  memcpy(cont->author, &data[pos+2], cont->author_len);
  cont->author[cont->author_len]=0;
  pos=pos+2+cont->author_len;
  cont->copyright_len=AV_RB16(&data[pos]);
  cont->copyright=new char [cont->copyright_len+1];
  memcpy(cont->copyright, &data[pos+2], cont->copyright_len);
  cont->copyright[cont->copyright_len]=0;
  pos=pos+2+cont->copyright_len;
  cont->comment_len=AV_RB16(&data[pos]);
  cont->comment=new char [cont->comment_len+1];
  memcpy(cont->comment, &data[pos+2], cont->comment_len);
  cont->comment[cont->comment_len]=0;

  return cont;
}

static rmff_data_t *rmff_scan_dataheader(const char *data) {

  rmff_data_t *dh=new rmff_data_t;

  dh->object_id=AV_RB32(data);
  dh->size=AV_RB32(&data[4]);
  dh->object_version=AV_RB16(&data[8]);
  if (dh->object_version != 0) mpxp_warn<<"warning: unknown object version in DATA: 0x"<<std::hex<<dh->object_version<<std::endl;
  dh->num_packets=AV_RB32(&data[10]);
  dh->next_data_header=AV_RB32(&data[14]);

  return dh;
}

rmff_header_t *rmff_scan_header(const char *data) {

	rmff_header_t *header=new rmff_header_t;
	rmff_mdpr_t   *mdpr=NULL;
	int           chunk_size;
	uint32_t      chunk_type;
  const char    *ptr=data;
  int           i;

  header->fileheader=NULL;
	header->prop=NULL;
	header->cont=NULL;
	header->data=NULL;

  chunk_type = AV_RB32(ptr);
  if (chunk_type != RMF_TAG){
    mpxp_err<<"rmff: not an real media file header (.RMF tag not found)"<<std::endl;
    delete header;
    return NULL;
  }
  header->fileheader=rmff_scan_fileheader(ptr);
  ptr += header->fileheader->size;

	header->streams=new rmff_mdpr_t*[header->fileheader->num_headers];
  for (i=0; i<header->fileheader->num_headers; i++) {
    header->streams[i]=NULL;
  }

  for (i=1; i<header->fileheader->num_headers; i++) {
    chunk_type = AV_RB32(ptr);

    if (ptr[0] == 0) {
      mpxp_warn<<"rmff: warning: only "<<i<<" of "<<header->fileheader->num_headers<<" header found"<<std::endl;
      break;
    }

    chunk_size=1;
    switch (chunk_type) {
    case PROP_TAG:
      header->prop=rmff_scan_prop(ptr);
      chunk_size=header->prop->size;
      break;
    case MDPR_TAG:
      mdpr=rmff_scan_mdpr(ptr);
      chunk_size=mdpr->size;
      header->streams[mdpr->stream_number]=mdpr;
      break;
    case CONT_TAG:
      header->cont=rmff_scan_cont(ptr);
      chunk_size=header->cont->size;
      break;
    case DATA_TAG:
      header->data=rmff_scan_dataheader(ptr);
      chunk_size=34;     /* hard coded header size */
      break;
    default:
      mpxp_warn<<"unknown chunk"<<std::endl;
      hexdump(ptr,10);
      chunk_size=1;
      break;
    }
    ptr+=chunk_size;
  }

	return header;
}

rmff_header_t *rmff_scan_header_stream(int fd) {

  rmff_header_t *header;
  char *buf=(char*)xbuffer_init(1024);
  int index=0;
  uint32_t chunk_type;
  uint32_t chunk_size;

  do {
    buf = (char*)xbuffer_ensure_size(buf, index+8);
    recv(fd, buf+index, 8, 0);
    chunk_type=AV_RB32(buf+index); index+=4;
    chunk_size=AV_RB32(buf+index); index+=4;

    switch (chunk_type) {
      case DATA_TAG:
	chunk_size=18;
      case MDPR_TAG:
      case CONT_TAG:
      case RMF_TAG:
      case PROP_TAG:
	buf = (char*)xbuffer_ensure_size(buf, index+chunk_size-8);
	recv(fd, buf+index, (chunk_size-8), 0);
	index+=(chunk_size-8);
	break;
      default:
	mpxp_warn<<"rmff_scan_header_stream: unknown chunk"<<std::endl;
	hexdump(buf+index-8, 8);
	chunk_type=DATA_TAG;
    }
  } while (chunk_type != DATA_TAG);

  header = rmff_scan_header(buf);

  xbuffer_free(buf);

  return header;
}

void rmff_scan_pheader(rmff_pheader_t *h, char *data) {

  h->object_version=AV_RB16(data);
  h->length=AV_RB16(data+2);
  h->stream_number=AV_RB16(data+4);
  h->timestamp=AV_RB32(data+6);
  h->reserved=(uint8_t)data[10];
  h->flags=(uint8_t)data[11];
}

rmff_fileheader_t *rmff_new_fileheader(uint32_t num_headers) {

  rmff_fileheader_t *fileheader=new rmff_fileheader_t;

  fileheader->object_id=RMF_TAG;
  fileheader->size=18;
  fileheader->object_version=0;
  fileheader->file_version=0;
  fileheader->num_headers=num_headers;

  return fileheader;
}

rmff_prop_t *rmff_new_prop (
    uint32_t max_bit_rate,
    uint32_t avg_bit_rate,
    uint32_t max_packet_size,
    uint32_t avg_packet_size,
    uint32_t num_packets,
    uint32_t duration,
    uint32_t preroll,
    uint32_t index_offset,
    uint32_t data_offset,
    uint16_t num_streams,
    uint16_t flags ) {

  rmff_prop_t *prop=new rmff_prop_t;

  prop->object_id=PROP_TAG;
  prop->size=50;
  prop->object_version=0;

  prop->max_bit_rate=max_bit_rate;
  prop->avg_bit_rate=avg_bit_rate;
  prop->max_packet_size=max_packet_size;
  prop->avg_packet_size=avg_packet_size;
  prop->num_packets=num_packets;
  prop->duration=duration;
  prop->preroll=preroll;
  prop->index_offset=index_offset;
  prop->data_offset=data_offset;
  prop->num_streams=num_streams;
  prop->flags=flags;

  return prop;
}

rmff_mdpr_t *rmff_new_mdpr(
      uint16_t   stream_number,
      uint32_t   max_bit_rate,
      uint32_t   avg_bit_rate,
      uint32_t   max_packet_size,
      uint32_t   avg_packet_size,
      uint32_t   start_time,
      uint32_t   preroll,
      uint32_t   duration,
      const char *stream_name,
      const char *mime_type,
      uint32_t   type_specific_len,
      const char *type_specific_data ) {

  rmff_mdpr_t *mdpr=new rmff_mdpr_t;

  mdpr->object_id=MDPR_TAG;
  mdpr->object_version=0;

  mdpr->stream_number=stream_number;
  mdpr->max_bit_rate=max_bit_rate;
  mdpr->avg_bit_rate=avg_bit_rate;
  mdpr->max_packet_size=max_packet_size;
  mdpr->avg_packet_size=avg_packet_size;
  mdpr->start_time=start_time;
  mdpr->preroll=preroll;
  mdpr->duration=duration;
  mdpr->stream_name_size=0;
  if (stream_name) {
    mdpr->stream_name=mp_strdup(stream_name);
    mdpr->stream_name_size=strlen(stream_name);
  }
  mdpr->mime_type_size=0;
  if (mime_type) {
    mdpr->mime_type=mp_strdup(mime_type);
    mdpr->mime_type_size=strlen(mime_type);
  }
  mdpr->type_specific_len=type_specific_len;
  mdpr->type_specific_data=new char [type_specific_len];
  memcpy(mdpr->type_specific_data,type_specific_data,type_specific_len);
  mdpr->mlti_data=NULL;

  mdpr->size=mdpr->stream_name_size+mdpr->mime_type_size+mdpr->type_specific_len+46;

  return mdpr;
}

rmff_cont_t *rmff_new_cont(const char *title, const char *author, const char *copyright, const char *comment) {

  rmff_cont_t *cont=new rmff_cont_t;

  cont->object_id=CONT_TAG;
  cont->object_version=0;

  cont->title=NULL;
  cont->author=NULL;
  cont->copyright=NULL;
  cont->comment=NULL;

  cont->title_len=0;
  cont->author_len=0;
  cont->copyright_len=0;
  cont->comment_len=0;

  if (title) {
    cont->title_len=strlen(title);
    cont->title=mp_strdup(title);
  }
  if (author) {
    cont->author_len=strlen(author);
    cont->author=mp_strdup(author);
  }
  if (copyright) {
    cont->copyright_len=strlen(copyright);
    cont->copyright=mp_strdup(copyright);
  }
  if (comment) {
    cont->comment_len=strlen(comment);
    cont->comment=mp_strdup(comment);
  }
  cont->size=cont->title_len+cont->author_len+cont->copyright_len+cont->comment_len+18;

  return cont;
}

rmff_data_t *rmff_new_dataheader(uint32_t num_packets, uint32_t next_data_header) {

  rmff_data_t *data=new rmff_data_t;

  data->object_id=DATA_TAG;
  data->size=18;
  data->object_version=0;
  data->num_packets=num_packets;
  data->next_data_header=next_data_header;

  return data;
}

void rmff_print_header(rmff_header_t *h) {

  rmff_mdpr_t **stream;

  if(!h) {
    mpxp_warn<<"rmff_print_header: NULL given"<<std::endl;
    return;
  }
  if(h->fileheader)
  {
    mpxp_info<<"FILE:"<<std::endl;
    mpxp_info<<"file version      : "<<h->fileheader->file_version<<std::endl;
    mpxp_info<<"number of headers : "<<h->fileheader->num_headers<<std::endl;
  }
  if(h->cont)
  {
    mpxp_info<<"CONTENT:"<<std::endl;
    mpxp_info<<"title     : "<<h->cont->title<<std::endl;
    mpxp_info<<"author    : "<<h->cont->author<<std::endl;
    mpxp_info<<"copyright : "<<h->cont->copyright<<std::endl;
    mpxp_info<<"comment   : "<<h->cont->comment<<std::endl;
  }
  if(h->prop)
  {
    mpxp_info<<"STREAM PROPERTIES:"<<std::endl;
    mpxp_info<<"bit rate (max/avg)    : "<<h->prop->max_bit_rate<<"/"<<h->prop->avg_bit_rate<<std::endl;
    mpxp_info<<"packet size (max/avg) : "<<h->prop->max_packet_size<<"/"<<h->prop->avg_packet_size<<" bytes"<<std::endl;
    mpxp_info<<"packets       : "<<h->prop->num_packets<<std::endl;
    mpxp_info<<"duration      : "<<h->prop->duration<<" ms"<<std::endl;
    mpxp_info<<"pre-buffer    : "<<h->prop->preroll<<" ms"<<std::endl;
    mpxp_info<<"index offset  : "<<h->prop->index_offset<<" bytes"<<std::endl;
    mpxp_info<<"data offset   : "<<h->prop->data_offset<<" bytes"<<std::endl;
    mpxp_info<<"media streams : "<<h->prop->num_streams<<std::endl;
    mpxp_info<<"flags         : ";
    if (h->prop->flags & PN_SAVE_ENABLED) mpxp_info<<"save_enabled ";
    if (h->prop->flags & PN_PERFECT_PLAY_ENABLED) mpxp_info<<"perfect_play_enabled ";
    if (h->prop->flags & PN_LIVE_BROADCAST) mpxp_info<<"live_broadcast ";
    mpxp_info<<std::endl;
  }
  stream=h->streams;
  if(stream)
  {
    while (*stream)
    {
      mpxp_info<<"STREAM: "<<(*stream)->stream_number<<std::endl;
      mpxp_info<<"stream name [mime type] : "<<(*stream)->stream_name<<"["<<((*stream)->mime_type)<<"]"<<std::endl;
      mpxp_info<<"bit rate (max/avg)      : "<<(*stream)->max_bit_rate<<"/"<<((*stream)->avg_bit_rate)<<std::endl;
      mpxp_info<<"packet size (max/avg)   : "<<(*stream)->max_packet_size<<"/"<<((*stream)->avg_packet_size)<<" bytes"<<std::endl;
      mpxp_info<<"start time : "<<(*stream)->start_time<<std::endl;
      mpxp_info<<"pre-buffer : "<<(*stream)->preroll<<" ms"<<std::endl;
      mpxp_info<<"duration   : "<<(*stream)->duration<<" ms"<<std::endl;
      mpxp_info<<"type specific data:"<<std::endl;
      hexdump((*stream)->type_specific_data, (*stream)->type_specific_len);
      stream++;
    }
  }
  if(h->data)
  {
    mpxp_info<<"DATA:"<<std::endl;
    mpxp_info<<"size      : "<<h->data->size<<std::endl;
    mpxp_info<<"packets   : "<<h->data->num_packets<<std::endl;
    mpxp_info<<"next DATA : 0x"<<std::hex<<h->data->next_data_header<<std::endl;
  }
}

void rmff_fix_header(rmff_header_t *h) {

  int num_headers=0;
  int header_size=0;
  rmff_mdpr_t **streams;
  int num_streams=0;

  if (!h) {
    mpxp_err<<"rmff_fix_header: fatal: no header given"<<std::endl;
    return;
  }

  if (!h->streams) mpxp_warn<<"rmff_fix_header: warning: no MDPR chunks"<<std::endl;
  else {
    streams=h->streams;
    while (*streams)
    {
      num_streams++;
      num_headers++;
      header_size+=(*streams)->size;
      streams++;
    }
  }

  if (h->prop) {
    if (h->prop->size != 50)
    {
#ifdef LOG
      mpxp_info<<"rmff_fix_header: correcting prop.size from "<<h->prop->size<<" to 50"<<std::endl;
#endif
      h->prop->size=50;
    }
    if (h->prop->num_streams != num_streams)
    {
#ifdef LOG
      mpxp_info<<"rmff_fix_header: correcting prop.num_streams from "<<h->prop->num_streams<<" to"<<num_streams<<std::endl;
#endif
      h->prop->num_streams=num_streams;
    }
    num_headers++;
    header_size+=50;
  } else
    mpxp_warn<<"rmff_fix_header: warning: no PROP chunk"<<std::endl;

  if (h->cont) {
    num_headers++;
    header_size+=h->cont->size;
  } else
    mpxp_warn<<"rmff_fix_header: warning: no CONT chunk"<<std::endl;

  if (!h->data) {
#ifdef LOG
    mpxp_info<<"rmff_fix_header: no DATA chunk, creating one"<<std::endl;
#endif
    h->data=new rmff_data_t;
    h->data->object_id=DATA_TAG;
    h->data->object_version=0;
    h->data->size=34;
    h->data->num_packets=0;
    h->data->next_data_header=0;
  }
  num_headers++;


  if (!h->fileheader) {
#ifdef LOG
    mpxp_info<<"rmff_fix_header: no fileheader, creating one");
#endif
    h->fileheader=new rmff_fileheader_t;
    h->fileheader->object_id=RMF_TAG;
    h->fileheader->size=34;
    h->fileheader->object_version=0;
    h->fileheader->file_version=0;
    h->fileheader->num_headers=num_headers+1;
  }
  header_size+=h->fileheader->size;
  num_headers++;

  if(h->fileheader->num_headers != num_headers) {
#ifdef LOG
    mpxp_info<<"rmff_fix_header: setting num_headers from "<<h->fileheader->num_headers<<" to"<<num_headers<<std::endl;
#endif
    h->fileheader->num_headers=num_headers;
  }

  if(h->prop) {
    if (h->prop->data_offset != header_size) {
#ifdef LOG
      mpxp_info<<"rmff_fix_header: setting prop.data_offset from "<<h->prop->data_offset<<" to"<<header_size<<std::endl;
#endif
      h->prop->data_offset=header_size;
    }
    if (h->prop->num_packets == 0) {
      int p=(int)(h->prop->avg_bit_rate/8.0*(h->prop->duration/1000.0)/h->prop->avg_packet_size);
#ifdef LOG
      mpxp_info<<"rmff_fix_header: assuming prop.num_packets="<<p<<std::endl;
#endif
      h->prop->num_packets=p;
    }
    if (h->data->num_packets == 0) {
#ifdef LOG
      mpxp_info<<"rmff_fix_header: assuming data.num_packets="<<h->prop->num_packets<<std::endl;
#endif
      h->data->num_packets=h->prop->num_packets;
    }

#ifdef LOG
    mpxp_info<<"rmff_fix_header: assuming data.size="<<h->prop->num_packets*h->prop->avg_packet_size<<std::endl;
#endif
    h->data->size=h->prop->num_packets*h->prop->avg_packet_size;
  }
}

int rmff_get_header_size(rmff_header_t *h) {

  if (!h) return 0;
  if (!h->prop) return -1;

  return h->prop->data_offset+18;

}

void rmff_free_header(rmff_header_t *h) {

  if (!h) return;

  if (h->fileheader) delete h->fileheader;
  if (h->prop) delete h->prop;
  if (h->data) delete h->data;
  if (h->cont)
  {
    delete h->cont->title;
    delete h->cont->author;
    delete h->cont->copyright;
    delete h->cont->comment;
    delete h->cont;
  }
  if (h->streams)
  {
    rmff_mdpr_t **s=h->streams;

    while(*s) {
      delete (*s)->stream_name;
      delete (*s)->mime_type;
      delete (*s)->type_specific_data;
      delete *s;
      s++;
    }
    delete h->streams;
  }
  delete h;
}
