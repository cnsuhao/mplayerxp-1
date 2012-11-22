/*
 * xbuffer code
 *
 * Includes a minimalistic replacement for xine_buffer functions used in
 * Real streaming code. Only function needed by this code are implemented.
 *
 * Most code comes from xine_buffer.c Copyright (C) 2002 the xine project
 *
 * WARNING: do not mix original xine_buffer functions with this code!
 * xbuffers behave like xine_buffers, but are not byte-compatible with them.
 * You must take care of pointers returned by xbuffers functions (no macro to
 * do it automatically)
 *
 */

#include "mplayerxp.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "xbuffer.h"
#include "osdep/mplib.h"


typedef struct {
  uint32_t size;
  uint32_t chunk_size;
} xbuffer_header_t;

#define XBUFFER_HEADER_SIZE sizeof (xbuffer_header_t)



any_t*xbuffer_init(int chunk_size) {
  uint8_t *data=new(zeromem) uint8_t[chunk_size+XBUFFER_HEADER_SIZE];

  xbuffer_header_t *header=(xbuffer_header_t*)data;

  header->size=chunk_size;
  header->chunk_size=chunk_size;

  return data+XBUFFER_HEADER_SIZE;
}



any_t*xbuffer_free(any_t*buf) {
  if (!buf) {
    return NULL;
  }

  mp_free (((uint8_t*)buf)-XBUFFER_HEADER_SIZE);

  return NULL;
}



any_t*xbuffer_copyin(any_t*buf, int _index, const any_t*data, int len) {
    if (!buf || !data) {
    return NULL;
  }

  buf = xbuffer_ensure_size(buf, _index+len);
  memcpy(((uint8_t*)buf)+_index, data, len);

  return buf;
}



any_t*xbuffer_ensure_size(any_t*buf, int size) {
  xbuffer_header_t *xbuf;
  int new_size;

  if (!buf) {
    return 0;
  }

  xbuf = ((xbuffer_header_t*)(((uint8_t*)buf)-XBUFFER_HEADER_SIZE));

  if (xbuf->size < (unsigned)size) {
    new_size = size + xbuf->chunk_size - (size % xbuf->chunk_size);
    xbuf->size = new_size;
    buf = ((uint8_t*)mp_realloc(((uint8_t*)buf)-XBUFFER_HEADER_SIZE,
	  new_size+XBUFFER_HEADER_SIZE)) + XBUFFER_HEADER_SIZE;
  }

  return buf;
}



any_t*xbuffer_strcat(any_t*buf, char *data) {

  if (!buf || !data) {
    return NULL;
  }

  buf = xbuffer_ensure_size(buf, strlen((char *)buf)+strlen(data)+1);

  strcat((char *)buf, data);

  return buf;
}
