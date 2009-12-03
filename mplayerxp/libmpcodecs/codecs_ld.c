/*
   codecs_ld - light interface to codec loader
*/

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include "codecs_ld.h"
#include "config.h"
#include "../help_mp.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "../__mp_msg.h"

void * ld_codec(const char *name,const char *url_hint)
{
  void *dll_handle;
  if(!(dll_handle=dlopen(name,RTLD_LAZY|RTLD_GLOBAL)))
  {
    MSG_FATAL(MSGTR_CODEC_CANT_LOAD_DLL,name,dlerror());
    if(url_hint) MSG_HINT(MSGTR_CODEC_DLL_HINT,url_hint);
    return NULL;
  }
  MSG_V(MSGTR_CODEC_DLL_OK,name);
  return dll_handle;
}

/* this code should be called before thread creating */
static char cname[FILENAME_MAX];
char * codec_name( const char *name )
{
  strcpy(cname,CODECDIR"/codecs/");
  strcat(cname,name);
  return cname;
}

char * wineld_name( const char *name )
{
  strcpy(cname,CODECDIR"/wine/");
  strcat(cname,name);
  return cname;
}

void * ld_sym(void *handle,const char *sym_name)
{
  void *rval;
  if(!(rval=dlsym(handle,sym_name)))
    MSG_ERR(MSGTR_CODEC_DLL_SYM_ERR,sym_name);
  return rval;
}
