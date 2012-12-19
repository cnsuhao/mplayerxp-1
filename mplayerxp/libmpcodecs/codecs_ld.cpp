#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
   codecs_ld - light interface to codec loader
*/

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdarg.h>
#include "codecs_ld.h"
#include "mpxp_help.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "mpxp_msg.h"

any_t* ld_codec(const char *name,const char *url_hint)
{
  any_t*dll_handle;
  if(!(dll_handle=dlopen(name,RTLD_LAZY|RTLD_GLOBAL)))
  {
    MSG_FATAL(MSGTR_CODEC_CANT_LOAD_DLL,name,dlerror());
    if(url_hint) MSG_HINT(MSGTR_CODEC_DLL_HINT,url_hint);
    return NULL;
  }
  MSG_V(MSGTR_CODEC_DLL_OK,name);
  return dll_handle;
}

#if 0
/* this code should be called before thread creating */
static char cname[FILENAME_MAX];
char * codec_name( const char *name )
{
  strcpy(cname,CODECDIR"/codecs/");
  strcat(cname,name);
  return cname;
}
#endif

any_t* ld_sym(any_t*handle,const char *sym_name)
{
  any_t*rval;
  if(!(rval=dlsym(handle,sym_name))) {
    MSG_ERR(MSGTR_CODEC_DLL_SYM_ERR,sym_name);
  }
  return rval;
}

any_t* ld_aliased_sym(any_t*handle,const char *sym_name,...)
{
  any_t*rval=dlsym(handle,sym_name);
  if(!rval) {
    const char *alias;
    va_list list;
    va_start( list, sym_name );
    do {
      alias = va_arg(list, const char *);
      if(alias) rval = dlsym(handle,alias);
      if(rval) break;
    }while(alias);
    va_end( list );
  }
  if(!rval) MSG_ERR(MSGTR_CODEC_DLL_SYM_ERR,sym_name);
  return rval;
}
