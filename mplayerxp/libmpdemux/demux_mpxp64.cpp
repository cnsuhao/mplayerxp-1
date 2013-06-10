#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
  MPlayerXP's design. MPXPAV64 format.
*/
#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>

#include "mpxp_help.h"

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "win32sdk/qtxsdk/components.h"
#include "nls/nls.h"
#include "libmpsub/subreader.h"

#include "osdep/bswap.h"
#include "aviheader.h"
#include "libmpcodecs/dec_audio.h"
#include "libvo2/sub.h"
#include "aviprint.h"
#include "mpxpav64.h"
#include "demux_msg.h"

static const int MAX_AV_STREAMS=MAX_V_STREAMS+MAX_A_STREAMS+MAX_S_STREAMS;

struct mpxpav64_priv_t : public Opaque {
    public:
	mpxpav64_priv_t() {}
	virtual ~mpxpav64_priv_t() {}

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
	uint64_t* idx[MAX_AV_STREAMS];
	unsigned idx_size[MAX_AV_STREAMS];
};

static void le2me_ImageDesc(ImageDescription* h) {
    h->idSize = le2me_32(h->idSize);
    h->cType = le2me_32(h->cType);
    h->resvd1 = le2me_32(h->resvd1);
    h->resvd2 = le2me_16(h->resvd2);
    h->dataRefIndex = le2me_16(h->dataRefIndex);
    h->version = le2me_16(h->version);
    h->revisionLevel = le2me_16(h->revisionLevel);
    h->vendor = le2me_32(h->vendor);
    h->temporalQuality = le2me_32(h->temporalQuality);
    h->spatialQuality = le2me_32(h->spatialQuality);
    h->width = le2me_16(h->width);
    h->height = le2me_16(h->height);
    h->vRes = le2me_32(h->vRes);
    h->hRes = le2me_32(h->hRes);
    h->dataSize = le2me_32(h->dataSize);
    h->frameCount = le2me_16(h->frameCount);
    h->depth = le2me_16(h->depth);
    h->clutID = le2me_16(h->clutID);
}

static const int MAX_PACKS=4096;

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

static void mpxpav64_read_indexes(Demuxer *demuxer,unsigned id,uint64_t idx_off)
{
    uint64_t i,fpos,iid;
    Stream *s=demuxer->stream;
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
    unsigned sid;
    int is_valid;
    fpos=s->tell();
    s->seek(idx_off);
    iid=s->read_qword();
    is_valid=0;
    if(memcmp("IX32",&iid,4)==0)
    {
	s->skip(-4);
	iid=s->read_dword_le(); /* index size */
	sid=s->read_dword_le(); /* stn */
	if(sid==id)
	{
	    is_valid=1;
	    priv->idx_size[id]=iid>>2; /* 32-bit in file */
	    priv->idx[id]=new uint64_t [priv->idx_size[id]]; /* 64-bit in memory */
	    for(i=0;i<priv->idx_size[id];i++)
		priv->idx[id][i]=s->read_dword_le();
	}
	else MSG_ERR("Index offset doesn't match to stream %u != %u\n",sid,id);
    }
    else
    if(memcmp("INDEX_64",&iid,8)==0)
    {
	iid=s->read_qword_le(); /* index size */
	sid=s->read_dword_le(); /* stn */
	if(sid==id)
	{
	    is_valid=1;
	    priv->idx_size[id]=iid>>3;
	    priv->idx[id]=new uint64_t [priv->idx_size[id]];
	    for(i=0;i<priv->idx_size[id];i++)
		priv->idx[id][i]=s->read_qword_le();
	}
	else MSG_ERR("Index offset doesn't match to stream %u != %u\n",sid,id);
    }
    else MSG_ERR("Broken or incomplete file: can't find indexes\n");
    if(is_valid) MSG_V("For stream %u were found %lu indexes\n",id,priv->idx_size[id]);
    s->seek(fpos);
}

static int mpxpav64_read_st64v(Demuxer *demuxer,unsigned hsize,unsigned id){
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
    Stream *s=demuxer->stream;
    uint32_t fourcc,fsize;
    int have_bih=0;
    binary_packet bp(1);
    sh_video_t *sh=demuxer->new_sh_video(id);
    do {
	fourcc=s->read_dword_le();
	fsize=s->read_dword_le();
	MSG_V("Found %c%c%c%c chunk with %i bytes of size\n"
	,((char *)&fourcc)[0],((char *)&fourcc)[1]
	,((char *)&fourcc)[2],((char *)&fourcc)[3],
	fsize);
	switch(fourcc)
	{
	    case mmioFOURCC('B','I','H',' '):
		sh->bih=(BITMAPINFOHEADER*)mp_malloc(fsize<sizeof(BITMAPINFOHEADER)?sizeof(BITMAPINFOHEADER):fsize);
		bp=s->read(fsize); memcpy(sh->bih,bp.data(),bp.size());
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
		    s->skip(fsize);
		}
		else
		{
		    VideoPropHeader vprp;
		    bp=s->read(sizeof(VideoPropHeader)); memcpy(&vprp,bp.data(),bp.size());
		    le2me_VideoPropHeader(&vprp);
		    le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[0]);
		    le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[1]);
		    if(mp_conf.verbose) print_vprp(&vprp);
		    if(fsize>sizeof(VideoPropHeader)) s->skip(fsize-sizeof(VideoPropHeader));
		    sh->aspect=GET_AVI_ASPECT(vprp.dwFrameAspectRatio);
		}
		break;
	    case mmioFOURCC('I','M','G','D'):
		if(fsize<sizeof(ImageDescription))
		{
		    MSG_ERR("Too small IMGD size %i\n",fsize);
		    s->skip(fsize);
		}
		else
		{
		    sh->ImageDesc = (ImageDescription *)mp_malloc(fsize);
		    bp=s->read(fsize); memcpy(sh->ImageDesc,bp.data(),bp.size());
		    le2me_ImageDesc(((ImageDescription *)sh->ImageDesc));
		}
		break;
	    default:
		MSG_WARN("Unknown video descriptor '%c%c%c%c' size %lu was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		s->skip(fsize);
		break;
	}
    }while(s->tell()<hsize);
    if(!have_bih) MSG_ERR("'BIH ' descriptor was not found\n");
    return have_bih;
}

static int mpxpav64_read_st64a(Demuxer *demuxer,unsigned hsize,unsigned id){
    Stream *s=demuxer->stream;
    uint32_t fourcc,fsize;
    int have_wf=0;
    binary_packet bp(1);
    sh_audio_t *sh=demuxer->new_sh_audio(id);
    do {
	fourcc=s->read_dword_le();
	fsize=s->read_dword_le();
	MSG_V("Found %c%c%c%c chunk with %i bytes of size\n"
	,((char *)&fourcc)[0],((char *)&fourcc)[1]
	,((char *)&fourcc)[2],((char *)&fourcc)[3],
	fsize);
	switch(fourcc)
	{
	    case mmioFOURCC('W','A','V','E'):
		sh->wf=(WAVEFORMATEX*)mp_malloc(fsize<sizeof(WAVEFORMATEX)?sizeof(WAVEFORMATEX):fsize);
		bp=s->read(fsize); memcpy((char *)sh->wf,bp.data(),bp.size());
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
		sh->wtag=s->read_dword_le();
		if(sh->wf) sh->wf->wFormatTag=0;
		break;
	    do_def:
	    default:
		MSG_WARN("Unknown audio descriptor '%c%c%c%c' size %lu was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		s->skip(fsize);
		break;
	}
    }while(s->tell()<hsize);
    if(!have_wf) MSG_ERR("'WAVE' descriptor was not found\n");
    return have_wf;
}

static int mpxpav64_read_st64(Demuxer *demuxer,unsigned hsize,unsigned id){
    Stream *s=demuxer->stream;
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
    uint64_t idx_off;
    uint32_t fourcc,hoff;
    hoff=s->tell();
    fourcc=s->read_dword_le();
    priv->data_off[id]=s->read_qword_le();
    if((idx_off=s->read_qword_le())!=0ULL) mpxpav64_read_indexes(demuxer,id,idx_off);
    /* Read stream properties */
    binary_packet bp=s->read(sizeof(mpxpav64StreamProperties_t)); memcpy((char *)&priv->sprop[id],bp.data(),bp.size());
    le2me_mpxpav64StreamProperties(&priv->sprop[id]);
    if(mp_conf.verbose)
    {
	char mime[priv->sprop[id].mimetype_len+1];
	bp=s->read(priv->sprop[id].mimetype_len); memcpy(mime,bp.data(),bp.size());
	mime[priv->sprop[id].mimetype_len]='\0';
	print_StreamProp(&priv->sprop[id],mime,fourcc,priv->data_off[id],idx_off);
    }
    else s->skip(priv->sprop[id].mimetype_len);
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
    }while(s->tell()<hoff);
    return 1;
}

#ifdef USE_ICONV
static void mpxpav64_read_fcnt(Demuxer* demuxer,unsigned fsize)
{
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
    Stream *s=demuxer->stream;
    int64_t hoff;
    const char * codepage;
    hoff=s->tell()+fsize;
    switch(priv->fprop.flags & 0x3)
    {
	case 0: codepage="UTF-7"; break;
	default:
	case 1: codepage="UTF-8"; break;
	case 2: codepage="UTF-16LE"; break;
	case 3: codepage="UTF-32LE"; break;
    }
    while(s->tell()<hoff)
    {
	uint32_t fourcc,len;
	unsigned infot;
	fourcc=s->read_dword_le();
	len=s->read_word_le();
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
//	    case mmioFOURCC('M','I','M','E'): infot=INFOT_MIME; break;
	    default: MSG_V("Unhandled contents descriptor %c%c%c%c %u bytes found\n",
			    ((char *)&fourcc)[0],((char *)&fourcc)[1],
			    ((char *)&fourcc)[2],((char *)&fourcc)[3],
			    (unsigned)len);
		    break;
	}
	if(infot)
	{
	    binary_packet bp=s->read(len);
	    sub_data.cp=nls_get_screen_cp(mpxp_get_environment());
	    demuxer->info().add(infot,nls_recode2screen_cp(codepage,bp.cdata(),bp.size()));
	}
	else s->skip(len);
    }
}
#endif

static void mpxpav64_reset_prevs(Demuxer *demuxer)
{
    mpxpav64_priv_t* priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
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

static Opaque* mpxpav64_open(Demuxer* demuxer){
    Stream *s=demuxer->stream;
    uint64_t id,hsize,t;
    uint32_t fourcc;
    uint16_t scount=0;
    mpxpav64_priv_t* priv;
    binary_packet bp(1);

    s->seek(0);
    id=s->read_qword_le();
    if(memcmp(&id,"MPXPAV64",8)!=0) return NULL;
    s->skip(8); /* skip filesize for partially downloaded files */
    id=s->read_qword_le();
    if(memcmp(&id,"HEADER64",8)!=0) return NULL;
    hsize=s->read_qword_le(); /* header size */

    // priv struct:
    priv=new(zeromem) mpxpav64_priv_t;
    demuxer->priv=priv;
    demuxer->video->id=-1;
    demuxer->audio->id=-1;
    mpxpav64_reset_prevs(demuxer);
    do
    {
	uint32_t fsize;
	fourcc=s->read_dword_le();
	switch(fourcc)
	{
	    case mmioFOURCC('F','P','R','P'): /* FileProperties */
		fsize=s->read_dword_le();
		if(fsize<sizeof(mpxpav64FileProperties_t))
		{
		    MSG_ERR("Size of FPRP(%u) != %u\n",fsize,sizeof(mpxpav64FileProperties_t));
		    open_failed:
		    delete priv;
		    return NULL;
		}
		bp=s->read(sizeof(mpxpav64FileProperties_t)); memcpy((char *)&priv->fprop,bp.data(),bp.size());
		le2me_mpxpav64FileProperties(&priv->fprop);
		demuxer->movi_length=(priv->fprop.PlayDuration-priv->fprop.Preroll)/1000;
		if((priv->nstreams=priv->fprop.StreamCount)>MAX_AV_STREAMS)
		{
		    too_many_streams:
		    MSG_ERR("Too many (%i) streams. Max available=%i\n",priv->nstreams,MAX_AV_STREAMS);
		    goto open_failed;
		}
		if(mp_conf.verbose) print_FileProp(&priv->fprop);
		s->skip(fsize-sizeof(mpxpav64FileProperties_t));
		if((priv->fprop.flags&(~MPXPAV64_FP_FCNT_UTF32))!=0ULL)
		{
		    MSG_ERR("Unknown fileprop flags: %016llX\n",priv->fprop.flags);
		    goto open_failed;
		}
		break;
	    case mmioFOURCC('F','C','N','T'):
		fsize=s->read_dword_le();
#ifdef USE_ICONV
		mpxpav64_read_fcnt(demuxer,fsize);
#else
		s->skip(fsize); /* TODO read human readable info here */
#endif
		break;
	    case mmioFOURCC('S','T','6','4'):
		fsize=s->read_dword_le();
		if(scount>=MAX_AV_STREAMS) goto too_many_streams;
		if(!mpxpav64_read_st64(demuxer,fsize,scount)) goto open_failed;
		scount++;
		break;
	    default:
		fsize=s->read_dword_le();
		MSG_WARN("Unknown header '%c%c%c%c' with %lu bytes of size was found\n"
			,((char *)&fourcc)[0]
			,((char *)&fourcc)[1]
			,((char *)&fourcc)[2]
			,((char *)&fourcc)[3],fsize);
		s->skip(fsize);
		break;
	}
    }while(s->tell()<(off_t)hsize+16);
    t=s->tell();
    id=s->read_qword_le();
    hsize=s->read_qword_le();
    if(memcmp(&id,"AVDATA64",8)!=0)
    {
	MSG_ERR("Can't find 'AVDATA64' chunk\n");
	goto open_failed;
    }
    demuxer->movi_start=s->tell();
    demuxer->movi_end=demuxer->movi_start+hsize;
    MSG_V("Found AVDATA64 at offset %016llX bytes\n",t);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return priv;
}

static int mpxpav64_read_packet(Demuxer *demux,unsigned id,uint64_t len,float pts,int keyframe)
{
    Demuxer_Stream *ds=NULL;
    Stream* s=demux->stream;

    if(demux->video->id==-1)
	if(demux->get_sh_video(id))
	    demux->video->id=id;

    if(demux->audio->id==-1)
	if(demux->get_sh_audio(id))
	    demux->audio->id=id;

    if(id==(unsigned)demux->audio->id){
	// audio
	ds=demux->audio;
	if(!ds->sh){
	    ds->sh=demux->get_sh_audio(id);
	    MSG_V("Auto-selected MPXPAV64 audio ID = %d\n",ds->id);
	}
    } else
    if(id==(unsigned)demux->video->id){
	// video
	ds=demux->video;
	if(!ds->sh){
	    ds->sh=demux->get_sh_video(id);
	    MSG_V("Auto-selected MPXPAV64 video ID = %d\n",ds->id);
	}
    }
    if(ds){
	off_t pos=0LL;
	Demuxer_Packet* dp=new(zeromem) Demuxer_Packet(len);
	if(mp_conf.verbose>1) pos=s->tell();
	binary_packet bp=s->read(len); memcpy(dp->buffer(),bp.data(),bp.size());
	len=bp.size();
	dp->resize(len);
	dp->pts=pts;
	dp->flags=keyframe?DP_KEYFRAME:DP_NONKEYFRAME;
	dp->pos=demux->filepos;
	MSG_DBG2("MPXPAV64: reading %llu of %s PTS %f %skeyframe at %016llX\n",len,ds==demux->audio?"audio":"video",dp->pts,keyframe?"":"non",pos);
	ds->add_packet(dp);
	return 1;
    }
    else
    {
	MSG_V("Unhandled %i stream_id found\n",id);
	s->skip(len);
    }
    return 0;
}

/* DATx|stn|size|pts|stream data of size32 */
static int mpxpav64_demux(Demuxer *demux,Demuxer_Stream *__ds){
    Stream* s=demux->stream;
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demux->priv);
    uint8_t flg;
    char p[8];
    uint64_t len;
    float pts=HUGE,xpts=HUGE;
    unsigned id;
    int bad_pts,bad_len;
    demux->filepos=s->tell();
    if((demux->filepos>=demux->movi_end)){
	s->eof(1);
	return 0;
    }
    if(s->eof()) return 0; // EOF
    binary_packet bp=s->read(2); memcpy(p,bp.data(),bp.size());
    if(p[0]=='S' && p[1]=='E')
    {
	off_t cl_off;
	cl_off=s->tell();
	bp=s->read(2); memcpy(p,bp.data(),bp.size());
	if(p[0]=='E' && p[1] == 'K')
	{
	    MSG_DBG2("Found SEEK-point at %016llX\n",cl_off-2);
	    mpxpav64_reset_prevs(demux);
	    p[1]=s->read_char();
	    p[0]='D';
	    goto do_next;
	}
	s->seek(cl_off); /* wrong - not a cluster */
    }
    do_next:
    if(!(p[0]=='D'|| p[0]=='d'))
    {
	MSG_ERR("Broken stream! Can't find 'D','d' or 'SEEK' chunk. Have %02X instead\n",p[0]);
	s->eof(1);
	return 0;
    }
    flg=p[1];
    if(flg&0x80)
    {
	if(flg&0x10)	id=s->read_word_le();
	else		id=s->read_char();
    }
    else		id=priv->prev_id;
    if(id>priv->nstreams)
    {
	MSG_ERR("Broken stream! stid( %i ) > nstreams( %i )\n",id,priv->nstreams);
	s->eof(1);
	return 0;
    }
    bad_len=0;
    if(flg&0x40)
    {
	switch(flg&0x03)
	{
	    case 0:	len=s->read_char()*priv->sprop[id].size_scaler; break;
	    case 1:	len=s->read_word_le()*priv->sprop[id].size_scaler; break;
	    default:
	    case 2:	len=s->read_dword_le()*priv->sprop[id].size_scaler; break;
	    case 3:	len=s->read_qword_le()*priv->sprop[id].size_scaler; break;
	}
    }
    else		len=priv->prev_size[id];
    if(len == (uint64_t)-1)
    {
	MSG_ERR("Broken stream! Illegal frame size -1ULL found\n");
	s->eof(1);
	return 0;
    }
    bad_pts=0;
    if(flg&0x20)
    {
	switch((flg&0x0C)>>2)
	{
	    case 0: xpts=(float)s->read_char();
		    if(priv->prev_pts[id]==HUGE) bad_pts=1;
		    if(!bad_pts)
			pts=priv->prev_pts[id]+xpts/(float)priv->sprop[id].pts_rate;
		    break;
	    case 1: xpts=(float)s->read_word_le();
		    if(priv->prev_pts[id]==HUGE) bad_pts=1;
		    if(!bad_pts)
			pts=priv->prev_pts[id]+xpts/(float)priv->sprop[id].pts_rate;
		    break;
	    default:
	    case 2: pts=(float)s->read_dword_le()/(float)priv->sprop[id].pts_rate;
		    break;
	    case 3: pts=(float)s->read_qword_le()/(float)priv->sprop[id].pts_rate;
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
	s->eof(1);
	return 0;
    }
    mpxpav64_read_packet(demux,id,len,pts,p[0]=='D'?DP_KEYFRAME:DP_NONKEYFRAME);
    priv->prev_pts[id]=pts;
    priv->prev_xpts[id]=xpts;
    priv->prev_size[id]=len;
    priv->prev_id=id;
    return 1;
}

static int mpxpav64_test_seekpoint(Demuxer *demuxer)
{
    Stream* s=demuxer->stream;
    uint64_t len=0;
    int is_key,nkeys=0;
    char p[4];
    binary_packet bp(1);
    while(!s->eof())
    {
	if(nkeys>5) return 1;
	bp=s->read(2); memcpy(p,bp.data(),bp.size());
	is_key=0;
	if(p[0]=='S' && p[1]=='E')
	{
	    bp=s->read(2); memcpy(p,bp.data(),bp.size());
	    if(p[0]!='E' || p[1]!='K') return 0;
	    p[1]=s->read_char();
	    is_key=1;
	    nkeys++;
	}
	else
	if(!(p[0]=='D' || p[0]=='d')) return 0;
	if(p[1]&0x80) /* id */
	{
	    if(p[1]&0x10) s->skip(2);
	    else	  s->skip(1);
	}
	else if(is_key) return 0;
	if(p[1]&0x40)
	{
	    switch(p[1]&0x03)
	    {
		case 0:	len=s->read_char(); break;
		case 1:	len=s->read_word_le(); break;
		default:
		case 2:	len=s->read_dword_le(); break;
		case 3:	len=s->read_qword_le(); break;
	    }
	}
	else if(is_key) return 0; /* else copy from previous frame */
	if(p[1]&0x20)
	{
	    switch((p[3]&0x0C)>>2)
	    {
		case 0:	s->skip(1);  break;
		case 1:	s->skip(2); break;
		default:
		case 2:	s->skip(4); break;
		case 3:	s->skip(8); break;
	    }
	}
	else if(is_key) return 0;
	s->skip(len);
    }
    return 0;
}

static int mpxpav64_sync(Demuxer *demuxer)
{
    Stream* s=demuxer->stream;
    char p[4];
    off_t rpos,cpos=s->tell();
    while(!s->eof())
    {
	rpos=s->tell();
	binary_packet bp=s->read(4); memcpy(p,bp.data(),bp.size());
	if(p[0]=='S' && p[1]=='E' && p[2]=='E' && p[3]=='K')
	{
	    s->skip(-4);
	    if(mpxpav64_test_seekpoint(demuxer))
	    {
		s->seek(rpos);
		return 1;
	    }
	}
	s->skip(-3);
    }
    s->seek(cpos);
    return 0;
}

static const int USE_INDEXES=1;
static void mpxpav64_seek(Demuxer *demuxer,const seek_args_t* seeka){
    mpxpav64_priv_t *priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
    float brate=priv->fprop.AveBitrate;
    off_t rel_seek_bytes=(seeka->flags&DEMUX_SEEK_PERCENTS)?
	(seeka->secs*(demuxer->movi_end-demuxer->movi_start)):
	(seeka->secs*brate);
    uint64_t newpos,cpos;
    int has_idx;
    cpos=demuxer->stream->tell();
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
		    if(newpos<=priv->idx[demuxer->video->id][i])
		    {
			newpos=priv->idx[demuxer->video->id][i];
			break;
		    }
		}
	    }
	    else
	    {
		for(i=n-1;i;i--)
		{
		    if(newpos>=priv->idx[demuxer->video->id][i])
		    {
			newpos=priv->idx[demuxer->video->id][i];
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
    demuxer->stream->seek(newpos);
    if(!has_idx) {
	if(mpxpav64_sync(demuxer)) {
		if(mp_conf.verbose) MSG_V("MPXPAV64_SEEK: newpos after sync %016llX\n",demuxer->stream->tell());
		mpxpav64_reset_prevs(demuxer);
	}
	else {
		cant_seek:
		MSG_WARN("MPXPAV64_SEEK: can't find CLUSTER0\n");
		demuxer->stream->seek(cpos);
	}
    }
}

static MPXP_Rc mpxpav64_probe(Demuxer *demuxer)
{
  uint64_t id1,id2,id3;
  uint32_t id4;

  id1=demuxer->stream->read_qword_le();/*MPXPAV64*/
  demuxer->stream->skip(8); /*filesize */
  id2=demuxer->stream->read_qword_le(); /*HEADER64*/
  id3=demuxer->stream->read_qword_le(); /* headersize */
  demuxer->stream->skip(id3);
  id3=demuxer->stream->read_qword_le(); /* AVDATA64 */
  demuxer->stream->skip(8);
  id4=demuxer->stream->read_dword_le(); /* SEEK */
  demuxer->file_format=Demuxer::Type_ASF;
  if(memcmp(&id1,"MPXPAV64",8)==0 &&
     memcmp(&id2,"HEADER64",8)==0 &&
     memcmp(&id3,"AVDATA64",8)==0 &&
     memcmp(&id4,"SEEK",4)==0) return MPXP_Ok;
  return MPXP_False;
}

static void mpxpav64_close(Demuxer *demuxer)
{
  unsigned i;
  mpxpav64_priv_t* priv=static_cast<mpxpav64_priv_t*>(demuxer->priv);
  if(!priv) return;
  for(i=0;i<MAX_AV_STREAMS;i++) if(priv->idx[i]!=NULL) delete priv->idx[i];
  delete priv;
}

static MPXP_Rc mpxpav64_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_mpxpav64 =
{
    "mpxpav64",
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
