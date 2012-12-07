#ifndef __ASF_STEAMING_H_INCLUDED
#define __ASF_STEAMING_H_INCLUDED 1
#include "stream.h"

namespace mpxp {
    class Tcp;
}
extern MPXP_Rc asf_networking_start(Tcp& fd, networking_t *networking);
extern MPXP_Rc asf_mmst_networking_start(Tcp& fd, networking_t *networking);

#endif
