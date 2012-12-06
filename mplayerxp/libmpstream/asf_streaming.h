#ifndef __ASF_STEAMING_H_INCLUDED
#define __ASF_STEAMING_H_INCLUDED 1
#include "stream.h"

typedef int net_fd_t;
extern int asf_networking_start(net_fd_t* fd, networking_t *networking);
extern int asf_mmst_networking_start(net_fd_t* fd, networking_t *networking);

#endif
