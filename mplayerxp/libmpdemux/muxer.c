
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "../mp_config.h"
#include "../version.h"

#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"

#include "muxer.h"

muxer_t *muxer_new_muxer(const char *type,const char *subtype,FILE *f){
    muxer_t* muxer=malloc(sizeof(muxer_t));
    memset(muxer,0,sizeof(muxer_t));
    muxer->file = f;
//    if(!strcmp(type,"lavf")) { if(!muxer_init_muxer_lavf(muxer,subtype)) { free(muxer); muxer=NULL; }}
//    else
    if(!strcmp(type,"mpxp")) muxer_init_muxer_mpxp64(muxer);
    else
    if(!strcmp(type,"raw")) muxer_init_muxer_raw(muxer);
    else { free(muxer); muxer=NULL; }
    return muxer;
}
