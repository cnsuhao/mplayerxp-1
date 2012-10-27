/*
  MplayerXP's Signal handling
*/
#ifndef __SIG_HAND_H
#define __SIG_HAND_H 1

#include <sys/types.h>
#include "xmp_core.h"

#define __MP_UNIT(id,name) (xp_core.mpxp_threads[id]->unit=name)
#define MP_UNIT(name) (xp_core.mpxp_threads[main_id]->unit=name)

extern void init_signal_handling( void );
extern void uninit_signal_handling( int xp_id );

#endif


