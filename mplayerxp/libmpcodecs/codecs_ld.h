/*
   codecs_ld - light interface to codec loader
*/

#ifndef __CODECS_LD
#define __CODECS_LD

extern void * ld_codec(const char *name,const char *url_hint);
extern char * codec_name( const char *name );
extern char * wineld_name( const char *name );

extern void * ld_sym(void *handle,const char *sym_name);
#if defined(__OpenBSD__) && !defined(__ELF__)
#define dlsym(h,s) ld_sym(h, "_" s)
#endif

#endif
