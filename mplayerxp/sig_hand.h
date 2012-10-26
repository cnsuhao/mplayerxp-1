/*
  MplayerXP's Signal handling
*/
#ifndef __SIG_HAND_H
#define __SIG_HAND_H 1

#include <sys/types.h>

#define MAX_XPTHREADS 16

typedef void (*mysighandler)(void);

typedef struct pth_info
{
    mysighandler sig_handler;
    pid_t pid;
    const char *thread_name;
    const char *current_module;
    void (*unlink)(int force);
    pthread_t pth_id;
} pth_info_t;

extern pth_info_t pinfo[MAX_XPTHREADS];
extern int xp_threads;
#define MP_UNIT(id,name) (pinfo[id].current_module=name)

int init_signal_handling( void (*callback)( void ),void (*unlink)(int));
void uninit_signal_handling( int xp_id );

#endif


