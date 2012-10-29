
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "mp_config.h"
#include "version.h"

//#include "stream.h"
//#include "demuxer.h"
//#include "stheader.h"

#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"
#include "osdep/bswap.h"

#include "muxer.h"
#include "aviheader.h"

#include "demux_msg.h"

typedef struct priv_raw_stream_s
{
    uint32_t	ckid; // chunk id (00dc 01wb etc)
}priv_raw_stream_t;

static muxer_stream_t* rawfile_new_stream(muxer_t *muxer,int type){
    muxer_stream_t* s;
    if (!muxer) return NULL;
    if(muxer->avih.dwStreams>=MUXER_MAX_STREAMS){
	MSG_ERR("Too many streams! increase MUXER_MAX_STREAMS !\n");
	return NULL;
    }
    s=malloc(sizeof(muxer_stream_t));
    memset(s,0,sizeof(muxer_stream_t));
    if(!s) return NULL; // no mem!?
    muxer->streams[muxer->avih.dwStreams]=s;
    s->type=type;
    s->id=muxer->avih.dwStreams;
    s->timer=0.0;
    s->muxer=muxer;
    s->priv=malloc(sizeof(priv_raw_stream_t));
    memset(s->priv,0,sizeof(priv_raw_stream_t));
    s->size=0;
    switch(type){
    case MUXER_TYPE_VIDEO:
      ((priv_raw_stream_t *)s->priv)->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'d','c');
      s->h.fccType=streamtypeVIDEO;
      if(!muxer->def_v) muxer->def_v=s;
      break;
    case MUXER_TYPE_AUDIO:
      ((priv_raw_stream_t *)s->priv)->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'w','b');
      s->h.fccType=streamtypeAUDIO;
      break;
    case MUXER_TYPE_SUBS:
      ((priv_raw_stream_t *)s->priv)->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'t','x');
      break;
    default:
      MSG_WARN("WarninG! unknown stream type: %d\n",type);
      return NULL;
    }
    muxer->avih.dwStreams++;
    return s;
}

static void rawfile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags,float pts){
    muxer_t *muxer=s->muxer;
    fwrite(s->buffer,len,1,muxer->file);
}

static void rawfile_write_header(muxer_t *muxer,demuxer_t*dinfo){
}

static void rawfile_write_index(muxer_t *muxer){
}

void muxer_init_muxer_raw(muxer_t *muxer){
  muxer->cont_new_stream = &rawfile_new_stream;
  muxer->cont_write_chunk = &rawfile_write_chunk;
  muxer->cont_write_header = &rawfile_write_header;
  muxer->cont_write_index = &rawfile_write_index;
}
