#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "version.h"

#include "win32loader/wine/mmreg.h"
#include "win32loader/wine/avifmt.h"
#include "win32loader/wine/vfw.h"
#include "muxer.h"

muxer_packet_t* new_muxer_packet(float pts,any_t*data,unsigned length,unsigned flags)
{
    muxer_packet_t* retval;
    retval = new(zeromem) muxer_packet_t;
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

muxer_t *muxer_new_muxer(const std::string& type,const std::string& subtype,std::ofstream& f){
    muxer_t* muxer=new(zeromem) muxer_t(f);
//    if(type=="lavf") { if(!muxer_init_muxer_lavf(muxer,subtype)) { delete muxer; muxer=NULL; }}
//    else
    if(type=="mpxp") muxer_init_muxer_mpxp64(muxer);
    else
    if(type=="raw") muxer_init_muxer_raw(muxer);
    else { delete muxer; muxer=NULL; }
    return muxer;
}
