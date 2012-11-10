#ifndef __LIRC_H_INCLUDED
#define __LIRC_H_INCLUDED 1

extern int mp_input_lirc_init(void);
extern int mp_input_lirc_read(int fd,char* dest, int s);
extern void mp_input_lirc_close(int fd);
#endif
