#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
   codecs_ld - light interface to codec loader
*/

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdarg.h>
#include "codecs_ld.h"
#include "mpxp_help.h"
#include "global_msg.h"

namespace	usr {
any_t* ld_codec(const std::string& name,const std::string& url_hint)
{
    any_t*dll_handle;
    if(!(dll_handle=::dlopen(name.c_str(),RTLD_LAZY|RTLD_GLOBAL))) {
	mpxp_fatal<<"[codec_ld] "<<MSGTR_CODEC_CANT_LOAD_DLL<<":"<<name<<" {"<<dlerror()<<"}"<<std::endl;
	if(!url_hint.empty()) mpxp_hint<<"[codec_ld] "<<MSGTR_CODEC_DLL_HINT<<":"<<url_hint<<std::endl;
	return NULL;
    }
    mpxp_v<<"[codec_ld] "<<MSGTR_CODEC_DLL_OK<<":"<<name<<std::endl;
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

any_t* ld_sym(any_t*handle,const std::string& sym_name)
{
    any_t*rval;
    if(!(rval=::dlsym(handle,sym_name.c_str()))) {
	mpxp_err<<"[codec_ld] "<<MSGTR_CODEC_DLL_SYM_ERR<<":"<<sym_name<<std::endl;
    }
    return rval;
}

any_t* ld_aliased_sym(any_t*handle,const std::string& sym_name,...)
{
    any_t*rval=::dlsym(handle,sym_name.c_str());
    if(!rval) {
	const char *alias;
	va_list list;
	va_start( list, sym_name );
	do {
	    alias = va_arg(list, const char *);
	    if(alias) rval = ::dlsym(handle,alias);
	    if(rval) break;
	}while(alias);
	va_end( list );
    }
    if(!rval) mpxp_err<<"[codec_ld] "<<MSGTR_CODEC_DLL_SYM_ERR<<":"<<sym_name<<std::endl;
    return rval;
}
} // namespace	usr
