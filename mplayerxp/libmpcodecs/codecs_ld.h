/*
   codecs_ld - light interface to codec loader
*/

#ifndef __CODECS_LD
#define __CODECS_LD

#include "mpxp_config.h"

extern any_t* ld_codec(const char *name,const char *url_hint);
/*extern char * codec_name( const char *name );*/

extern any_t* ld_sym(any_t*handle,const char *sym_name);
extern any_t* ld_aliased_sym(any_t*handle,const char *sym_name,...);
#if defined(__OpenBSD__) && !defined(__ELF__)
#define dlsym(h,s) ld_sym(h, "_" s)
#endif
#endif
