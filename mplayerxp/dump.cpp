#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    dump.c - stream dumper
*/

#include <stdio.h>
#include <stdlib.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>

#include "xmpcore/sig_hand.h"
#include "mpxp_help.h"
#include "input2/input.h"
#include "mplayerxp.h"
#include "libmpdemux/muxer.h"
#include "libmpstream2/stream.h"
#include "libmpstream2/mrl.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "mpxp_msg.h"
#include "dump.h"

namespace mpxp {

static char *media=NULL,*port=NULL;

/* example: -dump @avi:my_file.avi#rate=50 */
int  dump_parse(const char *param)
{
    int type=0;
    const char *tile;
    tile=mrl_parse_line(param,NULL,NULL,&media,&port);
    if(!media) return 0;
    if(strcmp(media,"stream")==0)	type=1;
    else				type=2;
    return type;
}

void dump_stream(Stream *stream)
{
  char buf[4096];
  int len;
  FILE *f;
  const char *ext,*name;
  MP_UNIT("dumpstream");
  stream->reset();
  stream->seek(stream->start_pos());
  ext=".ext";
  if(!port)
  {
    strcpy(buf,"stream_dump");
    strcat(buf,ext);
  }
  else
  {
    strcpy(buf,port);
  }
  name=buf;
  f=fopen(name,"wb");
  if(!f){
    MSG_FATAL(MSGTR_CantOpenDumpfile);
    exit_player(MSGTR_Exit_error);
  }
  MSG_INFO("Dumping stream to %s\n",name);
  while(!stream->eof()){
      len=stream->read(buf,4096);
      if(len>0) fwrite(buf,len,1,f);
  }
  fclose(f);
  MSG_INFO(MSGTR_CoreDumped); /* nice joke ;) */
  exit_player(MSGTR_Exit_eof);
}

#define MUX_HAVE_AUDIO 0x01
#define MUX_HAVE_VIDEO 0x02
#define MUX_HAVE_SUBS  0x04
struct dump_priv_t : public Opaque {
    public:
	dump_priv_t(libinput_t& l):libinput(l) {}
	virtual ~dump_priv_t() {}

	int		my_use_pts;
	FILE*		mux_file;
	muxer_t*	muxer;
	muxer_stream_t *m_audio,*m_video,*m_subs;
	unsigned	decoded_frameno;
	unsigned	a_frameno;
	int		mux_type;
	uint64_t	vsize,asize,ssize;
	float		timer_corr; /* use common time-base */
	float		vtimer;
	libinput_t&	libinput;
};

/*
    returns: 0 - nothing interested
	    -1 - quit
*/
static int check_cmd(dump_priv_t* priv)
{
  mp_cmd_t* cmd;
  int retval;
  retval = 0;
  while((cmd = mp_input_get_cmd(priv->libinput,0,0,0)) != NULL)
  {
    switch(cmd->id)
    {
	case MP_CMD_QUIT:
	case MP_CMD_SOFT_QUIT:
			retval = -1;
			break;
	default:	break;
    }
  }
  return retval;
}

void dump_mux_init(Demuxer *demuxer,libinput_t& libinput)
{
    sh_audio_t* sha=reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
    sh_video_t* shv=reinterpret_cast<sh_video_t*>(demuxer->video->sh);
    char stream_dump_name[1024];
    /* TODO copy it from demuxer */
    if(demuxer->priv) return;
    dump_priv_t*priv=new(zeromem) dump_priv_t(libinput);
    demuxer->priv=priv;
    /* describe other useless dumps */
    priv->mux_type=MUX_HAVE_AUDIO|MUX_HAVE_VIDEO|MUX_HAVE_SUBS;
    if(port) {
	if(strcmp(port,"audio") ==0 ) { strcpy(stream_dump_name,"a_"); priv->mux_type&=~(MUX_HAVE_VIDEO|MUX_HAVE_SUBS); }
	else if(strcmp(port,"video") ==0 ) { strcpy(stream_dump_name,"v_"); priv->mux_type&=~(MUX_HAVE_AUDIO|MUX_HAVE_SUBS); }
	else if(strcmp(port,"sub") ==0 ) { strcpy(stream_dump_name,"s_"); priv->mux_type&=~(MUX_HAVE_AUDIO|MUX_HAVE_VIDEO); }
	else strcpy(stream_dump_name,port);
    } else strcpy(stream_dump_name,"avs_");
    if(strcmp(media,"lavf") == 0) { strcpy(stream_dump_name,"avs_dump."); strcat(stream_dump_name,port); }
    else if(strcmp(media,"mpxp") == 0) strcat(stream_dump_name,"dump.mpxp");
    else if(strcmp(media,"raw") == 0) strcat(stream_dump_name,"dump.raw");
    else {
	MSG_FATAL("Unsupported muxer format %s found\n",media);
	exit_player(MSGTR_Exit_error);
    }
    priv->mux_file=fopen(stream_dump_name,"wb");
    MSG_DBG2("Preparing stream dumping: %s\n",stream_dump_name);
    if(!priv->mux_file){
	MSG_FATAL(MSGTR_CantOpenDumpfile);
	exit_player(MSGTR_Exit_error);
    }
    if(!(priv->muxer=muxer_new_muxer(media,port,priv->mux_file))) {
	MSG_FATAL("Can't initialize muxer\n");
	exit_player(MSGTR_Exit_error);
    }
    if(sha && (priv->mux_type&MUX_HAVE_AUDIO)) {
	priv->m_audio=muxer_new_stream(priv->muxer,MUXER_TYPE_AUDIO);
	priv->m_audio->buffer_size=0x100000; //16384;
	priv->m_audio->source=sha;
	priv->m_audio->codec=0;
	if(!sha->wf) {
	    sha->wf=(WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
	    sha->wf->nBlockAlign = 1; //mux_a->h.dwSampleSize;
	    sha->wf->wFormatTag = sha->wtag;
	    sha->wf->nChannels = sha->nch;
	    sha->wf->nSamplesPerSec = sha->rate;
	    sha->wf->nAvgBytesPerSec=sha->i_bps; //mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
	    sha->wf->wBitsPerSample = 16; // FIXME
	    sha->wf->cbSize=0; // FIXME for l3codeca.acm
	}
	priv->m_audio->wf=(WAVEFORMATEX*)mp_malloc(sha->wf->cbSize+sizeof(WAVEFORMATEX));
	memcpy(priv->m_audio->wf,sha->wf,sha->wf->cbSize+sizeof(WAVEFORMATEX));
	if(!sha->wf->cbSize && sha->codecdata_len) {
	    priv->m_audio->wf->cbSize=sha->wf->cbSize=sha->codecdata_len;
	    priv->m_audio->wf=(WAVEFORMATEX*)mp_realloc(priv->m_audio->wf,sha->wf->cbSize+sizeof(WAVEFORMATEX));
	    memcpy((char *)(priv->m_audio->wf+1),sha->codecdata,sha->codecdata_len);
	}
	if(!sha->i_bps) sha->i_bps=priv->m_audio->wf->nAvgBytesPerSec;
	if(sha->audio.dwScale){
	    priv->m_audio->h.dwSampleSize=sha->audio.dwSampleSize;
	    priv->m_audio->h.dwScale=sha->audio.dwScale;
	    priv->m_audio->h.dwRate=sha->audio.dwRate;
	} else {
	    priv->m_audio->h.dwSampleSize=priv->m_audio->wf->nBlockAlign;
	    priv->m_audio->h.dwScale=priv->m_audio->h.dwSampleSize;
	    priv->m_audio->h.dwRate=priv->m_audio->wf->nAvgBytesPerSec;
	}
    } else priv->m_audio=NULL;
    if(shv && (priv->mux_type&MUX_HAVE_VIDEO)) {
	priv->m_video=muxer_new_stream(priv->muxer,MUXER_TYPE_VIDEO);
	priv->m_video->buffer_size=0x200000; // 2MB
	priv->m_video->source=shv;
	priv->m_video->h.dwSampleSize=0; // VBR
	priv->m_video->h.dwScale=10000;
	priv->m_video->h.dwRate=priv->m_video->h.dwScale*shv->fps;
	priv->m_video->h.dwSuggestedBufferSize=shv->video.dwSuggestedBufferSize;
	if(!shv->bih) {
	    shv->bih=(BITMAPINFOHEADER*)mp_malloc(sizeof(BITMAPINFOHEADER));
	    shv->bih->biSize=sizeof(BITMAPINFOHEADER);
	    shv->bih->biWidth=shv->src_w;
	    shv->bih->biHeight=shv->src_h;
	    shv->bih->biCompression=shv->fourcc;
	    shv->bih->biPlanes=1;
	    shv->bih->biBitCount=24; // FIXME!!!
	    shv->bih->biSizeImage=shv->bih->biWidth*shv->bih->biHeight*(shv->bih->biBitCount/8);
	}
	priv->m_video->bih=(BITMAPINFOHEADER*)mp_malloc(shv->bih->biSize);
	memcpy(priv->m_video->bih,shv->bih,shv->bih->biSize);
	priv->m_video->ImageDesc=shv->ImageDesc;
	priv->m_video->aspect=shv->aspect;
	priv->m_video->codec=0;
    } else priv->m_video=NULL;
    if(demuxer->sub->sh && (priv->mux_type&MUX_HAVE_SUBS)) {
	priv->m_subs=muxer_new_stream(priv->muxer,MUXER_TYPE_SUBS);
	priv->m_subs->buffer_size=0x100000; //16384;
	priv->m_subs->source=NULL;
	priv->m_subs->codec=0;
    } else priv->m_subs=NULL;
    MSG_DBG2("Opening dump: %X\n",demuxer);
    MSG_INFO("Dumping stream to %s\n",stream_dump_name);
    muxer_fix_parameters(priv->muxer);
    muxer_write_header(priv->muxer,demuxer);
}

void dump_mux_close(Demuxer *demuxer)
{
    dump_priv_t* priv=static_cast<dump_priv_t*>(demuxer->priv);
    Demuxer_Stream *d_audio=demuxer->audio;
    Demuxer_Stream *d_video=demuxer->video;
    sh_audio_t* sha=reinterpret_cast<sh_audio_t*>(d_audio->sh);
    sh_video_t* shv=reinterpret_cast<sh_video_t*>(d_video->sh);
    if(priv) {
	MSG_DBG2("Closing dump: %X %f secs\n"
	     "As video %X-%ix%i audio %X-%ix%ix%i\n"
	,demuxer,shv?priv->vtimer:sha->timer
	,priv->m_video?priv->m_video->bih->biCompression:-1
	,priv->m_video?priv->m_video->bih->biWidth:-1
	,priv->m_video?priv->m_video->bih->biHeight:-1
	,priv->m_audio?priv->m_audio->wf->wFormatTag:-1
	,priv->m_audio?priv->m_audio->wf->nSamplesPerSec:-1
	,priv->m_audio?priv->m_audio->wf->wBitsPerSample:-1
	,priv->m_audio?priv->m_audio->wf->nChannels:-1);
	if(shv && (priv->mux_type&MUX_HAVE_VIDEO)) priv->m_video->source=shv;
	if(sha && (priv->mux_type&MUX_HAVE_AUDIO)) priv->m_audio->source=sha;
	muxer_write_index(priv->muxer);
	/* fixup avi header */
	if(shv) {
	    if(priv->vtimer) shv->fps=(float)priv->decoded_frameno/(priv->vtimer+priv->timer_corr);
	    if(priv->m_video) {
		priv->m_video->h.dwRate=priv->m_video->h.dwScale*shv->fps;
		priv->m_video->h.dwSuggestedBufferSize=priv->vsize/priv->decoded_frameno;
		MSG_V("Finishing vstream as: scale %u rate %u fps %f (frames=%u timer=%f)\n"
		,priv->m_video->h.dwScale
		,priv->m_video->h.dwRate
		,shv->fps
		,priv->decoded_frameno
		,priv->vtimer+priv->timer_corr);
	    }
	}
	if(sha) {
	    if(priv->m_audio) {
		priv->m_audio->h.dwSuggestedBufferSize=priv->asize/priv->a_frameno;
		MSG_V("Finishing astream as: scale %u rate %u (frames=%u timer=%f) avg=%i size=%u\n"
		,priv->m_audio->h.dwScale
		,priv->m_audio->h.dwRate
		,priv->a_frameno
		,sha->timer+priv->timer_corr
		,priv->m_audio->wf->nAvgBytesPerSec
		,priv->asize);
	    }
	}
	if(demuxer->sub->sh) {
	    if(priv->m_subs) MSG_V("Finishing sstream as\n");
	}
	fseeko(priv->mux_file,0,SEEK_SET);
	muxer_write_header(priv->muxer,demuxer);
	fclose(priv->mux_file);
	delete demuxer->priv;
	demuxer->priv=NULL;
    }
    MSG_INFO(MSGTR_CoreDumped);
}


void dump_mux(Demuxer *demuxer,int use_pts,const char *seek_to_sec,unsigned play_n_frames)
{
    dump_priv_t* priv=static_cast<dump_priv_t*>(demuxer->priv);
    Demuxer_Stream *d_audio=demuxer->audio;
    Demuxer_Stream *d_video=demuxer->video;
    sh_audio_t* sha=reinterpret_cast<sh_audio_t*>(d_audio->sh);
    sh_video_t* shv=reinterpret_cast<sh_video_t*>(d_video->sh);
  float frame_time,a_duration;
  float mpeg_vtimer=HUGE,mpeg_atimer=HUGE;
  unsigned char* start=NULL;
  int in_size,aeof,veof,seof,cmd;

  if(!priv) return;
  MP_UNIT("dump");
  priv->my_use_pts=use_pts;
  /* test stream property */
  MSG_INFO("%s using PTS method\n",use_pts?"":"not");
  if(priv->m_video)
  {
    if(!shv) { MSG_ERR("Video not found!!!Skip this stream\n"); return; }
    if(!shv->bih) { MSG_ERR("Video property not found!!!Skip this stream\n"); return; }
    if(memcmp(shv->bih,priv->m_video->bih,sizeof(BITMAPINFOHEADER))!=0)
    {
       MSG_ERR("Found different video properties(%X-%ix%i)!=(%X-%ix%i)!!!\nSkip this stream\n",
       shv->bih->biCompression,shv->bih->biWidth,shv->bih->biHeight,
       priv->m_video->bih->biCompression,priv->m_video->bih->biWidth,
       priv->m_video->bih->biHeight);
       return;
    }
    priv->m_video->source=shv;
  }
  if(priv->m_audio)
  {
    if(!sha) { MSG_ERR("Audio not found!!!Skip this stream\n"); return; }
    if(!sha->wf) { MSG_ERR("Audio property not found!!!Skip this stream\n"); return; }
    if(memcmp(sha->wf,priv->m_audio->wf,sizeof(WAVEFORMATEX))!=0)
    {
       MSG_ERR("Found different audio properties(%X-%ix%ix%i)!=(%X-%ix%ix%i)X!!!\nSkip this stream\n",
       sha->wf->wFormatTag,sha->wf->nSamplesPerSec,sha->wf->wBitsPerSample,sha->wf->nChannels,
       priv->m_audio->wf->wFormatTag,priv->m_audio->wf->nSamplesPerSec,
       priv->m_audio->wf->wBitsPerSample,priv->m_audio->wf->nChannels);
       return;
    }
    priv->m_audio->source=sha;
  }
  if (seek_to_sec) {
    float d;
    float rel_seek_secs=0;
    seek_args_t seek_p = { 0, 1};
    int a,b;
    if (sscanf(seek_to_sec, "%d:%d:%f", &a,&b,&d)==3)
	rel_seek_secs += 3600*a +60*b +d ;
    else if (sscanf(seek_to_sec, "%d:%f", &a, &d)==2)
	rel_seek_secs += 60*a +d;
    else if (sscanf(seek_to_sec, "%f", &d)==1)
	rel_seek_secs += d;

    seek_to_sec = NULL;
    MSG_INFO("seeking to %u seconds\n");
    seek_p.secs=rel_seek_secs;
    demux_seek_r(demuxer,&seek_p);
  }
  aeof=sha?0:1;
  veof=shv?0:1;
  if(shv) priv->vtimer=0;
  if(sha) sha->timer=0;
  while(!(aeof && veof)){
    in_size=0;
    if(sha && !aeof)
    {
      float a_pts;
      while(sha->timer < (shv?priv->vtimer:HUGE) || !shv || veof) /* autolimitation of audio reading */
      {
	/* we should try to keep structure of audio packets here
	   and don't generate badly interlaved stream.
	   The ideal case is:  type=read_packet(ANY_TYPE); put_packet(type);
	 */
	in_size=ds_get_packet_r(sha->ds,&start,&a_pts);
	cmd = check_cmd(priv);
	if(cmd == -1) goto done;
	else
	a_duration=(float)in_size/(float)(sha->i_bps);
	if(mpeg_atimer==HUGE) mpeg_atimer=a_pts;
	else
	{
	    if( mpeg_atimer-a_duration<a_pts) mpeg_atimer=a_pts;
	    else	mpeg_atimer+=a_duration;
	}
	if(use_pts) sha->timer=a_pts;
	else	    sha->timer=mpeg_atimer;
	MSG_V("Got audio frame: %f %u\n",a_pts,(!aeof)?priv->a_frameno:-1);
	aeof=sha->ds->eof;
	priv->a_frameno++;
	if(aeof) break;
	if(priv->m_audio)
	{
	    priv->m_audio->buffer=start;
	    if(in_size>0)
	    {
		MSG_V("put audio: %f %f %u\n",a_pts,sha->timer+priv->timer_corr,in_size);
		if(priv->m_audio)
		    muxer_write_chunk(priv->m_audio,in_size,priv->m_video?0:AVIIF_KEYFRAME,sha->timer+priv->timer_corr);
	    }
	    if(!priv->m_audio->h.dwSampleSize && priv->m_audio->timer>0)
		priv->m_audio->wf->nAvgBytesPerSec=0.5f+(double)priv->m_audio->size/priv->m_audio->timer;
	}
	priv->asize += in_size;
      }
    }
    if(shv && !veof)
    {
	float v_pts;
	in_size=shv->read_frame(&frame_time,&v_pts,&start,0);
	cmd = check_cmd(priv);
	if(cmd == -1) goto done;
	else
	if(mpeg_vtimer==HUGE) mpeg_vtimer=v_pts;
	else
	{
	    if( mpeg_vtimer-frame_time<v_pts ) mpeg_vtimer=v_pts;
	    else	mpeg_vtimer+=frame_time;
	}
	if(use_pts) priv->vtimer=v_pts;
	else	    priv->vtimer=mpeg_vtimer;
	++priv->decoded_frameno;
	veof=shv->ds->eof;
	MSG_V("Got video frame %f %i\n",v_pts,(!veof)?priv->decoded_frameno:-1);
	if(priv->m_video) priv->m_video->buffer=start;
	if(in_size>0)
	{
	    MSG_V("put video: %f %f %u flg=%u\n",v_pts,priv->vtimer+priv->timer_corr,in_size,shv->ds->flags);
	    if(priv->m_video) muxer_write_chunk(priv->m_video,in_size,(shv->ds->flags&1)?AVIIF_KEYFRAME:0,priv->vtimer+priv->timer_corr);
	    priv->vsize += in_size;
	}
	if(!(priv->decoded_frameno%100))
	    MSG_STATUS("Done %u frames\r",priv->decoded_frameno);
    }
    if(demuxer->sub->sh)
    {
	float s_pts=0;
	while(s_pts < (shv?priv->vtimer:HUGE) || !shv || veof) /* autolimitation of sub reading */
	{
	    in_size=ds_get_packet_r(demuxer->sub,&start,&s_pts);
	    seof=demuxer->sub->eof;
	    if(seof) break;
	    cmd = check_cmd(priv);
	    if(cmd == -1) goto done;
	    else
	    MSG_V("Got sub frame: %f\n",s_pts);
	    if(priv->m_subs)
	    {
		priv->m_subs->buffer=start;
		if(in_size>0)
		{
		    MSG_V("put subs: %f %u\n",s_pts,in_size);
		    if(priv->m_subs)
			muxer_write_chunk(priv->m_subs,in_size,priv->m_video?0:AVIIF_KEYFRAME,s_pts);
		}
	    }
	}
    }
    if(priv->decoded_frameno>play_n_frames) break;
  }
  done:
  if(shv) priv->timer_corr+=priv->vtimer+frame_time;
  else
  {
    if(sha) priv->timer_corr+=d_audio->pts;
    if(priv->m_audio->wf->nAvgBytesPerSec)
	    priv->timer_corr+=((float)d_audio->tell_pts())/((float)priv->m_audio->wf->nAvgBytesPerSec);
  }
  MSG_STATUS("Done %u frames (video(%X-%ix%i): %llu bytes audio(%X-%ix%ix%i): %llu bytes)\n"
  ,priv->decoded_frameno
  ,priv->m_video?priv->m_video->bih->biCompression:-1
  ,priv->m_video?priv->m_video->bih->biWidth:-1
  ,priv->m_video?priv->m_video->bih->biHeight:-1
  ,priv->vsize
  ,priv->m_audio?priv->m_audio->wf->wFormatTag:-1
  ,priv->m_audio?priv->m_audio->wf->nSamplesPerSec:-1
  ,priv->m_audio?priv->m_audio->wf->wBitsPerSample:-1
  ,priv->m_audio?priv->m_audio->wf->nChannels:-1
  ,priv->asize);
}

} // namespace mpxp
