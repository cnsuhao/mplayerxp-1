
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "mp_config.h"
#include "version.h"

#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"

#include "muxer.h"
#include "osdep/mplib.h"

muxer_packet_t* new_muxer_packet(float pts,any_t*data,unsigned length,unsigned flags)
{
    muxer_packet_t* retval;
    retval = new muxer_packet_t;
    retval->data = mp_malloc(length);
    retval->pts=pts;
    memcpy(retval->data,data,length);
    retval->length=length;
    retval->flags=flags;
    retval->next=NULL;
    return retval;
}

void free_muxer_packet(muxer_packet_t *packet)
{
    delete packet->data;
    delete packet;
}

muxer_t *muxer_new_muxer(const char *type,const char *subtype,FILE *f){
    muxer_t* muxer=new(zeromem) muxer_t;
    muxer->file = f;
//    if(!strcmp(type,"lavf")) { if(!muxer_init_muxer_lavf(muxer,subtype)) { delete muxer; muxer=NULL; }}
//    else
    if(!strcmp(type,"mpxp")) muxer_init_muxer_mpxp64(muxer);
    else
    if(!strcmp(type,"raw")) muxer_init_muxer_raw(muxer);
    else { delete muxer; muxer=NULL; }
    return muxer;
}
