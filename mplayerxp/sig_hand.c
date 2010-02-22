/*
  MplayerXP's Signal handling
*/
#include "mp_config.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* to get definition of strsignal */
#endif
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/resource.h>
#include "sig_hand.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "dec_ahead.h"
#include "mp_msg.h"

pth_info_t pinfo[MAX_XPTHREADS];
int xp_threads = 0;

extern pid_t mplayer_pid;
extern pthread_t mplayer_pth_id;
extern pid_t dec_ahead_pid;

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
/* Obtain a backtrace and print it to stdout. */
static void print_trace (void)
{
  void *array[10];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);

  MSG_ERR ("Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++)
     MSG_ERR ("%s\n", strings[i]);

  free (strings);
}

/* A dummy function to make the backtrace more interesting. */
static void dump_trace (void)
{
  print_trace ();
}
#endif
static void my_callback(int signo)
{
    int i;
    for(i=0; i < MAX_XPTHREADS && !pthread_equal(pinfo[i].pth_id, pthread_self()); i++);
    if(i >= MAX_XPTHREADS || i >= xp_threads || !pthread_equal(pinfo[i].pth_id, pthread_self())) {
	i = 0; /* Use 0 as default handler */
    }

   mp_msg(MSGT_CPLAYER,MSGL_FATAL,__FILE__,__LINE__,"catching signal: %s in thread: %s (%i) in module: %s\n",strsignal(signo),pinfo[i].thread_name,i,pinfo[i].current_module);
#ifdef HAVE_BACKTRACE
   dump_trace();
#endif
   pinfo[i].sig_handler();

   signal(signo,SIG_DFL);
   /* try coredump*/
   return;
}

int init_signal_handling( void (*callback)( void ),void (*_unlink)(int))
{
  if(xp_threads >= MAX_XPTHREADS)
    return MAX_XPTHREADS-1;
  pinfo[xp_threads].sig_handler = callback;
  pinfo[xp_threads].pid = getpid();
  pinfo[xp_threads].pth_id = pthread_self();
  pinfo[xp_threads].current_module = NULL;
  pinfo[xp_threads].unlink = _unlink;
  xp_threads++;
#if 1
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
#endif
#ifdef RLIMIT_CORE
  {
    /* on many systems default coresize is 0.
       Enable any coresize here. */
    struct rlimit rl;
    getrlimit(RLIMIT_CORE,&rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_CORE,&rl);
  }
#endif
  return xp_threads-1;
}

void uninit_signal_handling(int xp_id)
{
    pinfo[xp_id].pid = 0;
    pinfo[xp_id].pth_id = 0;
    pinfo[xp_id].current_module = NULL;
    pinfo[xp_id].sig_handler = NULL;

    if(xp_threads == xp_id+1) {
	while( xp_threads > 0 && pinfo[xp_threads-1].pid == 0 )
	    xp_threads--;
    }
}
