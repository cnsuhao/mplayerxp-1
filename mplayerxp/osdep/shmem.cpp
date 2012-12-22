#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 *   shmem.c - Shared memory allocation
 *
 *   based on mpg123's xfermem.c by
 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
 *   Sun Apr  6 02:26:26 MET DST 1997
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <fcntl.h>

#ifdef AIX
#include <sys/select.h>
#endif

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "mpxp_help.h"
#include "osdep_msg.h"

#if defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON MAP_ANONYMOUS
#endif

static int shmem_type=0;

any_t* shmem_alloc(int size){
any_t* p;
static int devzero = -1;
while(1){
  switch(shmem_type){
  case 0:  // ========= MAP_ANON|MAP_SHARED ==========
#ifdef MAP_ANON
    p=mmap(0,size,PROT_READ|PROT_WRITE,MAP_ANON|MAP_SHARED,-1,0);
    if(p==MAP_FAILED) break; // failed
    mpxp_dbg2<<"shmem: "<<size<<" bytes allocated using mmap anon"<<std::endl;
    return p;
#else
// system does not support MAP_ANON at all (e.g. solaris 2.5.1/2.6), just fail
    mpxp_dbg3<<"shmem: using mmap anon failed"<<std::endl;
#endif
    break;
  case 1:  // ========= MAP_SHARED + /dev/zero ==========
    if (devzero == -1 && (devzero = open("/dev/zero", O_RDWR, 0)) == -1) break;
    p=mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,devzero,0);
    if(p==MAP_FAILED) break; // failed
    mpxp_dbg2<<"shmem: "<<size<<" bytes allocated using mmap /dev/zero"<<std::endl;
    return p;
  case 2: { // ========= shmget() ==========
#ifdef HAVE_SHM
    struct shmid_ds shmemds;
    int shmemid;
    if ((shmemid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600)) == -1) break;
    if ((p = shmat(shmemid, 0, 0)) == (any_t*)-1){
      mpxp_err<<"shmem: shmat() failed: "<<strerror(errno)<<std::endl;
      shmctl (shmemid, IPC_RMID, &shmemds);
      break;
    }
    if (shmctl(shmemid, IPC_RMID, &shmemds) == -1) {
      mpxp_err<<"shmem: shmctl() failed: "<<strerror(errno)<<std::endl;
      if (shmdt(p) == -1) perror ("shmdt()");
      break;
    }
    mpxp_dbg2<<"shmem: "<<size<<" bytes allocated using SHM"<<std::endl;
    return p;
#else
    mpxp_fatal<<"shmem: no SHM support was compiled in!"<<std::endl;
    return(NULL);
#endif
    }
  default:
    mpxp_fatal<<MSGTR_ShMemAllocFail<<std::endl;
    return NULL;
  }
  ++shmem_type;
}
}

void shmem_free(any_t* p){
  switch(shmem_type){
    case 2:
#ifdef HAVE_SHM
	if (shmdt(p) == -1)
	    mpxp_err<<"shmfree: shmdt() failed: "<<strerror(errno)<<std::endl;
#else
	mpxp_err<<"shmfree: no SHM support was compiled in!"<<std::endl;
#endif
      break;
  }
}
