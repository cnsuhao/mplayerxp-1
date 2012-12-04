#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
  MplayerXP's Signal handling
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* to get definition of strsignal */
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "mp_msg.h"
#include "sig_hand.h"
#define MSGT_CLASS MSGT_CPLAYER
#include "mp_msg.h"

#include <execinfo.h>

namespace mpxp {
/* A dummy function to make the backtrace more interesting. */
static void dump_trace (void) {
    show_backtrace("Obtained %zd stack frames.\n",mp_conf.max_trace);
    MSG_HINT("\nFor source lines you may also print in (gdb): list *0xADDRESS\n");
}

static void my_callback(int signo)
{
    int i;
    pthread_t _self = pthread_self();
    for(i=0; i < mpxp_context().engine().xp_core->num_threads && !pthread_equal(mpxp_context().engine().xp_core->mpxp_threads[i]->pth_id, _self); i++);
    if(i >= mpxp_context().engine().xp_core->num_threads ||
	!pthread_equal(mpxp_context().engine().xp_core->mpxp_threads[i]->pth_id, _self)) i = 0; /* Use 0 as default handler */

    MSG_FATAL("catching signal: %s in thread: %s (%i) in module: %s\n"
	,strsignal(signo)
	,mpxp_context().engine().xp_core->mpxp_threads[i]->name
	,i
	,mpxp_context().engine().xp_core->mpxp_threads[i]->unit);
    dump_trace();
#ifdef NDEBUG
    mpxp_context().engine().xp_core->mpxp_threads[i]->sigfunc();
#endif
    signal(signo,SIG_DFL); /* try coredump*/

    return;
}

void init_signal_handling( void )
{
  /*========= Catch terminate signals: ================*/
  /* terminate requests:*/
  signal(SIGTERM,my_callback); /* kill*/
  signal(SIGHUP,my_callback);  /* kill -HUP  /  xterm closed*/

  signal(SIGINT,my_callback);  /* Interrupt from keyboard */

  signal(SIGQUIT,my_callback); /* Quit from keyboard */
  /* fatal errors: */
  signal(SIGBUS,my_callback);  /* bus error */
  signal(SIGSEGV,my_callback); /* segfault */
  signal(SIGILL,my_callback);  /* illegal instruction */
  signal(SIGFPE,my_callback);  /* floating point exc. */
  signal(SIGABRT,my_callback); /* abort() */
#ifndef NDEBUG
    /* on many systems default coresize is 0.
       Enable any coresize here. */
    struct rlimit rl;
    getrlimit(RLIMIT_CORE,&rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_CORE,&rl);
#endif
}

} // namespace mpxp
