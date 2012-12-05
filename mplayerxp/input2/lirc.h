#ifndef __LIRC_H_INCLUDED
#define __LIRC_H_INCLUDED 1
namespace mpxp {
    extern any_t* mp_input_lirc_open(void);
    extern int mp_input_lirc_read_cmd(any_t* ctx,char* dest, int s);
    extern void mp_input_lirc_close(any_t* ctx);
} // namespace mpxp
#endif
