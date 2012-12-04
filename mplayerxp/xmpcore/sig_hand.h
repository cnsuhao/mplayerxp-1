/*
  MplayerXP's Signal handling
*/
#ifndef __SIG_HAND_H
#define __SIG_HAND_H 1

#include <sys/types.h>
#include "xmpcore/xmp_core.h"
#include "mplayerxp.h"

namespace mpxp {
    inline void __MP_UNIT(unsigned id,const char *name)  { mpxp_context().engine().xp_core->mpxp_threads[id]->unit=name; }
    inline void MP_UNIT(const char *name) { mpxp_context().engine().xp_core->mpxp_threads[main_id]->unit=name; }

    void init_signal_handling( void );
    void uninit_signal_handling( int xp_id );
} // namespace
#endif


