/*
  MPlayerXP's design. MPXPAV64 format.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>

#include "mp_config.h"
#include "help_mp.h"

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "loader/qtx/qtxsdk/components.h"
#include "nls/nls.h"
#include "libmpsub/subreader.h"

#include "osdep/bswap.h"
#include "aviheader.h"
#include "libmpcodecs/dec_audio.h"
#include "libvo/sub.h"
#include "aviprint.h"
#include "mpxpav64.h"
#include "demux_msg.h"
#include "osdep/mplib.h"

#define MAX_AV_STREAMS MAX_V_STREAMS+MAX_A_STREAMS+MAX_S_STREAMS

typedef struct {
  unsigned nstreams;
  mpxpav64FileProperties_t fprop;
  mpxpav64StreamProperties_t sprop[MAX_AV_STREAMS];
  uint64_t data_off[MAX_AV_STREAMS];
  // deltas
  float prev_pts[MAX_AV_STREAMS];
  float prev_xpts[MAX_AV_STREAMS];
  uint64_t prev_size[MAX_AV_STREAMS];
  uint32_t prev_id;
  // index stuff:
  any_t* idx[MAX_AV_STREAMS];
  unsigned idx_size[MAX_AV_STREAMS];
} mpxpav64_priv_t;

#define le2me_ImageDesc(h) {						\
    (h)->idSize = le2me_32((h)->idSize);				\
    (h)->cType = le2me_32((h)->cType);					\
    (h)->resvd1 = le2me_32((h)->resvd1);				\
    (h)->resvd2 = le2me_16((h)->resvd2);				\
    (h)->dataRefIndex = le2me_16((h)->dataRefIndex);			\
    (h)->version = le2me_16((h)->version);				\
    (h)->revisionLevel = le2me_16((h)->revisionLevel);			\
    (h)->vendor = le2me_32((h)->vendor);				\
    (h)->temporalQuality = le2me_32((h)->temporalQuality);		\
    (h)->spatialQuality = le2me_32((h)->spatialQuality);		\
    (h)->width = le2me_16((h)->width);					\
    (h)->height = le2me_16((h)->height);				\
    (h)->vRes = le2me_32((h)->vRes);					\
    (h)->hRes = le2me_32((h)->hRes);					\
    (h)->dataSize = le2me_32((h)->dataSize);				\
    (h)->frameCount = le2me_16((h)->frameCount);			\
    (h)->depth = le2me_16((h)->depth);					\
    (h)->clutID = le2me_16((h)->clutID);				\
}

#define MAX_PACKS 4096
#define MIN(a,b) (((a)<(b))?(a):(b))

#if 0
/*
According to :
"MPEG Extension to AVI File Format "Editable MPEG FileFormat"
Draft Version 1.1 of 5/13/94"
*/
static float avi_aspects[]=
{
  1.0000, 0.6735, 0.7031, 0.7615, 0.8055, 0.8437, 0.8935,
  0.9375, 0.9815, 1.0255, 1.0695, 1.1250, 1.1575, 1.2015
};

static float get_avi_aspect(unsigned char id)
{
    if(id>0&&id<15)
    {
	return avi_aspects[id-1];
    }
    else return 1.0;
}
#endif

static void print_FileProp(mpxpav64FileProperties_t *fp)
{
  MSG_V("\n========== mpxpav64FileProperties_t =================\n"
	"#packets: %llu\n"
	"#nbytes: %llu\n"
	"#flags: %llu\n"
	"PlayDuration: %f\n"
	"Preroll: %f\n"
	"MaxBitrate: %lu\n"
	"AveBitrate: %lu\n"
	"#sterams: %u\n"
	"========== end of mpxpav64FileProperties_t ==========\n"
	,fp->num_packets
	,fp->num_bytes
	,fp->flags
	,(float)fp->PlayDuration/1000.
	,(float)fp->Preroll/1000.
	,fp->MaxBitrate
	,fp->AveBitrate
	,fp->StreamCount);
}

static void print_StreamProp(mpxpav64StreamProperties_t *sp,char *mime,uint32_t fourcc,uint64_t dataoff,uint64_t idxoff)
{
  char *p=(char *)&fourcc;
  MSG_V("\n====== mpxpav64StreamProperties_t '%c%c%c%c' ==============\n"
	"#packets: %llu\n"
	"#nbytes: %llu\n"
	"#flags: %llu\n"
	"PlayDuration: %f\n"
	"Preroll: %f\n"
	"MaxPacketSize: %lu\n"
	"AvePacketSize: %lu\n"
	"MinPacketSize: %lu\n"
	"MaxFrameDuration: %lu\n"
	"AveFrameDuration: %lu\n"
	"MinFrameDuration: %lu\n"
	"MaxBitrate: %lu\n"
	"AveBitrate: %lu\n"
	"MinBitrate: %lu\n"
	"mime: %s\n"
	"========== end of mpxpav64StreamProperties_t ==========\n"
	"Data off: %016llX\n"
	"Index off: %016llX\n"
	,p[0],p[1],p[2],p[3]
	,sp->num_packets
	,sp->num_bytes
	,sp->flags
	,(float)sp->PlayDuration/1000.
	,(float)sp->Preroll/1000.
	,sp->MaxPacketSize
	,sp->AvePacketSize
	,sp->MinPacketSize
	,sp->MaxFrameDuration
	,sp->AveFrameDuration
	,sp->MinFrameDuration
	,sp->MaxBitrate
	,sp->AveBitrate
	,sp->MinBitrate
	,mime
	,dataoff
	,idxoff);
}

static void mpxpav64_read_indexes(demuxer_t *demuxer,unsigned id,uint64_t idx_off)
{
    uint64_t i,fpos,iid;
    stream_t *s=demuxer->stream;
    mpxpav64_priv_t *priv=demuxer->priv;
    unsigned sid;
    int is_valid;
    fpos=stream_tell(s);
    stream_seek(s,idx_off);
    stream_read(s,(char *)&iid,8);
    is_valid=0;
    if(memcmp("IX32",&iid,4)==0)
    {
	stream_skip(s,-4);
	iid=stream_read_dword_le(s); /* index size */
	sid=stream_read_dword_le(s); /* stn */
	if(sid==id)
	{
	    is_valid=1;
	    priv->idx_size[id]=iid>>2; /* 32-bit in file */
	    priv->idx[id]=mp_malloc(priv->idx_size[id]<<3); /* 64-bit in memory */
	    for(i=0;i<priv->idx_size[id];i++)
		((uint64_t *)priv->idx[id])[i]=stream_read_dword_le(s);
	}
	else MSG_ERR("Index offset doesn't match to stream %u != %u\n",sid,id);
    }
    else
    if(memcmp("INDEX_64",&iid,8)==0)
    {
	iid=stream_read_qword_le(s); /* index size */
	sid=stream_read_dword_le(s); /* stn */
	if(sid==id)
	{
	    is_valid=1;
	    priv->idx_size[id]=iid>>3;
	    priv->idx[id]=mp_malloc(priv->idx_size[id]<<3);
	    for(i=0;i<priv->idx_size[id];i++)
		((uint64_t *)priv->idx[id])[i]=stream_read_qword_le(s);
	}
	else MSG_ERR("Index offset doesn't match to stream %u != %u\n",sid,id);
    }
    else MSG_ERR("Broken or incomplete file: can't find indexes\n");
    if(is_valid) MSG_V("For stream %u were found %lu indexes\n",id,priv->idx_size[id]);
    stream_seek(s,fpos);
}

static int mpxpav64_read_st64v(demuxer_t *demuxer,unsigned hsize,unsigned id){
    mpxpav64_priv_t *priv=demuxer->priv;
    stream_t *s=demuxer->stream;
    uint32_t fourcc,fsize;
    int have_bih=0;
    sh_video_t *sh=new_sh_video(demuxer,id);
    do {
	fourcc=stream_read_dword_le(s);
	fsize=stream_read_dword_le(s);
	MSG_V("Found %c%c%c%c chunk with %i bytes of size\n"
	,((char *)&fourcc)[0],((char *)&fourcc)[1]
	,((char *)&fourcc)[2],((char *)&fourcc)[3],
	fsize);
	switch(fourcc)
	{
	    case mmioFOURCC('B','I','H',' '):
		sh->bih=mp_malloc(fsize<sizeof(BITMAPINFOHEADER)?sizeof(BITMAPINFOHEADER):fsize);
		stream_read(s,(char *)sh->bih,fsize);
		le2me_BITMAPINFOHEADER(sh->bih);
		if(mp_conf.verbose>=1) print_video_header(sh->bih,fsize);
		have_bih=1;
		if(demuxer->video->id==-1) demuxer->video->id=id; /* TODO: select best */
		demuxer->video->sh=sh;
		sh->ds=demuxer->video;
		sh->fps=(float)(priv->sprop[id].num_packets*1000.)/(priv->sprop[id].PlayDuration-priv->sprop[id].Preroll);
		sh->fourcc=sh->bih->biCompression;
		sh->src_w=sh->bih->biWidth;
		sh->src_h=sh->bih->biHeight;
		MSG_V("FPS: %f\n",sh->fps);
		break;
	    case mmioFOURCC('v','p','r','p'):
		if(fsize<sizeof(VideoPropHeader))
		{
		    MSG_ERR("Too small vprp size %i\n",fsize);
		    stream_skip(s,fsize);
		}
		else
		{
		    VideoPropHeader vprp;
		    stream_read(s,(char *)&vprp,sizeof(VideoPropHeader));
		    le2me_VideoPropHeader(&vprp);
		    le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[0]);
		    le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[1]);
		    if(mp_conf.verbose) print_vprp(&vprp);
		    if(fsize>sizeof(VideoPropHeader)) stream_skip(s,fsize-sizeof(VideoPropHeader));
		    sh->aspect=GET_AVI_ASPECT(vprp.dwFrameAspectRatio);
		}
		break;
	    case mmioFOURCC('I','M','G','D'):
		if(fsize<sizeof(ImageDescription))
		{
		    MSG_ERR("Too small IMGD size %i\n",fsize);
		    stream_skip(s,fsize);
		}
		else
		{
		    sh->ImageDesc = mp_malloc(fsize);
		    stream_read(s,(char *)sh->ImageDesc,fsize);
		    le2me_ImageDesc(((ImageDescription *)sh->ImageDesc));
		}
		break;
	    default:
		MSG_WARN("Unknown video descriptor '%c%c%c%c' size %lu was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		stream_skip(s,fsize);
		break;
	}
    }while(stream_tell(s)<hsize);
    if(!have_bih) MSG_ERR("'BIH ' descriptor was not found\n");
    return have_bih;
}

static int mpxpav64_read_st64a(demuxer_t *demuxer,unsigned hsize,unsigned id){
    stream_t *s=demuxer->stream;
    uint32_t fourcc,fsize;
    int have_wf=0;
    sh_audio_t *sh=new_sh_audio(demuxer,id);
    do {
	fourcc=stream_read_dword_le(s);
	fsize=stream_read_dword_le(s);
	MSG_V("Found %c%c%c%c chunk with %i bytes of size\n"
	,((char *)&fourcc)[0],((char *)&fourcc)[1]
	,((char *)&fourcc)[2],((char *)&fourcc)[3],
	fsize);
	switch(fourcc)
	{
	    case mmioFOURCC('W','A','V','E'):
		sh->wf=mp_malloc(fsize<sizeof(WAVEFORMATEX)?sizeof(WAVEFORMATEX):fsize);
		stream_read(s,(char *)sh->wf,fsize);
		le2me_WAVEFORMATEX(sh->wf);
		if(mp_conf.verbose>=1) print_wave_header(sh->wf,fsize);
		have_wf=1;
		if(demuxer->audio->id==-1) demuxer->audio->id=id; /* TODO: select best */
		demuxer->audio->sh=sh;
		sh->ds=demuxer->audio;
		sh->wtag=sh->wf->wFormatTag;
		break;
	    case mmioFOURCC('F','R','C','C'):
		if(fsize!=4)
		{
		    MSG_ERR("Broken chunk FRCC with size=%i found\n",fsize);
		    goto do_def;
		}
		sh->wtag=stream_read_dword_le(s);
		if(sh->wf) sh->wf->wFormatTag=0;
		break;
	    do_def:
	    default:
		MSG_WARN("Unknown audio descriptor '%c%c%c%c' size %lu was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		stream_skip(s,fsize);
		break;
	}
    }while(stream_tell(s)<hsize);
    if(!have_wf) MSG_ERR("'WAVE' descriptor was not found\n");
    return have_wf;
}

static int mpxpav64_read_st64(demuxer_t *demuxer,unsigned hsize,unsigned id){
    stream_t *s=demuxer->stream;
    mpxpav64_priv_t *priv=demuxer->priv;
    uint64_t idx_off;
    uint32_t fourcc,hoff;
    hoff=stream_tell(s);
    fourcc=stream_read_dword_le(s);
    priv->data_off[id]=stream_read_qword_le(s);
    if((idx_off=stream_read_qword_le(s))!=0ULL) mpxpav64_read_indexes(demuxer,id,idx_off);
    /* Read stream properties */
    stream_read(s,(char *)&priv->sprop[id],sizeof(mpxpav64StreamProperties_t));
    le2me_mpxpav64StreamProperties(&priv->sprop[id]);
    if(mp_conf.verbose)
    {
	char mime[priv->sprop[id].mimetype_len+1];
	stream_read(s,mime,priv->sprop[id].mimetype_len);
	mime[priv->sprop[id].mimetype_len]='\0';
	print_StreamProp(&priv->sprop[id],mime,fourcc,priv->data_off[id],idx_off);
    }
    else stream_skip(s,priv->sprop[id].mimetype_len);
    if(priv->sprop[id].flags!=0ULL)
    {
	MSG_ERR("Unknown flags: %016llX for stream %u\n",priv->sprop[id].flags,id);
    }
    hoff+=hsize;
    do
    {
	switch(fourcc)
	{
	    case mmioFOURCC('v','i','d','s'): {
		MSG_V("Found video descriptor %lu bytes\n",hsize);
		if(!mpxpav64_read_st64v(demuxer,hoff,id)) return 0;
		break;
	    }
	    case mmioFOURCC('a','u','d','s'): {
		MSG_V("Found audio descriptor %lu bytes\n",hsize);
		if(!mpxpav64_read_st64a(demuxer,hoff,id)) return 0;
		break;
	    }
	    default:
		MSG_WARN("Unknown type of ST64: '%c%c%c%c'\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3]);
		break;
	}
    }while(stream_tell(s)<hoff);
    return 1;
}

#ifdef USE_ICONV
static void mpxpav64_read_fcnt(demuxer_t* demuxer,unsigned fsize)
{
    mpxpav64_priv_t *priv=demuxer->priv;
    stream_t *s=demuxer->stream;
    int64_t hoff;
    const char * codepage;
    hoff=stream_tell(s)+fsize;
    switch(priv->fprop.flags & 0x3)
    {
	case 0: codepage="UTF-7"; break;
	default:
	case 1: codepage="UTF-8"; break;
	case 2: codepage="UTF-16LE"; break;
	case 3: codepage="UTF-32LE"; break;
    }
    while(stream_tell(s)<hoff)
    {
	uint32_t fourcc,len;
	unsigned infot;
	char *str;
	fourcc=stream_read_dword_le(s);
	len=stream_read_word_le(s);
	infot=INFOT_NULL;
	switch(fourcc)
	{
	    case mmioFOURCC('A','U','T','H'): infot=INFOT_AUTHOR; break;
	    case mmioFOURCC('N','A','M','E'): infot=INFOT_NAME; break;
	    case mmioFOURCC('S','U','B','J'): infot=INFOT_SUBJECT; break;
	    case mmioFOURCC('C','P','Y','R'): infot=INFOT_COPYRIGHT; break;
	    case mmioFOURCC('D','E','S','C'): infot=INFOT_DESCRIPTION; break;
	    case mmioFOURCC('A','L','B','M'): infot=INFOT_ALBUM; break;
	    case mmioFOURCC('C','R','D','T'): infot=INFOT_DATE; break;
	    case mmioFOURCC('T','R','C','K'): infot=INFOT_TRACK; break;
	    case mmioFOURCC('G','E','N','R'): infot=INFOT_GENRE; break;
	    case mmioFOURCC('S','O','F','T'): infot=INFOT_ENCODER; break;
	    case mmioFOURCC('S','R','C','M'): infot=INFOT_SOURCE_MEDIA; break;
	    case mmioFOURCC('I','U','R','L'): infot=INFOT_WWW; break;
	    case mmioFOURCC('M','A','I','L'): infot=INFOT_MAIL; break;
	    case mmioFOURCC('R','A','T','E'): infot=INFOT_RATING; break;
	    case mmioFOURCC('C','M','T','S'): infot=INFOT_COMMENTS; break;
	    case mmioFOURCC('M','I','M','E'): infot=INFOT_MIME; break;
	    default: MSG_V("Unhandled contents descriptor %c%c%c%c %u bytes found\n",
			    ((char *)&fourcc)[0],((char *)&fourcc)[1],
			    ((char *)&fourcc)[2],((char *)&fourcc)[3],
			    (unsigned)len);
		    break;
	}
	if(infot)
	{
	    str=mp_malloc(len);
	    stream_read(s,str,len);
	    sub_data.cp=nls_get_screen_cp();
	    demux_info_add(demuxer,infot,nls_recode2screen_cp(codepage,str,len));
	    mp_free(str);
	}
	else stream_skip(s,len);
    }
}
#endif

static void mpxpav64_reset_prevs(demuxer_t *demuxer)
{
    mpxpav64_priv_t* priv=demuxer->priv;
    unsigned i;
    MSG_DBG2("mpxpav64_reset_prevs()\n");
    for(i=0;i<MAX_AV_STREAMS;i++)
    {
	priv->prev_xpts[i]=
	priv->prev_pts[i]=HUGE;
	priv->prev_size[i]=(uint64_t)-1;
    }
    priv->prev_id=0xFFFFFFFFUL;
}

static demuxer_t* mpxpav64_open(demuxer_t* demuxer){
    stream_t *s=demuxer->stream;
    uint64_t id,hsize,t;
    uint32_t fourcc;
    uint16_t scount=0;
    mpxpav64_priv_t* priv;

    stream_seek(s,0);
    id=stream_read_qword_le(s);
    if(memcmp(&id,"MPXPAV64",8)!=0) return NULL;
    stream_skip(s,8); /* skip filesize for partially downloaded files */
    id=stream_read_qword_le(s);
    if(memcmp(&id,"HEADER64",8)!=0) return NULL;
    hsize=stream_read_qword_le(s); /* header size */

    // priv struct:
    priv=mp_mallocz(sizeof(mpxpav64_priv_t));
    demuxer->priv=(any_t*)priv;
    demuxer->video->id=-1;
    demuxer->audio->id=-1;
    mpxpav64_reset_prevs(demuxer);
    do
    {
	uint32_t fsize;
	fourcc=stream_read_dword_le(s);
	switch(fourcc)
	{
	    case mmioFOURCC('F','P','R','P'): /* FileProperties */
		fsize=stream_read_dword_le(s);
		if(fsize<sizeof(mpxpav64FileProperties_t))
		{
		    MSG_ERR("Size of FPRP(%u) != %u\n",fsize,sizeof(mpxpav64FileProperties_t));
		    open_failed:
		    mp_free(priv);
		    return NULL;
		}
		stream_read(s,(char *)&priv->fprop,sizeof(mpxpav64FileProperties_t));
		le2me_mpxpav64FileProperties(&priv->fprop);
		demuxer->movi_length=(priv->fprop.PlayDuration-priv->fprop.Preroll)/1000;
		if((priv->nstreams=priv->fprop.StreamCount)>MAX_AV_STREAMS)
		{
		    too_many_streams:
		    MSG_ERR("Too many (%i) streams. Max available=%i\n",priv->nstreams,MAX_AV_STREAMS);
		    goto open_failed;
		}
		if(mp_conf.verbose) print_FileProp(&priv->fprop);
		stream_skip(s,fsize-sizeof(mpxpav64FileProperties_t));
		if((priv->fprop.flags&(~MPXPAV64_FP_FCNT_UTF32))!=0ULL)
		{
		    MSG_ERR("Unknown fileprop flags: %016llX\n",priv->fprop.flags);
		    goto open_failed;
		}
		break;
	    case mmioFOURCC('F','C','N','T'):
		fsize=stream_read_dword_le(s);
#ifdef USE_ICONV
		mpxpav64_read_fcnt(demuxer,fsize);
#else
		stream_skip(s,fsize); /* TODO read human readable info here */
#endif
		break;
	    case mmioFOURCC('S','T','6','4'):
		fsize=stream_read_dword_le(s);
		if(scount>=MAX_AV_STREAMS) goto too_many_streams;
		if(!mpxpav64_read_st64(demuxer,fsize,scount)) goto open_failed;
		scount++;
		break;
	    default:
		fsize=stream_read_dword_le(s);
		MSG_WARN("Unknown header '%c%c%c%c' with %lu bytes of size was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		stream_skip(s,fsize);
		break;
	}
    }while(stream_tell(s)<(off_t)hsize+16);
    t=stream_tell(s);
    id=stream_read_qword_le(s);
    hsize=stream_read_qword_le(s);
    if(memcmp(&id,"AVDATA64",8)!=0)
    {
	MSG_ERR("Can't find 'AVDATA64' chunk\n");
	goto open_failed;
    }
    demuxer->movi_start=stream_tell(s);
    demuxer->movi_end=demuxer->movi_start+hsize;
    MSG_V("Found AVDATA64 at offset %016llX bytes\n",t);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return demuxer;
}

static int mpxpav64_read_packet(demuxer_t *demux,unsigned id,uint64_t len,float pts,int keyframe)
{
    demux_stream_t *ds=NULL;
    stream_t* s=demux->stream;

    if(demux->video->id==-1)
	if(demux->v_streams[id])
	    demux->video->id=id;

    if(demux->audio->id==-1)
	if(demux->a_streams[id])
	    demux->audio->id=id;

    if(id==(unsigned)demux->audio->id){
	// audio
	ds=demux->audio;
	if(!ds->sh){
	    ds->sh=demux->a_streams[id];
	    MSG_V("Auto-selected MPXPAV64 audio ID = %d\n",ds->id);
	}
    } else
    if(id==(unsigned)demux->video->id){
	// video
	ds=demux->video;
	if(!ds->sh){
	    ds->sh=demux->v_streams[id];
	    MSG_V("Auto-selected MPXPAV64 video ID = %d\n",ds->id);
	}
    }
    if(ds){
	off_t pos=0LL;
	demux_packet_t* dp;
	dp=new_demux_packet(len);
	if(mp_conf.verbose>1) pos=stream_tell(s);
	len=stream_read(s,dp->buffer,len);
	resize_demux_packet(dp,len);
	dp->pts=pts;
	dp->flags=keyframe?DP_KEYFRAME:DP_NONKEYFRAME;
	dp->pos=demux->filepos;
	MSG_DBG2("MPXPAV64: reading %llu of %s PTS %f %skeyframe at %016llX\n",len,ds==demux->audio?"audio":"video",dp->pts,keyframe?"":"non",pos);
	ds_add_packet(ds,dp);
	return 1;
    }
    else
    {
	MSG_V("Unhandled %i stream_id found\n",id);
	stream_skip(s,len);
    }
    return 0;
}

/* DATx|stn|size|pts|stream data of size32 */
static int mpxpav64_demux(demuxer_t *demux,demux_stream_t *__ds){
    stream_t* s=demux->stream;
    mpxpav64_priv_t *priv=demux->priv;
    uint8_t flg;
    char p[8];
    uint64_t len;
    float pts=HUGE,xpts=HUGE;
    unsigned id;
    int bad_pts,bad_len;
    demux->filepos=stream_tell(s);
    if((demux->filepos>=demux->movi_end)){
	stream_set_eof(s,1);
	return 0;
    }
    if(stream_eof(s)) return 0; // EOF
    stream_read(s,p,2);
    if(p[0]=='S' && p[1]=='E')
    {
	off_t cl_off;
	cl_off=stream_tell(s);
	stream_read(s,p,2);
	if(p[0]=='E' && p[1] == 'K')
	{
	    MSG_DBG2("Found SEEK-point at %016llX\n",cl_off-2);
	    mpxpav64_reset_prevs(demux);
	    stream_read(s,&p[1],1);
	    p[0]='D';
	    goto do_next;
	}
	stream_seek(s,cl_off); /* wrong - not a cluster */
    }
    do_next:
    if(!(p[0]=='D'|| p[0]=='d'))
    {
	MSG_ERR("Broken stream! Can't find 'D','d' or 'SEEK' chunk. Have %02X instead\n",p[0]);
	stream_set_eof(s,1);
	return 0;
    }
    flg=p[1];
    if(flg&0x80)
    {
	if(flg&0x10)	id=stream_read_word_le(s);
	else		id=stream_read_char(s);
    }
    else		id=priv->prev_id;
    if(id>priv->nstreams)
    {
	MSG_ERR("Broken stream! stid( %i ) > nstreams( %i )\n",id,priv->nstreams);
	stream_set_eof(s,1);
	return 0;
    }
    bad_len=0;
    if(flg&0x40)
    {
	switch(flg&0x03)
	{
	    case 0:	len=stream_read_char(s)*priv->sprop[id].size_scaler; break;
	    case 1:	len=stream_read_word_le(s)*priv->sprop[id].size_scaler; break;
	    default:
	    case 2:	len=stream_read_dword_le(s)*priv->sprop[id].size_scaler; break;
	    case 3:	len=stream_read_qword_le(s)*priv->sprop[id].size_scaler; break;
	}
    }
    else		len=priv->prev_size[id];
    if(len == (uint64_t)-1)
    {
	MSG_ERR("Broken stream! Illegal frame size -1ULL found\n");
	stream_set_eof(s,1);
	return 0;
    }
    bad_pts=0;
    if(flg&0x20)
    {
	switch((flg&0x0C)>>2)
	{
	    case 0: xpts=(float)stream_read_char(s);
		    if(priv->prev_pts[id]==HUGE) bad_pts=1;
		    if(!bad_pts)
			pts=priv->prev_pts[id]+xpts/(float)priv->sprop[id].pts_rate;
		    break;
	    case 1: xpts=(float)stream_read_word_le(s);
		    if(priv->prev_pts[id]==HUGE) bad_pts=1;
		    if(!bad_pts)
			pts=priv->prev_pts[id]+xpts/(float)priv->sprop[id].pts_rate;
		    break;
	    default:
	    case 2: pts=(float)stream_read_dword_le(s)/(float)priv->sprop[id].pts_rate;
		    break;
	    case 3: pts=(float)stream_read_qword_le(s)/(float)priv->sprop[id].pts_rate;
		    break;
	}
    }
    else
    {
	if(priv->prev_xpts[id]==HUGE) bad_pts=2;
	if(!bad_pts)
	{
	    xpts=priv->prev_xpts[id];
	    pts=priv->prev_pts[id]+xpts/(float)priv->sprop[id].pts_rate;
	}
    }
    MSG_DBG2("Found SEEK(D)%02X %u %u %f chunk at %016llX\n",flg,id,len,pts,demux->filepos);
    if(bad_pts)
    {
	MSG_ERR("Broken stream! PTS of #%i stream is %s but PTS of prev frame is lost (%016llX)\n",id,bad_pts==2?"missing":"delta",demux->filepos);
	stream_set_eof(s,1);
	return 0;
    }
    mpxpav64_read_packet(demux,id,len,pts,p[0]=='D'?DP_KEYFRAME:DP_NONKEYFRAME);
    priv->prev_pts[id]=pts;
    priv->prev_xpts[id]=xpts;
    priv->prev_size[id]=len;
    priv->prev_id=id;
    return 1;
}

static int mpxpav64_test_seekpoint(demuxer_t *demuxer)
{
    stream_t* s=demuxer->stream;
    uint64_t len=0;
    int is_key,nkeys=0;
    char p[4];
    while(!stream_eof(s))
    {
	if(nkeys>5) return 1;
	stream_read(s,p,2);
	is_key=0;
	if(p[0]=='S' && p[1]=='E')
	{
	    stream_read(s,p,2);
	    if(p[0]!='E' || p[1]!='K') return 0;
	    p[1]=stream_read_char(s);
	    is_key=1;
	    nkeys++;
	}
	else
	if(!(p[0]=='D' || p[0]=='d')) return 0;
	if(p[1]&0x80) /* id */
	{
	    if(p[1]&0x10) stream_skip(s,2);
	    else	  stream_skip(s,1);
	}
	else if(is_key) return 0;
	if(p[1]&0x40)
	{
	    switch(p[1]&0x03)
	    {
		case 0:	len=stream_read_char(s); break;
		case 1:	len=stream_read_word_le(s); break;
		default:
		case 2:	len=stream_read_dword_le(s); break;
		case 3:	len=stream_read_qword_le(s); break;
	    }
	}
	else if(is_key) return 0; /* else copy from previous frame */
	if(p[1]&0x20)
	{
	    switch((p[3]&0x0C)>>2)
	    {
		case 0:	stream_skip(s,1);  break;
		case 1:	stream_skip(s,2); break;
		default:
		case 2:	stream_skip(s,4); break;
		case 3:	stream_skip(s,8); break;
	    }
	}
	else if(is_key) return 0;
	stream_skip(s,len);
    }
    return 0;
}

static int mpxpav64_sync(demuxer_t *demuxer)
{
    stream_t* s=demuxer->stream;
    char p[4];
    off_t rpos,cpos=stream_tell(s);
    while(!stream_eof(s))
    {
	rpos=stream_tell(s);
	stream_read(s,p,4);
	if(p[0]=='S' && p[1]=='E' && p[2]=='E' && p[3]=='K')
	{
	    stream_skip(s,-4);
	    if(mpxpav64_test_seekpoint(demuxer))
	    {
		stream_seek(s,rpos);
		return 1;
	    }
	}
	stream_skip(s,-3);
    }
    stream_seek(s,cpos);
    return 0;
}

#define USE_INDEXES 1
static void mpxpav64_seek(demuxer_t *demuxer,const seek_args_t* seeka){
    mpxpav64_priv_t *priv=demuxer->priv;
    float brate=priv->fprop.AveBitrate;
    off_t rel_seek_bytes=(seeka->flags&DEMUX_SEEK_PERCENTS)?
	(seeka->secs*(demuxer->movi_end-demuxer->movi_start)):
	(seeka->secs*brate);
    uint64_t newpos,cpos;
    int has_idx;
    cpos=stream_tell(demuxer->stream);
    newpos=((seeka->flags&DEMUX_SEEK_SET)?demuxer->movi_start:demuxer->filepos)+rel_seek_bytes;
    MSG_V("MPXPAV64_SEEK: We want %016llX newpos\n",newpos);
    /* have indexes */
    has_idx=0;
#ifdef USE_INDEXES
    if(demuxer->video->id>=0)
    {
	if(priv->idx[demuxer->video->id]) {
	    int64_t i,n;
	    has_idx=1;
	    n=priv->idx_size[demuxer->video->id];
	    if(newpos>cpos)
	    {
		for(i=0;i<n;i++)
		{
		    if(newpos<=((uint64_t*)priv->idx[demuxer->video->id])[i])
		    {
			newpos=((uint64_t*)priv->idx[demuxer->video->id])[i];
			break;
		    }
		}
	    }
	    else
	    {
		for(i=n-1;i;i--)
		{
		    if(newpos>=((uint64_t*)priv->idx[demuxer->video->id])[i])
		    {
			newpos=((uint64_t*)priv->idx[demuxer->video->id])[i];
			break;
		    }
		}
	    }
	    MSG_V("MPXPAV64_SEEK: newpos after indexes %016llX\n",newpos);
	}
	else MSG_V("Indexes are lost for #%u stream\n",demuxer->video->id);
    }
#endif
    if(newpos<(uint64_t)demuxer->movi_start) newpos=demuxer->movi_start;
    if(newpos>(uint64_t)demuxer->movi_end) goto cant_seek;
    stream_seek(demuxer->stream,newpos);
    if(!has_idx)
    {
	if(mpxpav64_sync(demuxer))
	{
		if(mp_conf.verbose) MSG_V("MPXPAV64_SEEK: newpos after sync %016llX\n",stream_tell(demuxer->stream));
		mpxpav64_reset_prevs(demuxer);
		mpca_resync_stream(((sh_audio_t*)demuxer->audio->sh)->decoder);
	}
	else
	{
		cant_seek:
		MSG_WARN("MPXPAV64_SEEK: can't find CLUSTER0\n");
		stream_seek(demuxer->stream,cpos);
	}
    }
}

static MPXP_Rc mpxpav64_probe(demuxer_t *demuxer)
{
  uint64_t id1,id2,id3;
  uint32_t id4;

  id1=stream_read_qword_le(demuxer->stream);/*MPXPAV64*/
  stream_skip(demuxer->stream,8); /*filesize */
  id2=stream_read_qword_le(demuxer->stream); /*HEADER64*/
  id3=stream_read_qword_le(demuxer->stream); /* headersize */
  stream_skip(demuxer->stream,id3);
  id3=stream_read_qword_le(demuxer->stream); /* AVDATA64 */
  stream_skip(demuxer->stream,8);
  id4=stream_read_dword_le(demuxer->stream); /* SEEK */
  demuxer->file_format=DEMUXER_TYPE_ASF;
  if(memcmp(&id1,"MPXPAV64",8)==0 &&
     memcmp(&id2,"HEADER64",8)==0 &&
     memcmp(&id3,"AVDATA64",8)==0 &&
     memcmp(&id4,"SEEK",4)==0) return MPXP_Ok;
  return MPXP_False;
}

static void mpxpav64_close(demuxer_t *demuxer)
{
  unsigned i;
  mpxpav64_priv_t* priv=demuxer->priv;
  if(!priv) return;
  for(i=0;i<MAX_AV_STREAMS;i++) if(priv->idx[i]!=NULL) mp_free(priv->idx[i]);
  mp_free(priv);
}

static MPXP_Rc mpxpav64_control(const demuxer_t *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

demuxer_driver_t demux_mpxpav64 =
{
    "MPXPAV64 - MPlayerXP's AudioVideo64 parser",
    ".mpxp",
    NULL,
    mpxpav64_probe,
    mpxpav64_open,
    mpxpav64_demux,
    mpxpav64_seek,
    mpxpav64_close,
    mpxpav64_control
};
