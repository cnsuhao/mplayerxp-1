#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include <limits.h>

#include "mpxp_help.h"
#include "osdep/bswap.h"
#include "aviheader.h"

#include "muxer.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "libavformat/avformat.h"
#include "libavformat/riff.h"
#include "libmpcodecs/codecs_ld.h"
#include "demux_msg.h"

extern unsigned int codec_get_wav_tag(int id);

typedef struct {
	//AVInputFormat *avif;
	AVFormatContext *oc;
	ByteIOContext *pb;
	int audio_streams;
	int video_streams;
	int64_t last_pts;
} muxer_priv_t;

typedef struct {
	int64_t last_pts;
	AVStream *avstream;
} muxer_stream_priv_t;

static void mpxp_free(any_t*ptr)
{
    /* XXX: this test should not be needed on most libcs */
    if (ptr)
#ifdef MEMALIGN_HACK
	delete ptr - ((char*)ptr)[-1];
#else
	delete ptr;
#endif
}

static void mpxp_freep(any_t*arg)
{
    any_t**ptr= (any_t**)arg;
    mpxp_free(*ptr);
    *ptr = NULL;
}

static int mux_rate= 0;
static int mux_packet_size= 0;
static float mux_preload= 0.5;
static float mux_max_delay= 0.7;

static int mpxp_open(URLContext *h, const char *filename, int flags)
{
	return 0;
}

static int mpxp_close(URLContext *h)
{
	return 0;
}


static int mpxp_read(URLContext *h, unsigned char *buf, int size)
{
	fprintf(stderr, "READ %d\n", size);
	return -1;
}

static int mpxp_write(URLContext *h, unsigned char *buf, int size)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	return fwrite(buf, 1, size, muxer->file);
}

static int64_t mpxp_seek(URLContext *h, int64_t pos, int whence)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	fprintf(stderr, "SEEK %"PRIu64"\n", (int64_t)pos);
	return fseeko(muxer->file, pos, whence);
}


static URLProtocol mp_protocol = {
	"mpxp",
	mpxp_open,
	mpxp_read,
	mpxp_write,
	mpxp_seek,
	mpxp_close,
	NULL, /*  struct URLProtocol *next; */
	NULL, /* int (*url_read_play)(URLContext *h); */
	NULL, /* int (*url_read_pause)(URLContext *h); */
	NULL  /* int (*url_read_seek)(URLContext *h,
			 int stream_index, int64_t timestamp, int flags); */
};

static muxer_stream_t* lavf_new_stream(muxer_t *muxer, int type)
{
	muxer_priv_t *priv = (muxer_priv_t*) muxer->priv;
	muxer_stream_t *stream;
	muxer_stream_priv_t *spriv;
	AVCodecContext *ctx;

	if(!muxer || (type != MUXER_TYPE_VIDEO && type != MUXER_TYPE_AUDIO && type != MUXER_TYPE_SUBS))
	{
		MSG_ERR("UNKNOW TYPE %d\n", type);
		return NULL;
	}

	stream = (muxer_stream_t*) mp_calloc(1, sizeof(muxer_stream_t));
	if(!stream)
	{
		MSG_ERR("Could not alloc muxer_stream, EXIT\n");
		return NULL;
	}
	muxer->streams[muxer->avih.dwStreams] = stream;
	spriv = (muxer_stream_priv_t*) mp_calloc(1, sizeof(muxer_stream_priv_t));
	if(!spriv)
	{
		delete stream;
		return NULL;
	}
	stream->priv = spriv;

	spriv->avstream = av_new_stream(priv->oc, 1);
	if(!spriv->avstream)
	{
		MSG_ERR("Could not alloc avstream, EXIT\n");
		return NULL;
	}
	spriv->avstream->stream_copy = 1;

#if LIBAVFORMAT_BUILD >= 4629
	ctx = spriv->avstream->codec;
#else
	ctx = &(spriv->avstream->codec);
#endif
	ctx->codec_id = muxer->avih.dwStreams;
	switch(type)
	{
		case MUXER_TYPE_VIDEO:
			ctx->codec_type = CODEC_TYPE_VIDEO;
			break;
		case MUXER_TYPE_AUDIO:
			ctx->codec_type = CODEC_TYPE_AUDIO;
			break;
	}

	muxer->avih.dwStreams++;
	stream->muxer = muxer;
	stream->type = type;
	MSG_V("ALLOCATED STREAM N. %d, type=%d\n", muxer->avih.dwStreams, type);
	return stream;
}

static void fix_parameters(struct muxer_t *muxer)
{
    unsigned i;
    for(i=0;i<muxer->avih.dwStreams;i++)
    {
	muxer_stream_t *stream=muxer->streams[i];
	muxer_stream_priv_t *spriv = stream->priv;
	AVCodecContext *ctx;

#if LIBAVFORMAT_BUILD >= 4629
	ctx = spriv->avstream->codec;
#else
	ctx = &(spriv->avstream->codec);
#endif

	if(stream->wf && stream->wf->nAvgBytesPerSec)
	    ctx->bit_rate = stream->wf->nAvgBytesPerSec * 8;
	ctx->rc_buffer_size=0;
	ctx->rc_max_rate=0;

	if(stream->type == MUXER_TYPE_AUDIO)
	{
		ctx->codec_id = av_codec_get_id(ff_codec_wav_tags,stream->wf->wFormatTag);
#if 0 //breaks aac in mov at least
		ctx->codec_tag = codec_get_wav_tag(ctx->codec_id);
#endif
		MSG_INFO("AUDIO CODEC ID: %x, TAG: %x\n", ctx->codec_id, (uint32_t) ctx->codec_tag);
		ctx->sample_rate = stream->wf->nSamplesPerSec;
//                mp_msg(MSGT_MUXER, MSGL_INFO, "stream->h.dwSampleSize: %d\n", stream->h.dwSampleSize);
		ctx->channels = stream->wf->nChannels;
		if(stream->h.dwRate && (stream->h.dwScale * (int64_t)ctx->sample_rate) % stream->h.dwRate == 0)
		    ctx->frame_size= (stream->h.dwScale * (int64_t)ctx->sample_rate) / stream->h.dwRate;
		MSG_V("MUXER_LAVF(audio stream) frame_size: %d, scale: %u, sps: %u, rate: %u, ctx->block_align = stream->wf->nBlockAlign; %d=%d stream->wf->nAvgBytesPerSec:%d\n",
			ctx->frame_size, stream->h.dwScale, ctx->sample_rate, stream->h.dwRate,
			ctx->block_align, stream->wf->nBlockAlign, stream->wf->nAvgBytesPerSec);
		ctx->block_align = stream->h.dwSampleSize;
		if(stream->wf+1 && stream->wf->cbSize)
		{
			ctx->extradata = mp_malloc(stream->wf->cbSize);
			if(ctx->extradata != NULL)
			{
				ctx->extradata_size = stream->wf->cbSize;
				memcpy(ctx->extradata, stream->wf+1, ctx->extradata_size);
			}
			else
				MSG_ERR("MUXER_LAVF(audio stream) error! couldn't allocate %d bytes for extradata\n",
					stream->wf->cbSize);
		}
	}
	else if(stream->type == MUXER_TYPE_VIDEO)
	{
		ctx->codec_id = av_codec_get_id(ff_codec_bmp_tags,stream->bih->biCompression);
		if(ctx->codec_id <= 0)
		    ctx->codec_tag= stream->bih->biCompression;
		MSG_INFO("VIDEO CODEC ID: %d\n", ctx->codec_id);
		ctx->width = stream->bih->biWidth;
		ctx->height = stream->bih->biHeight;
		ctx->bit_rate = 800000;
#if (LIBAVFORMAT_BUILD >= 4624)
		ctx->time_base.den = stream->h.dwRate;
		ctx->time_base.num = stream->h.dwScale;
#else
		ctx->frame_rate = stream->h.dwRate;
		ctx->frame_rate_base = stream->h.dwScale;
#endif
		if(stream->bih+1 && (stream->bih->biSize > sizeof(BITMAPINFOHEADER)))
		{
			ctx->extradata = mp_malloc(stream->bih->biSize - sizeof(BITMAPINFOHEADER));
			if(ctx->extradata != NULL)
			{
				ctx->extradata_size = stream->bih->biSize - sizeof(BITMAPINFOHEADER);
				memcpy(ctx->extradata, stream->bih+1, ctx->extradata_size);
			}
			else
				MSG_ERR("MUXER_LAVF(video stream) error! couldn't allocate %d bytes for extradata\n",
					stream->bih->biSize - sizeof(BITMAPINFOHEADER));
		}
	}
    }
}

/* initialize optional fields of a packet */
static inline void av_init_pkt(AVPacket *pkt)
{
    pkt->pts   = AV_NOPTS_VALUE;
    pkt->dts   = AV_NOPTS_VALUE;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->flags = 0;
    pkt->stream_index = 0;
    pkt->destruct= av_destruct_packet_nofree;
}

static void write_chunk(muxer_stream_t *stream, size_t len, unsigned int flags, float pts)
{
	muxer_t *muxer = (muxer_t*) stream->muxer;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_stream_priv_t *spriv = (muxer_stream_priv_t *) stream->priv;
	AVPacket pkt;

	if(len)
	{
	av_init_pkt(&pkt);
	pkt.size = len;
	pkt.stream_index= spriv->avstream->index;
	pkt.data = stream->buffer;

	if(flags & AVIIF_KEYFRAME)
		pkt.flags |= PKT_FLAG_KEY;
	else
		pkt.flags = 0;


	//pkt.pts = AV_NOPTS_VALUE;
#if LIBAVFORMAT_BUILD >= 4624
	pkt.pts = (stream->timer / av_q2d(priv->oc->streams[pkt.stream_index]->time_base) + 0.5);
#else
	pkt.pts = AV_TIME_BASE * stream->timer;
#endif
//fprintf(stderr, "%Ld %Ld id:%d tb:%f %f\n", pkt.dts, pkt.pts, pkt.stream_index, av_q2d(priv->oc->streams[pkt.stream_index]->time_base), stream->timer);

	if(muxer->avih.dwStreams>1)
	{
	    if(av_interleaved_write_frame(priv->oc, &pkt) != 0) //av_write_frame(priv->oc, &pkt)
	    {
		MSG_ERR("Error while writing frame\n");
	    }
	}else{
	}
	    if(av_write_frame(priv->oc, &pkt) != 0)
	    {
		MSG_ERR("Error while writing frame\n");
	    }
	}

	return;
}


static void write_header(muxer_t *muxer,demuxer_t*dinfo)
{
	const char *sname;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;

	MSG_INFO("Writing header\n");

	if((sname=demux_info_get(dinfo,INFOT_NAME))!=NULL)
	    if(sname[0]) strncpy(priv->oc->title,sname,512);
	if((sname=demux_info_get(dinfo,INFOT_AUTHOR))!=NULL)
	    if(sname[0]) strncpy(priv->oc->author,sname,512);
	if((sname=demux_info_get(dinfo,INFOT_COPYRIGHT))!=NULL)
	    if(sname[0]) strncpy(priv->oc->copyright,sname,512);
	if((sname=demux_info_get(dinfo,INFOT_COMMENTS))!=NULL)
	    if(sname[0]) strncpy(priv->oc->comment,sname,512);
	if((sname=demux_info_get(dinfo,INFOT_ALBUM))!=NULL)
	    if(sname[0]) strncpy(priv->oc->album,sname,512);
	if((sname=demux_info_get(dinfo,INFOT_GENRE))!=NULL)
	    if(sname[0]) strncpy(priv->oc->genre,sname,32);
	if((sname=demux_info_get(dinfo,INFOT_TRACK))!=NULL)
	    if(sname[0]) priv->oc->track=atoi(sname);
	if((sname=demux_info_get(dinfo,INFOT_DATE))!=NULL)
	    if(sname[0]) priv->oc->year=atoi(sname);
	av_write_header(priv->oc);
	muxer->cont_write_header = NULL;
}


static void write_index(muxer_t *muxer)
{
	int i;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;

	MSG_INFO("Writing indexes\n");
	av_write_trailer(priv->oc);
	for(i = 0; i < priv->oc->nb_streams; i++)
	{
		mpxp_freep(&(priv->oc->streams[i]));
	}

	url_fclose(priv->oc->pb);

	mpxp_free(priv->oc);
}

int muxer_init_muxer_lavf(muxer_t *muxer,const char *subtype)
{
	muxer_priv_t *priv;
	AVOutputFormat *fmt = NULL;
	char mp_filename[256] = "mpxp://stream.dummy";

	MSG_WARN(
"** MUXER_LAVF *****************************************************************\n"
"If you wish to use libavformat muxing, you must ensure that your video stream\n"
"does not contain B frames (out of order decoding)\n"
"REMEMBER: libavformat muxing is presently broken and will generate\n"
"INCORRECT files in the presence of B frames\n"
"*******************************************************************************\n");
	priv = (muxer_priv_t *) mp_calloc(1, sizeof(muxer_priv_t));
	if(priv == NULL)
		return 0;

	av_register_all();

	priv->oc = av_alloc_format_context();
	if(!priv->oc)
	{
		MSG_FATAL("Couldn't get format context\n");
		goto fail;
	}

	if(!(fmt = guess_format(subtype, NULL, NULL)))
	{
		MSG_FATAL("CAN'T GET SPECIFIED FORMAT '%s'\n",subtype);
		goto fail;
	}
	priv->oc->oformat = fmt;


	if(av_set_parameters(priv->oc, NULL) < 0)
	{
		MSG_FATAL("Invalid output format parameters\n");
		goto fail;
	}
	priv->oc->packet_size= mux_packet_size;
	priv->oc->mux_rate= mux_rate;
	priv->oc->preload= (int)(mux_preload*AV_TIME_BASE);
	priv->oc->max_delay= (int)(mux_max_delay*AV_TIME_BASE);

	register_protocol(&mp_protocol);

	if(url_fopen(priv->oc->pb, mp_filename, URL_WRONLY))
	{
		MSG_FATAL("Coulnd't open outfile\n");
		goto fail;
	}

	((URLContext*)(priv->oc->pb->opaque))->priv_data= muxer;

	muxer->priv = (any_t*) priv;
	muxer->cont_new_stream = &lavf_new_stream;
	muxer->cont_write_chunk = &write_chunk;
	muxer->cont_write_header = &write_header;
	muxer->cont_write_index = &write_index;
	muxer->fix_parameters = &fix_parameters;
	MSG_V("OK, exit\n");
	return 1;

fail:
	delete priv;
	return 0;
}
