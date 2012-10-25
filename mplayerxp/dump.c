/*
    dump.c - stream dumper
*/

#include <stdio.h>
#include <stdlib.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>

#include "mp_config.h"

#include "dump.h"
#include "sig_hand.h"
#include "help_mp.h"
#include "input/input.h"
#include "mplayer.h"
#include "libmpdemux/muxer.h"
#include "libmpdemux/mrl.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "__mp_msg.h"

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

void dump_stream(stream_t *stream)
{
  unsigned char buf[4096];
  int len;
  FILE *f;
  const char *ext,*name;
  pinfo[xp_id].current_module="dumpstream";
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
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
  while(!stream_eof(stream)){
      len=stream_read(stream,buf,4096);
      if(len>0) fwrite(buf,len,1,f);
  }
  fclose(f);
  MSG_INFO(MSGTR_CoreDumped); /* nice joke ;) */
  exit_player(MSGTR_Exit_eof);
}

static int dump_inited=0;
static int my_use_pts;
static FILE *mux_file;
static muxer_t *muxer;
static muxer_stream_t *m_audio,*m_video,*m_subs;
static sh_audio_t *sh_audio=NULL;
static sh_video_t *sh_video=NULL;
static unsigned decoded_frameno=0;
static unsigned a_frameno=0;
#define MUX_HAVE_AUDIO 0x01
#define MUX_HAVE_VIDEO 0x02
#define MUX_HAVE_SUBS  0x04
static int mux_type;
static uint64_t vsize=0,asize=0,ssize=0;
static float timer_corr=0; /* use common time-base */

void __FASTCALL__ dump_stream_event_handler(struct stream_s *s,const stream_packet_t*sp)
{
}

/*
    returns: 0 - nothing interested
	    -1 - quit
*/
static int check_cmd(void)
{
  mp_cmd_t* cmd;
  int retval;
  retval = 0;
  while((cmd = mp_input_get_cmd(0,0,0)) != NULL)
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

void dump_mux_init(demuxer_t *demuxer)
{
  char stream_dump_name[1024];
  /* TODO copy it from demuxer */
  if(dump_inited) return;
  sh_audio=demuxer->audio->sh;
  sh_video=demuxer->video->sh;
  /* describe other useless dumps */
  mux_type=MUX_HAVE_AUDIO|MUX_HAVE_VIDEO|MUX_HAVE_SUBS;
  if(port)
  {
    if(strcmp(port,"audio") ==0 ) { strcpy(stream_dump_name,"a_"); mux_type&=~(MUX_HAVE_VIDEO|MUX_HAVE_SUBS); }
    else
    if(strcmp(port,"video") ==0 ) { strcpy(stream_dump_name,"v_"); mux_type&=~(MUX_HAVE_AUDIO|MUX_HAVE_SUBS); }
    else
    if(strcmp(port,"sub") ==0 ) { strcpy(stream_dump_name,"s_"); mux_type&=~(MUX_HAVE_AUDIO|MUX_HAVE_VIDEO); }
    else strcpy(stream_dump_name,port);
  }
  else strcpy(stream_dump_name,"avs_");
  if(strcmp(media,"lavf") == 0) { strcpy(stream_dump_name,"avs_dump."); strcat(stream_dump_name,port); }
  else
  if(strcmp(media,"mpxp") == 0) strcat(stream_dump_name,"dump.mpxp");
  else
  if(strcmp(media,"raw") == 0) strcat(stream_dump_name,"dump.raw");
  else
  {
    MSG_FATAL("Unsupported muxer format %s found\n",media);
    exit_player(MSGTR_Exit_error);
  }
  mux_file=fopen(stream_dump_name,"wb");
  MSG_DBG2("Preparing stream dumping: %s\n",stream_dump_name);
  if(!mux_file){
    MSG_FATAL(MSGTR_CantOpenDumpfile);
    exit_player(MSGTR_Exit_error);
  }
  if(!(muxer=muxer_new_muxer(media,port,mux_file)))
  {
    MSG_FATAL("Can't initialize muxer\n");
    exit_player(MSGTR_Exit_error);
  }
  if(sh_audio && (mux_type&MUX_HAVE_AUDIO))
  {
    m_audio=muxer_new_stream(muxer,MUXER_TYPE_AUDIO);
    m_audio->buffer_size=0x100000; //16384;
    m_audio->source=sh_audio;
    m_audio->codec=0;
    if(!sh_audio->wf)
    {
	sh_audio->wf=malloc(sizeof(WAVEFORMATEX));
	sh_audio->wf->nBlockAlign = 1; //mux_a->h.dwSampleSize;
	sh_audio->wf->wFormatTag = sh_audio->format;
	sh_audio->wf->nChannels = sh_audio->channels;
	sh_audio->wf->nSamplesPerSec = sh_audio->samplerate;
	sh_audio->wf->nAvgBytesPerSec=sh_audio->i_bps; //mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
	sh_audio->wf->wBitsPerSample = 16; // FIXME
	sh_audio->wf->cbSize=0; // FIXME for l3codeca.acm
    }
    m_audio->wf=malloc(sh_audio->wf->cbSize+sizeof(WAVEFORMATEX));
    memcpy(m_audio->wf,sh_audio->wf,sh_audio->wf->cbSize+sizeof(WAVEFORMATEX));
    if(!sh_audio->wf->cbSize && sh_audio->codecdata_len)
    {
	m_audio->wf->cbSize=sh_audio->wf->cbSize=sh_audio->codecdata_len;
	m_audio->wf=realloc(m_audio->wf,sh_audio->wf->cbSize+sizeof(WAVEFORMATEX));
	memcpy((char *)(m_audio->wf+1),sh_audio->codecdata,sh_audio->codecdata_len);
    }
    if(!sh_audio->i_bps) sh_audio->i_bps=m_audio->wf->nAvgBytesPerSec;
    if(sh_audio->audio.dwScale){
	m_audio->h.dwSampleSize=sh_audio->audio.dwSampleSize;
	m_audio->h.dwScale=sh_audio->audio.dwScale;
	m_audio->h.dwRate=sh_audio->audio.dwRate;
    } else {
	m_audio->h.dwSampleSize=m_audio->wf->nBlockAlign;
	m_audio->h.dwScale=m_audio->h.dwSampleSize;
	m_audio->h.dwRate=m_audio->wf->nAvgBytesPerSec;
    }
  }
  else m_audio=NULL;
  if(sh_video && (mux_type&MUX_HAVE_VIDEO))
  {
    m_video=muxer_new_stream(muxer,MUXER_TYPE_VIDEO);
    m_video->buffer_size=0x200000; // 2MB
    m_video->source=sh_video;
    m_video->h.dwSampleSize=0; // VBR
    m_video->h.dwScale=10000;
    m_video->h.dwRate=m_video->h.dwScale*sh_video->fps;
    m_video->h.dwSuggestedBufferSize=sh_video->video.dwSuggestedBufferSize;
    if(!sh_video->bih)
    {
	sh_video->bih=malloc(sizeof(BITMAPINFOHEADER));
	sh_video->bih->biSize=sizeof(BITMAPINFOHEADER);
	sh_video->bih->biWidth=sh_video->disp_w;
	sh_video->bih->biHeight=sh_video->disp_h;
	sh_video->bih->biCompression=sh_video->format;
	sh_video->bih->biPlanes=1;
	sh_video->bih->biBitCount=24; // FIXME!!!
	sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight*(sh_video->bih->biBitCount/8);
    }
    m_video->bih=malloc(sh_video->bih->biSize);
    memcpy(m_video->bih,sh_video->bih,sh_video->bih->biSize);
    m_video->ImageDesc=sh_video->ImageDesc;
    m_video->aspect=sh_video->aspect;
    m_video->codec=0;
  }
  else m_video=NULL;
  if(demuxer->sub->sh && (mux_type&MUX_HAVE_SUBS))
  {
    m_subs=muxer_new_stream(muxer,MUXER_TYPE_SUBS);
    m_subs->buffer_size=0x100000; //16384;
    m_subs->source=NULL;
    m_subs->codec=0;
  }
  else m_subs=NULL;
  MSG_DBG2("Opening dump: %X\n",demuxer);
  MSG_INFO("Dumping stream to %s\n",stream_dump_name);
  dump_inited=1;
  muxer_fix_parameters(muxer);
  muxer_write_header(muxer,demuxer);
}

void dump_mux_close(demuxer_t *demuxer)
{
  demux_stream_t *d_audio;
  demux_stream_t *d_video;
  if(dump_inited)
  {
    MSG_DBG2("Closing dump: %X %f secs\n"
             "As video %X-%ix%i audio %X-%ix%ix%i\n"
    ,demuxer,sh_video?sh_video->timer:sh_audio->timer
    ,m_video?m_video->bih->biCompression:-1,m_video?m_video->bih->biWidth:-1,m_video?m_video->bih->biHeight:-1
    ,m_audio?m_audio->wf->wFormatTag:-1,m_audio?m_audio->wf->nSamplesPerSec:-1,m_audio?m_audio->wf->wBitsPerSample:-1,m_audio?m_audio->wf->nChannels:-1);
    d_audio=demuxer->audio;
    d_video=demuxer->video;
    sh_audio=d_audio->sh;
    sh_video=d_video->sh;
    if(sh_video && (mux_type&MUX_HAVE_VIDEO)) m_video->source=sh_video;
    if(sh_audio && (mux_type&MUX_HAVE_AUDIO)) m_audio->source=sh_audio;
    muxer_write_index(muxer);
    /* fixup avi header */
    if(sh_video)
    {
	if(sh_video->timer) sh_video->fps=(float)decoded_frameno/(sh_video->timer+timer_corr);
	if(m_video)
	{
	    m_video->h.dwRate=m_video->h.dwScale*sh_video->fps;
	    m_video->h.dwSuggestedBufferSize=vsize/decoded_frameno;
	    MSG_V("Finishing vstream as: scale %u rate %u fps %f (frames=%u timer=%f)\n"
	    ,m_video->h.dwScale,m_video->h.dwRate,sh_video->fps,decoded_frameno,sh_video->timer+timer_corr);
	}
    }
    if(sh_audio)
    {
	if(m_audio)
	{
	    m_audio->h.dwSuggestedBufferSize=asize/a_frameno;
	    MSG_V("Finishing astream as: scale %u rate %u (frames=%u timer=%f) avg=%i size=%u\n"
	    ,m_audio->h.dwScale,m_audio->h.dwRate,a_frameno,sh_audio->timer+timer_corr
	    ,m_audio->wf->nAvgBytesPerSec,asize);
	}
    }
    if(demuxer->sub->sh)
    {
	if(m_subs)
	{
	    MSG_V("Finishing sstream as\n");
	}
    }
    fseeko(mux_file,0,SEEK_SET);
    muxer_write_header(muxer,demuxer);
    fclose(mux_file);
    dump_inited=0;
  }
  MSG_INFO(MSGTR_CoreDumped);
}


void dump_mux(demuxer_t *demuxer,int use_pts,const char *seek_to_sec,unsigned play_n_frames)
{
  demux_stream_t *d_audio=demuxer->audio;
  demux_stream_t *d_video=demuxer->video;
  demux_stream_t *d_sub=demuxer->sub;
  float frame_time,a_duration;
  float mpeg_vtimer=HUGE,mpeg_atimer=HUGE;
  unsigned char* start=NULL;
  int in_size,aeof,veof,seof,cmd;
  
  if(!dump_inited) return;
  pinfo[xp_id].current_module="dump";
  sh_audio=d_audio->sh;
  sh_video=d_video->sh;
  my_use_pts=use_pts;
  /* test stream property */
  MSG_INFO("%s using PTS method\n",use_pts?"":"not");
  if(m_video)
  {
    if(!sh_video) { MSG_ERR("Video not found!!!Skip this stream\n"); return; }
    if(!sh_video->bih) { MSG_ERR("Video property not found!!!Skip this stream\n"); return; }
    if(memcmp(sh_video->bih,m_video->bih,sizeof(BITMAPINFOHEADER))!=0)
    {
       MSG_ERR("Found different video properties(%X-%ix%i)!=(%X-%ix%i)!!!\nSkip this stream\n",
       sh_video->bih->biCompression,sh_video->bih->biWidth,sh_video->bih->biHeight,
       m_video->bih->biCompression,m_video->bih->biWidth,m_video->bih->biHeight);
       return;
    }
    m_video->source=sh_video;
  }
  if(m_audio)
  {
    if(!sh_audio) { MSG_ERR("Audio not found!!!Skip this stream\n"); return; }
    if(!sh_audio->wf) { MSG_ERR("Audio property not found!!!Skip this stream\n"); return; }
    if(memcmp(sh_audio->wf,m_audio->wf,sizeof(WAVEFORMATEX))!=0)
    {
       MSG_ERR("Found different audio properties(%X-%ix%ix%i)!=(%X-%ix%ix%i)X!!!\nSkip this stream\n",
       sh_audio->wf->wFormatTag,sh_audio->wf->nSamplesPerSec,sh_audio->wf->wBitsPerSample,sh_audio->wf->nChannels,
       m_audio->wf->wFormatTag,m_audio->wf->nSamplesPerSec,m_audio->wf->wBitsPerSample,m_audio->wf->nChannels);
       return;
    }
    m_audio->source=sh_audio;
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
  aeof=sh_audio?0:1;
  veof=sh_video?0:1;
  if(sh_video) sh_video->timer=0;
  if(sh_audio) sh_audio->timer=0;
  while(!(aeof && veof)){
    in_size=0;
    if(sh_audio && !aeof)
    {
      float a_pts;
      while(sh_audio->timer < (sh_video?sh_video->timer:HUGE) || !sh_video || veof) /* autolimitation of audio reading */
      {
	/* we should try to keep structure of audio packets here 
	   and don't generate badly interlaved stream.
	   The ideal case is:  type=read_packet(ANY_TYPE); put_packet(type);
	 */
	in_size=ds_get_packet_r(sh_audio->ds,&start,&a_pts);
	cmd = check_cmd();
	if(cmd == -1) goto done;
	else
	a_duration=(float)in_size/(float)(sh_audio->i_bps);
	if(mpeg_atimer==HUGE) mpeg_atimer=a_pts;
	else 
	{
	    if( mpeg_atimer-a_duration<a_pts) mpeg_atimer=a_pts;
	    else	mpeg_atimer+=a_duration;
	}
	if(use_pts) sh_audio->timer=a_pts;
	else	    sh_audio->timer=mpeg_atimer;
	MSG_V("Got audio frame: %f %u\n",a_pts,(!aeof)?a_frameno:-1);
	aeof=sh_audio->ds->eof;
	a_frameno++;
	if(aeof) break;
	if(m_audio)
	{
	    m_audio->buffer=start;
	    if(in_size>0)
	    {
		MSG_V("put audio: %f %f %u\n",a_pts,sh_audio->timer+timer_corr,in_size);
		if(m_audio) muxer_write_chunk(m_audio,in_size,m_video?0:AVIIF_KEYFRAME,sh_audio->timer+timer_corr);
	    }
	    if(!m_audio->h.dwSampleSize && m_audio->timer>0)
		m_audio->wf->nAvgBytesPerSec=0.5f+(double)m_audio->size/m_audio->timer;
	}
	asize += in_size;
      }
    }
    if(sh_video && !veof)
    { 
	float v_pts;
	in_size=video_read_frame(sh_video,&frame_time,&v_pts,&start,0);
	cmd = check_cmd();
	if(cmd == -1) goto done;
	else
	if(mpeg_vtimer==HUGE) mpeg_vtimer=v_pts;
	else 
	{
	    if( mpeg_vtimer-frame_time<v_pts ) mpeg_vtimer=v_pts;
	    else	mpeg_vtimer+=frame_time;
	}
	if(use_pts) sh_video->timer=v_pts;
	else	    sh_video->timer=mpeg_vtimer;
	++decoded_frameno;
	veof=sh_video->ds->eof;
	MSG_V("Got video frame %f %i\n",v_pts,(!veof)?decoded_frameno:-1);
	if(m_video) m_video->buffer=start;
	if(in_size>0)
	{
	    MSG_V("put video: %f %f %u flg=%u\n",v_pts,sh_video->timer+timer_corr,in_size,sh_video->ds->flags);
	    if(m_video) muxer_write_chunk(m_video,in_size,(sh_video->ds->flags&1)?AVIIF_KEYFRAME:0,sh_video->timer+timer_corr);
	    vsize += in_size;
	}
	if(!(decoded_frameno%100))
	    MSG_STATUS("Done %u frames\r",decoded_frameno);
    }
    if(demuxer->sub->sh)
    {
	float s_pts=0;
	while(s_pts < (sh_video?sh_video->timer:HUGE) || !sh_video || veof) /* autolimitation of sub reading */
	{
	    in_size=ds_get_packet_r(demuxer->sub,&start,&s_pts);
	    seof=demuxer->sub->eof;
	    if(seof) break;
	    cmd = check_cmd();
	    if(cmd == -1) goto done;
	    else
	    MSG_V("Got sub frame: %f\n",s_pts);
	    if(m_subs)
	    {
		m_subs->buffer=start;
		if(in_size>0)
		{
		    MSG_V("put subs: %f %u\n",s_pts,in_size);
		    if(m_subs) muxer_write_chunk(m_subs,in_size,m_video?0:AVIIF_KEYFRAME,s_pts);
		}
	    }
	}
    }
    if(decoded_frameno>play_n_frames) break;
  }
  done:
  if(sh_video) timer_corr+=sh_video->timer+frame_time;
  else
  {
    if(sh_audio) timer_corr+=d_audio->pts;
    if(m_audio->wf->nAvgBytesPerSec)
	    timer_corr+=((float)ds_tell_pts(d_audio))/((float)m_audio->wf->nAvgBytesPerSec);
  }
  MSG_STATUS("Done %u frames (video(%X-%ix%i): %llu bytes audio(%X-%ix%ix%i): %llu bytes)\n"
  ,decoded_frameno
  ,m_video?m_video->bih->biCompression:-1,m_video?m_video->bih->biWidth:-1,m_video?m_video->bih->biHeight:-1
  ,vsize
  ,m_audio?m_audio->wf->wFormatTag:-1,m_audio?m_audio->wf->nSamplesPerSec:-1,m_audio?m_audio->wf->wBitsPerSample:-1,m_audio?m_audio->wf->nChannels:-1
  ,asize);
}
