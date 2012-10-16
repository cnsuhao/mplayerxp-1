/**
 * @file libavutil/fifo.h
 * a very simple circular buffer FIFO implementation
 */
#include "mp_config.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "__mp_msg.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// keyboard:
extern int keyb_fifo_put;
extern int keyb_fifo_get;

extern void __FASTCALL__ fifo_make_pipe(int* pr,int* pw);

typedef struct CBFifoBuffer {
    uint8_t *buffer;
    uint8_t *rptr, *wptr, *end;
    uint32_t rndx, wndx;
} CBFifoBuffer;

/**
 * Initializes an CBFifoBuffer.
 * @param size of FIFO
 * @return CBFifoBuffer or NULL if mem allocation failure
 */
CBFifoBuffer *cb_fifo_alloc(unsigned int size);

/**
 * Frees an CBFifoBuffer.
 * @param *f CBFifoBuffer to free
 */
static inline void cb_fifo_free(CBFifoBuffer *f)
{
    if(f){
        free(f->buffer);
        free(f);
    }
}

/**
 * Resets the CBFifoBuffer to the state right after cb_fifo_alloc, in particular it is emptied.
 * @param *f CBFifoBuffer to reset
 */
static inline void cb_fifo_reset(CBFifoBuffer *f)
{
    f->wptr = f->rptr = f->buffer;
    f->wndx = f->rndx = 0;
}

/**
 * Returns the amount of data in bytes in the CBFifoBuffer, that is the
 * amount of data you can read from it.
 * @param *f CBFifoBuffer to read from
 * @return size
 */
static inline int cb_fifo_size(CBFifoBuffer *f)
{
    return (uint32_t)(f->wndx - f->rndx);
}

/**
 * Returns the amount of space in bytes in the CBFifoBuffer, that is the
 * amount of data you can write into it.
 * @param *f CBFifoBuffer to write into
 * @return size
 */
static inline int cb_fifo_space(CBFifoBuffer *f)
{
    return f->end - f->buffer - cb_fifo_size(f);
}

/**
 * Feeds data from an CBFifoBuffer to a user-supplied callback.
 * @param *f CBFifoBuffer to read from
 * @param buf_size number of bytes to read
 * @param *func generic read function
 * @param *dest data destination
 */
int cb_fifo_generic_read(CBFifoBuffer *f, void *dest, int buf_size, void (*func)(void*, void*, int));

/**
 * Feeds data from a user-supplied callback to an CBFifoBuffer.
 * @param *f CBFifoBuffer to write to
 * @param *src data source
 * @param size number of bytes to write
 * @param *func generic write function; the first parameter is src,
 * the second is dest_buf, the third is dest_buf_size.
 * func must return the number of bytes written to dest_buf, or <= 0 to
 * indicate no more data available to write.
 * If func is NULL, src is interpreted as a simple byte array for source data.
 * @return the number of bytes written to the FIFO
 */
int cb_fifo_generic_write(CBFifoBuffer *f, void *src, int size, int (*func)(void*, void*, int));

/**
 * Resizes an CBFifoBuffer.
 * @param *f CBFifoBuffer to resize
 * @param size new CBFifoBuffer size in bytes
 * @return <0 for failure, >=0 otherwise
 */
int cb_fifo_realloc2(CBFifoBuffer *f, unsigned int size);

/**
 * Reads and discards the specified amount of data from an CBFifoBuffer.
 * @param *f CBFifoBuffer to read from
 * @param size amount of data to read in bytes
 */
static inline void cb_fifo_drain(CBFifoBuffer *f, int size)
{
    f->rptr += size;
    if (f->rptr >= f->end)
        f->rptr -= f->end - f->buffer;
    f->rndx += size;
}

static inline uint8_t cb_fifo_peek(CBFifoBuffer *f, int offs)
{
    uint8_t *ptr = f->rptr + offs;
    if (ptr >= f->end)
        ptr -= f->end - f->buffer;
    return *ptr;
}
