/*
  MplayerXP's Signal handling
*/
#ifndef __SIG_HAND_H
#define __SIG_HAND_H 1

#include <sys/types.h>
#include "xmpcore/xmp_core.h"

#ifdef __cplusplus
extern "C" {
#endif
static inline void __MP_UNIT(unsigned id,const char *name)  { xp_core->mpxp_threads[id]->unit=name; }
static inline void MP_UNIT(const char *name) { xp_core->mpxp_threads[main_id]->unit=name; }

extern void init_signal_handling( void );
extern void uninit_signal_handling( int xp_id );
#ifdef __cplusplus
}
#endif

#endif


