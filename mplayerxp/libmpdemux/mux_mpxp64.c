/*
  MPlayerXP's design. MPXPAV64 format.
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>

#include "mp_config.h"
#include "version.h"
#include "nls/nls.h"

#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"
#include "loader/qtx/qtxsdk/components.h"
#include "osdep/bswap.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "muxer.h"
#include "aviheader.h"

#include "mpxpav64.h"
#include "demux_msg.h"
#include "osdep/mplib.h"

typedef struct priv_mpxpav64_stream_s
{
    /* statistics */
    uint32_t	max_packet_size,min_packet_size;
    uint32_t	max_brate,min_brate;
    uint64_t	nbytes;
    uint64_t	npackets;
    float	prev_pts,first_pts,last_pts;
    float	max_frame_duration,min_frame_duration;
    // offsets
    uint64_t	data_off,idx_off;
    // indexes
    any_t*	idx;
    unsigned	idx_size;
    unsigned int prev_len;
    float	prev_xpts;
}priv_mpxpav64_stream_t;

#define SEEKPOINT_THRESHOLD 1.0
#define PTS2INT(pts) (lrint(pts*1000))

typedef struct priv_mpxpav64_s
{
    uint64_t mainh,datah;
    unsigned int prev_id;
    float prev_seek;
}priv_mpxpav64_t;

static void mpxpav64_put64(FILE *f,uint64_t value)
{
    uint64_t val;
    val = me2le_64(value);
    fwrite(&val,8,1,f);
}

static void mpxpav64_put32(FILE *f,uint32_t value)
{
    uint32_t val;
    val = me2le_32(value);
    fwrite(&val,4,1,f);
}

static void mpxpav64_put16(FILE *f,uint16_t value)
{
    uint16_t val;
    val = me2le_16(value);
    fwrite(&val,2,1,f);
}

static void mpxpav64_put8(FILE *f,uint8_t value)
{
    fwrite(&value,1,1,f);
}

static void mpxpav64_put_pts64(FILE *f,double value)
{
    uint64_t val;
    val = llrint(value*1000); /* 1 ms */
    mpxpav64_put64(f,val);
}

static void mpxpav64_put_pts32(FILE *f,float value)
{
    uint32_t val;
    val = PTS2INT(value); /* 1 ms */
    mpxpav64_put32(f,val);
}

static void mpxpav64_put_pts16(FILE *f,float value)
{
    uint16_t val;
    val = PTS2INT(value); /* 1 ms */
    mpxpav64_put16(f,val);
}

static void mpxpav64_put_pts8(FILE *f,float value)
{
    uint8_t val;
    val = PTS2INT(value); /* 1 ms */
    mpxpav64_put8(f,val);
}

static void mpxpav64_put_unicode(FILE *f, const char *tag)
{
    int len;
    char *str=nls_recode_from_screen_cp("UTF-16LE",tag,&len);
    mpxpav64_put16(f,len);
    fwrite(str,len,1,f);
    mp_free(str);
}

static void mpxpav64_put_frcc_unicode(FILE *f, const char *frcc,const char *tag)
{
    fwrite(frcc,4,1,f);
    mpxpav64_put_unicode(f,tag);
}

static uint64_t mpxpav64_open_header64(FILE *f,const char *id)
{
    fwrite(id,8,1,f);
    mpxpav64_put64(f,-1);
    return ftello(f);
}

static void mpxpav64_close_header64(FILE *f,uint64_t header_start)
{
    uint64_t header_end;
    header_end = ftello(f);
    fseeko(f,header_start-8,SEEK_SET);
    mpxpav64_put64(f,header_end-header_start);
    fseeko(f,header_end,SEEK_SET);
}

static uint64_t mpxpav64_open_header32(FILE *f,const char *id)
{
    fwrite(id,4,1,f);
    mpxpav64_put32(f,-1);
    return ftello(f);
}

static void mpxpav64_close_header32(FILE *f,uint64_t header_start)
{
    uint64_t header_end;
    header_end = ftello(f);
    fseeko(f,header_start-4,SEEK_SET);
    mpxpav64_put32(f,header_end-header_start);
    fseeko(f,header_end,SEEK_SET);
}

static muxer_stream_t* mpxpav64_new_stream(muxer_t *muxer,int type)
{
    muxer_stream_t* s;
    if (!muxer) return NULL;
    if(muxer->avih.dwStreams>=MUXER_MAX_STREAMS){
	MSG_ERR("Too many streams! increase MUXER_MAX_STREAMS !\n");
	return NULL;
    }
    s=mp_mallocz(sizeof(muxer_stream_t));
    if(!s) return NULL; // no mem!?
    if(!muxer->priv)
    {
	muxer->priv=mp_mallocz(sizeof(priv_mpxpav64_t));
	((priv_mpxpav64_t *)muxer->priv)->prev_seek=-SEEKPOINT_THRESHOLD*2;
    }
    s->priv=mp_mallocz(sizeof(priv_mpxpav64_stream_t));
    muxer->streams[muxer->avih.dwStreams]=s;
    s->type=type;
    s->id=muxer->avih.dwStreams;
    s->muxer=muxer;
    ((priv_mpxpav64_stream_t *)s->priv)->first_pts=
    ((priv_mpxpav64_stream_t *)s->priv)->prev_pts=
    ((priv_mpxpav64_stream_t *)s->priv)->min_frame_duration=HUGE;
    ((priv_mpxpav64_stream_t *)s->priv)->min_brate=
    ((priv_mpxpav64_stream_t *)s->priv)->min_packet_size=0xFFFFFFFFUL;
    switch(type){
    case MUXER_TYPE_VIDEO:
      if(!muxer->def_v) muxer->def_v=s;
      break;
    case MUXER_TYPE_SUBS:
    case MUXER_TYPE_AUDIO:
      break;
    default:
      MSG_WARN("WarninG! unknown stream type: %d\n",type);
      break;
    }
    muxer->avih.dwStreams++;
    return s;
}

static void mpxpav64_put_fcnt(muxer_t *muxer,demuxer_t*dinfo)
{
    uint64_t fpos;
    FILE *f = muxer->file;
    const char *sname;
    fpos=mpxpav64_open_header32(f,"FCNT");
#ifdef USE_ICONV
    if((sname=demux_info_get(dinfo,INFOT_AUTHOR))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"AUTH",sname);
    if((sname=demux_info_get(dinfo,INFOT_NAME))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"NAME",sname);
    if((sname=demux_info_get(dinfo,INFOT_SUBJECT))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"SUBJ",sname);
    if((sname=demux_info_get(dinfo,INFOT_COPYRIGHT))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"CPYR",sname);
    if((sname=demux_info_get(dinfo,INFOT_DESCRIPTION))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"DESC",sname);
    if((sname=demux_info_get(dinfo,INFOT_ALBUM))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"ALBM",sname);
    if((sname=demux_info_get(dinfo,INFOT_DATE))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"CRDT",sname);
    if((sname=demux_info_get(dinfo,INFOT_TRACK))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"TRCK",sname);
    if((sname=demux_info_get(dinfo,INFOT_GENRE))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"GENR",sname);
    if((sname=demux_info_get(dinfo,INFOT_ENCODER))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"SOFT",sname);
    if((sname=demux_info_get(dinfo,INFOT_SOURCE_MEDIA))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"SRCM",sname);
    if((sname=demux_info_get(dinfo,INFOT_WWW))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"IURL",sname);
    if((sname=demux_info_get(dinfo,INFOT_MAIL))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"MAIL",sname);
    if((sname=demux_info_get(dinfo,INFOT_RATING))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"RATE",sname);
    if((sname=demux_info_get(dinfo,INFOT_COMMENTS))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"CMTS",sname);
    if((sname=demux_info_get(dinfo,INFOT_MIME))!=NULL)
	if(sname[0]) mpxpav64_put_frcc_unicode(f,"MIME",sname);
#endif
    mpxpav64_close_header32(f,fpos);
}

static unsigned int avi_aspect(float aspect)
{
    int x,y;

    if (aspect <= 0.0) return 0;

    if (aspect > 15.99/9.0 && aspect < 16.01/9.0) {
        return MAKE_AVI_ASPECT(16, 9);
    }
    if (aspect > 3.99/3.0 && aspect < 4.01/3.0) {
        return MAKE_AVI_ASPECT(4, 3);
    }

    if (aspect >= 1.0) {
        x = 16384;
        y = (float)x / aspect;
    } else {
        y = 16384;
        x = (float)y * aspect;
    }

    return MAKE_AVI_ASPECT(x, y);
}

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

#define WFSIZE(wf) (sizeof(WAVEFORMATEX)+(((wf)->cbSize)?((wf)->cbSize):0))
static void mpxpav64_put_st64(muxer_t *muxer,muxer_stream_t* s)
{
    priv_mpxpav64_stream_t *privs=(priv_mpxpav64_stream_t *)s->priv;
    uint64_t fpos,spos,flags=0;
    uint8_t* frcc;
    FILE *f = muxer->file;
    VideoPropHeader vprp;
    fpos=mpxpav64_open_header32(f,"ST64");
    switch(s->type)
    {
	case MUXER_TYPE_VIDEO:
	    fwrite("vids",4,1,f);
	    break;
	case MUXER_TYPE_AUDIO:
	    fwrite("auds",4,1,f);
	    break;
	default:
	    fwrite("gens",4,1,f);
	    break;
    }
    mpxpav64_put64(f,privs->data_off);
    mpxpav64_put64(f,privs->idx_off);
    /* StreamProperties */
    mpxpav64_put64(f,privs->npackets);
    mpxpav64_put64(f,privs->nbytes);
    mpxpav64_put64(f,flags);
    mpxpav64_put_pts64(f,privs->last_pts);
    mpxpav64_put_pts32(f,privs->first_pts);
    mpxpav64_put32(f,privs->max_packet_size);
    mpxpav64_put32(f,privs->npackets?(privs->nbytes/privs->npackets):-1);
    mpxpav64_put32(f,privs->min_packet_size);
    mpxpav64_put_pts32(f,privs->max_frame_duration);
    mpxpav64_put_pts32(f,privs->npackets?((privs->last_pts-privs->first_pts)/(float)privs->npackets):-1);
    mpxpav64_put_pts32(f,privs->min_frame_duration);
    mpxpav64_put32(f,privs->max_brate);
    mpxpav64_put32(f,(privs->last_pts-privs->first_pts)?((float)privs->nbytes/(privs->last_pts-privs->first_pts)):-1);
    mpxpav64_put32(f,privs->min_brate);
    mpxpav64_put64(f,1000); /* PTS denimonator */
    mpxpav64_put64(f,1); /* SIZE numerator */
    switch(s->type)
    {
	case MUXER_TYPE_VIDEO:
	    flags=13;
	    mpxpav64_put8(f,flags);
	    fwrite("video/x-video",flags,1,f);
	    spos=mpxpav64_open_header32(f,"BIH ");
	    le2me_BITMAPINFOHEADER(s->bih);
	    fwrite(s->bih,s->bih->biSize,1,f);
	    le2me_BITMAPINFOHEADER(s->bih);
	    mpxpav64_close_header32(f,spos);
	    if(s->aspect)
	    {
		float dur;
		dur=privs->last_pts-privs->first_pts;
		memset(&vprp, 0, sizeof(vprp));
		vprp.dwVerticalRefreshRate = (s->h.dwRate+s->h.dwScale-1)/s->h.dwScale;
		vprp.dwHTotalInT = s->bih->biHeight*s->aspect;
		vprp.dwVTotalInLines = s->bih->biHeight;
		vprp.dwVerticalRefreshRate = dur?lrintf(privs->npackets/dur)*2:0;
		vprp.dwFrameAspectRatio = avi_aspect(s->aspect);
		vprp.dwFrameWidthInPixels = s->bih->biWidth;
		vprp.dwFrameHeightInLines = s->bih->biHeight;
		vprp.nbFieldPerFrame = 1; /* Depends Interlaced or Progressive frame */
		vprp.FieldInfo[0].CompressedBMHeight = s->bih->biHeight;
		vprp.FieldInfo[0].CompressedBMWidth = s->bih->biWidth;
		vprp.FieldInfo[0].ValidBMHeight = s->bih->biHeight;
		vprp.FieldInfo[0].ValidBMWidth = s->bih->biWidth;
		le2me_VideoPropHeader(&vprp);
		le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[0]);
		le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[1]);
		spos=mpxpav64_open_header32(f,"vprp");
		fwrite(&vprp,sizeof(VideoPropHeader),1,f);
		le2me_VideoPropHeader(&vprp);
		le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[0]);
		le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[1]);
		mpxpav64_close_header32(f,spos);
	    }
	    if(s->ImageDesc)
	    {
		ImageDescription id;
		spos=mpxpav64_open_header32(f,"IMGD");
		le2me_ImageDesc(&id);
		fwrite(&id,sizeof(ImageDescription),1,f);
		le2me_ImageDesc(&id);
		mpxpav64_close_header32(f,spos);
	    }
	    break;
	case MUXER_TYPE_AUDIO:
	    flags=13;
	    mpxpav64_put8(f,flags);
	    fwrite("audio/x-audio",flags,1,f);
	    spos=mpxpav64_open_header32(f,"WAVE");
	    le2me_WAVEFORMATEX(s->wf);
	    fwrite(s->wf,WFSIZE(s->wf),1,f);
	    le2me_WAVEFORMATEX(s->wf);
	    mpxpav64_close_header32(f,spos);
	    frcc=(uint8_t *)(&((sh_audio_t *)(s->source))->wtag);
	    if(isprint(frcc[0]) && isprint(frcc[1]) && isprint(frcc[2]) && isprint(frcc[3]))
	    {
		spos=mpxpav64_open_header32(f,"FRCC");
		mpxpav64_put32(f,((sh_audio_t *)(s->source))->wtag);
		mpxpav64_close_header32(f,spos);
	    }
	    break;
	default:
	    flags=17;
	    mpxpav64_put8(f,flags);
	    fwrite("unknown/x-unknown",flags,1,f);
	    break;
    }
    
    mpxpav64_close_header32(f,fpos);
}

static int pass=0;
static void mpxpav64_write_header(muxer_t *muxer,demuxer_t*dinfo)
{
    priv_mpxpav64_t *pmpxpav64=(priv_mpxpav64_t *)muxer->priv;
    uint64_t hpos,fpos,tmp,flags=MPXPAV64_FP_FCNT_UTF16;
    uint32_t max_bitrate=0;
    float pts;
    size_t i;
    FILE *f = muxer->file;
    if(!pass)	pmpxpav64->mainh=mpxpav64_open_header64(f,"MPXPAV64");
    else	fseeko(f,16,SEEK_CUR);
    hpos=mpxpav64_open_header64(f,"HEADER64");
    fpos=mpxpav64_open_header32(f,"FPRP");
    tmp=0;
    for(i=0;i<muxer->avih.dwStreams;i++) tmp+=((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->npackets;
    mpxpav64_put64(f,tmp);
    tmp=0;
    for(i=0;i<muxer->avih.dwStreams;i++) tmp+=((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->nbytes;
    mpxpav64_put64(f,tmp);
    mpxpav64_put64(f,flags);
    pts=0;
    for(i=0;i<muxer->avih.dwStreams;i++) if(pts < ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->last_pts) pts = ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->last_pts;
    mpxpav64_put_pts64(f,pts);
    pts=HUGE;
    for(i=0;i<muxer->avih.dwStreams;i++) if(pts > ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->first_pts) pts = ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->first_pts;
    mpxpav64_put_pts32(f,pts);
    for(i=0;i<muxer->avih.dwStreams;i++) if(max_bitrate < ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->max_brate) max_bitrate = ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->max_brate;
    mpxpav64_put32(f,max_bitrate);
    max_bitrate=0;
    for(i=0;i<muxer->avih.dwStreams;i++)
	max_bitrate += (((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->last_pts-((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->first_pts)?
			(float)((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->nbytes/
			(((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->last_pts-((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->first_pts):
			0;
    mpxpav64_put32(f,max_bitrate);
    mpxpav64_put16(f,muxer->avih.dwStreams);
    mpxpav64_close_header32(f,fpos);
    mpxpav64_put_fcnt(muxer,dinfo);
    for(i=0;i<muxer->avih.dwStreams;i++) mpxpav64_put_st64(muxer,muxer->streams[i]);
    mpxpav64_close_header64(f,hpos);
    if(!pass) pmpxpav64->datah=mpxpav64_open_header64(f,"AVDATA64");
    else	fseeko(f,16,SEEK_CUR);
    pass++;
}

static int is_first_packet=1;
static void mpxpav64_write_packet(muxer_stream_t *s,size_t len,unsigned int flags,float pts)
{
    priv_mpxpav64_t *pmpxpav64=(priv_mpxpav64_t *)s->muxer->priv;
    priv_mpxpav64_stream_t *privs=(priv_mpxpav64_stream_t *)s->priv;
    FILE *f = s->muxer->file;
    uint64_t off;
    float xpts;
    uint8_t xflg;
    int want_pts32,is_seek;
    uint8_t  seek[3]= {'S','E','E'}, Dx[2];
    /* make indexes */
    off=ftello(f);
    xflg=0;
    is_seek=0;
    if(!privs->data_off) privs->data_off=off;
    if(privs->first_pts==HUGE) privs->first_pts=pts; 
    privs->last_pts=pts;
    if(is_first_packet) { is_first_packet=0; is_seek=1; }
    if(s->muxer->def_v)
    {
	if(s==s->muxer->def_v && flags&AVIIF_KEYFRAME && pts-pmpxpav64->prev_seek >= SEEKPOINT_THRESHOLD)
	    is_seek=1;
    }
    else
#if 0
    if(s->muxer->def_a)
    {
	if(s==s->muxer->def_a && flags&AVIIF_KEYFRAME && pts-pmpxpav64->prev_seek >= SEEKPOINT_THRESHOLD)
	    is_seek=1;
    }
    else
#endif
    {
	if(flags&AVIIF_KEYFRAME && pts-pmpxpav64->prev_seek >= SEEKPOINT_THRESHOLD)
	    is_seek=1;
    }
    if(is_seek)
    {
	unsigned i;
	/* broadcast that keyframe has been happened */
	for(i=0;i<s->muxer->avih.dwStreams;i++)
	{
	    if(s->muxer->streams[i]!=s)
		((priv_mpxpav64_stream_t *)s->muxer->streams[i]->priv)->prev_pts=HUGE;
	}
	is_seek=1;
	pmpxpav64->prev_seek=pts;
	if(!privs->idx)	privs->idx=mp_malloc(sizeof(uint64_t));
	else		privs->idx=mp_realloc(privs->idx,(privs->idx_size+1)*sizeof(uint64_t));
	((uint64_t *)(privs->idx))[privs->idx_size]=off;
	privs->idx_size++;
	fwrite(seek,3,1,f);
    }
    want_pts32=0;
    if(privs->prev_pts==HUGE) want_pts32=1;
    else
    if(is_seek) want_pts32=1;

    if(len==privs->prev_len && !want_pts32)	xflg&=~0x40;
    else
    if(len<0x100U)				xflg|=0x40;
    else if(len<0x10000UL)			xflg|=0x41;
    else if(len<0x100000000ULL)			xflg|=0x42;
    else					xflg|=0x43;

    xflg|=0x20;
    if(!want_pts32 && pts>=privs->prev_pts) /* prevent the case of PTS discontinuities */
    {
	xpts=pts-privs->prev_pts;
	if(PTS2INT(xpts)==PTS2INT(privs->prev_xpts)) xflg&=~0x20;
    }
    else xpts=pts;

    if(PTS2INT(xpts)<0x100U && !want_pts32)		xflg|=0x00;
    else if(PTS2INT(xpts)<0x10000UL && !want_pts32)	xflg|=0x04;
    else if(PTS2INT(xpts)<0x100000000ULL)		xflg|=0x08;
    else						xflg|=0x0C;

    if(s->id == pmpxpav64->prev_id && !want_pts32)	xflg&=~0x80;
    else
    if(s->id < 0x100U)					xflg|=0x80;
    else						xflg|=0x90;

    Dx[0]=is_seek?'K':flags&AVIIF_KEYFRAME?'D':'d';
    Dx[1]=xflg;
    fwrite(Dx,2,1,f);
    if((xflg&0x80)==0x80)
    {
	if(s->id < 0x100U)	mpxpav64_put8(f,s->id);
	else			mpxpav64_put16(f,s->id);
    }
    if((xflg&0x40)==0x40)
    {
	if(len<0x100U)			mpxpav64_put8(f,len);
	else if(len<0x10000UL)		mpxpav64_put16(f,len);
	else if(len<0x100000000ULL)	mpxpav64_put32(f,len);
	else				mpxpav64_put64(f,len);
    }
    if((xflg&0x20)==0x20)
    {
	if(PTS2INT(xpts)<0x100U && !want_pts32)		mpxpav64_put_pts8(f,xpts);
	else if(PTS2INT(xpts)<0x10000UL && !want_pts32)	mpxpav64_put_pts16(f,xpts);
	else if(PTS2INT(xpts)<0x100000000ULL)		mpxpav64_put_pts32(f,xpts);
	else						mpxpav64_put_pts64(f,xpts);
    }
    fwrite(s->buffer,len,1,f);
    MSG_V("MUX_MPXPAV64: write %lu bytes of #%u %08X flags %f pts %f\n",len,s->id,flags,pts,xpts);
    /* update statistic */
    privs->npackets++;
    privs->nbytes+=len;
    if(privs->min_packet_size >= len) privs->min_packet_size=len;
    if(privs->max_packet_size <= len) privs->max_packet_size=len;
    xpts=pts-privs->prev_pts;
    if(privs->min_frame_duration >= xpts) privs->min_frame_duration=xpts;
    if(privs->max_frame_duration <= xpts) privs->max_frame_duration=xpts;
    xpts=(float)privs->prev_len/(pts-privs->prev_pts);
    if(privs->min_brate >= xpts) privs->min_brate=xpts;
    if(privs->max_brate <= xpts) privs->max_brate=xpts;
    /* accumulate statitistic */
    privs->prev_pts=pts;
    privs->prev_xpts=want_pts32?HUGE:xpts;
    privs->prev_len=len;
    pmpxpav64->prev_id=s->id;
}

static void mpxpav64_write_index_64(muxer_stream_t *s)
{
    priv_mpxpav64_stream_t *privs=(priv_mpxpav64_stream_t *)s->priv;
    uint64_t ioff;
    uint32_t i;
    FILE *f = s->muxer->file;
    if(!privs->idx_off) privs->idx_off=ftello(f);
    ioff=mpxpav64_open_header64(f,"INDEX_64");
    mpxpav64_put32(f,s->id);
    for(i=0;i<privs->idx_size;i++) mpxpav64_put64(f,((uint64_t *)privs->idx)[i]);
    mpxpav64_close_header64(f,ioff);
}

static void mpxpav64_write_index_32(muxer_stream_t *s)
{
    priv_mpxpav64_stream_t *privs=(priv_mpxpav64_stream_t *)s->priv;
    uint64_t ioff;
    uint32_t i;
    FILE *f = s->muxer->file;
    if(!privs->idx_off) privs->idx_off=ftello(f);
    ioff=mpxpav64_open_header32(f,"IX32");
    mpxpav64_put32(f,s->id);
    for(i=0;i<privs->idx_size;i++) mpxpav64_put32(f,((uint64_t *)privs->idx)[i]);
    mpxpav64_close_header32(f,ioff);
}

static void mpxpav64_write_index(muxer_t *muxer)
{
    off_t avdata64_size;
    priv_mpxpav64_t *pmpxpav64=(priv_mpxpav64_t *)muxer->priv;
    unsigned i;
    FILE *f = muxer->file;
    avdata64_size=ftello(f);
    mpxpav64_close_header64(f,pmpxpav64->datah);
    for(i=0;i<muxer->avih.dwStreams;i++)
    {
	if(((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->idx)
	{
	    if(avdata64_size<0x100000000ULL)
		mpxpav64_write_index_32(muxer->streams[i]);
	    else
		mpxpav64_write_index_64(muxer->streams[i]);
	    mp_free(((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->idx);
	    ((priv_mpxpav64_stream_t *)muxer->streams[i]->priv)->idx=NULL;
	}
    }
    mpxpav64_close_header64(f,pmpxpav64->mainh);
}

void muxer_init_muxer_mpxp64(muxer_t *muxer){
  muxer->cont_new_stream = &mpxpav64_new_stream;
  muxer->cont_write_chunk = &mpxpav64_write_packet;
  muxer->cont_write_header = &mpxpav64_write_header;
  muxer->cont_write_index = &mpxpav64_write_index;
}
