#include "mplayer.h"
#include "fifo.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

// keyboard:
int keyb_fifo_put=-1;
int keyb_fifo_get=-1;

void __FASTCALL__ fifo_make_pipe(int* pr,int* pw){
  int temp[2];
  if(pipe(temp)!=0) MSG_ERR("Cannot make PIPE!\n");
  *pr=temp[0];
  *pw=temp[1];
}

static inline int __FASTCALL__ my_write(int fd,unsigned char* mem,int len){
  int total=0;
  int len2;
  while(len>0){
    len2=write(fd,mem+total,len); if(len2<=0) break;
    total+=len2;len-=len2;
  }
  return total;
}

static inline int __FASTCALL__ my_read(int fd,unsigned char* mem,int len){
  int total=0;
  int len2;
  while(len>0){
    len2=read(fd,mem+total,len); if(len2<=0) break;
    total+=len2;len-=len2;
  }
  return total;
}

static void __FASTCALL__ send_cmd(int fd,int cmd){
  int fifo_cmd=cmd;
  write(fd,&fifo_cmd,4);
//  fflush(control_fifo);
}


void mplayer_put_key(int code){
           fd_set rfds;
           struct timeval tv;

           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds);
           FD_SET(keyb_fifo_put, &rfds);
           tv.tv_sec = 0;
           tv.tv_usec = 0;

           //retval = select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv);
           if(select(keyb_fifo_put+1, NULL, &rfds, NULL, &tv)>0){
             write(keyb_fifo_put,&code,4);
           }
}

int mplayer_get_key(void){
           fd_set rfds;
           struct timeval tv;
           int code=-1;

           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds);
           FD_SET(keyb_fifo_get, &rfds);
           tv.tv_sec = 0;
           tv.tv_usec = 0;

           //retval = select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv);
           if(select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv)>0){
             read(keyb_fifo_get,&code,4);
           }
           return code;
}

/*  === Circular Buffer FIFO support === */

CBFifoBuffer *cb_fifo_alloc(unsigned int size)
{
    CBFifoBuffer *f= malloc(sizeof(CBFifoBuffer));
    if(!f)
        return NULL;
    memset(f,0,sizeof(CBFifoBuffer));
    f->buffer = malloc(size);
    f->end = f->buffer + size;
    cb_fifo_reset(f);
    if (!f->buffer)
        free(f);
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
        free(f->buffer);
        *f= *f2;
        free(f2);
    }
    return 0;
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
