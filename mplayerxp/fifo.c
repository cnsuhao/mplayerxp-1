#include "mplayerxp.h"
#include "fifo.h"
#include "osdep/mplib.h"

static inline int min(int a,int b) { return (a<b)?a:b; };

/*  === Circular Buffer FIFO support === */

CBFifoBuffer *cb_fifo_alloc(unsigned int size)
{
    CBFifoBuffer *f= mp_mallocz(sizeof(CBFifoBuffer));
    if(!f)
        return NULL;
    f->buffer = mp_malloc(size);
    f->end = f->buffer + size;
    cb_fifo_reset(f);
    if (!f->buffer)
        mp_free(f);
    return f;
}

int cb_fifo_realloc2(CBFifoBuffer *f, unsigned int new_size) {
    unsigned int old_size= f->end - f->buffer;

    if(old_size < new_size){
        int len= cb_fifo_size(f);
        CBFifoBuffer *f2= cb_fifo_alloc(new_size);

        if (!f2)
            return -1;
        cb_fifo_generic_read(f, f2->buffer, len, NULL);
        f2->wptr += len;
        f2->wndx += len;
        mp_free(f->buffer);
        *f= *f2;
        mp_free(f2);
    }
    return 0;
}

void cb_fifo_free(CBFifoBuffer *f)
{
    if(f){
        mp_free(f->buffer);
        mp_free(f);
    }
}

int cb_fifo_generic_write(CBFifoBuffer *f, any_t*src, int size, int (*func)(any_t*, any_t*, int))
{
    int total = size;
    do {
        int len = min(f->end - f->wptr, size);
        if(func) {
            if(func(src, f->wptr, len) <= 0)
                break;
        } else {
            memcpy(f->wptr, src, len);
            src = (uint8_t*)src + len;
        }
// Write memory barrier needed for SMP here in theory
        f->wptr += len;
        if (f->wptr >= f->end)
            f->wptr = f->buffer;
        f->wndx += len;
        size -= len;
    } while (size > 0);
    return total - size;
}

int cb_fifo_generic_read(CBFifoBuffer *f, any_t*dest, int buf_size, void (*func)(any_t*, any_t*, int))
{
// Read memory barrier needed for SMP here in theory
    do {
        int len = min(f->end - f->rptr, buf_size);
        if(func) func(dest, f->rptr, len);
        else{
            memcpy(dest, f->rptr, len);
            dest = (uint8_t*)dest + len;
        }
// memory barrier needed for SMP here in theory
        cb_fifo_drain(f, len);
        buf_size -= len;
    } while (buf_size > 0);
    return 0;
}
