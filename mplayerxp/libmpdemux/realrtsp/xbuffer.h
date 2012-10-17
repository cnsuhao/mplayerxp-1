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


#ifndef XCL_H
#define XCL_H
#include "mp_config.h"

any_t*xbuffer_init(int chunk_size);
any_t*xbuffer_free(any_t*buf);
any_t*xbuffer_copyin(any_t*buf, int index, const any_t*data, int len);
any_t*xbuffer_ensure_size(any_t*buf, int size);
any_t*xbuffer_strcat(any_t*buf, char *data);

#endif
