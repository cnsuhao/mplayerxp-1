/*
   codecs_ld - light interface to codec loader
*/
#ifndef __CODECS_LD
#define __CODECS_LD
#include "mpxp_config.h"
#include <string>

namespace mpxp {
    extern any_t* ld_codec(const std::string& name,const std::string& url_hint);
/*  extern char * codec_name( const char *name );*/
    extern any_t* ld_sym(any_t*handle,const std::string& sym_name);
    extern any_t* ld_aliased_sym(any_t*handle,const std::string& sym_name,...);
} // namespace mpxp
#endif
