/*
    Realaudio demuxer for MPlayer
		(c) 2003 Roberto Togni
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "../mp_config.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "bswap.h"
#include "aviprint.h"
#include "demux_msg.h"

#define FOURCC_DOTRA mmioFOURCC('.','r','a', 0xfd)
#define FOURCC_144 mmioFOURCC('1','4','_','4')
#define FOURCC_288 mmioFOURCC('2','8','_','8')
#define FOURCC_DNET mmioFOURCC('d','n','e','t')
#define FOURCC_LPCJ mmioFOURCC('l','p','c','J')


typedef struct {
	unsigned short version;
	unsigned int dotranum;
	unsigned int data_size;
	unsigned short version2;
	unsigned int hdr_size;
	unsigned short codec_flavor;
	unsigned int coded_framesize;
	unsigned short sub_packet_h;
	unsigned short frame_size;
	unsigned short sub_packet_size;
	char genr[4];
	float last_pts;
} ra_priv_t;



static int ra_probe(demuxer_t* demuxer)
{
	unsigned int chunk_id;
  
	chunk_id = stream_read_dword_le(demuxer->stream);
	if (chunk_id == FOURCC_DOTRA)
		return 1;
	else
		return 0;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int ra_demux(demuxer_t *demuxer,demux_stream_t *__ds)
{
	ra_priv_t *ra_priv = demuxer->priv;
	int len;
	demux_stream_t *ds = demuxer->audio;
	sh_audio_t *sh = ds->sh;
	WAVEFORMATEX *wf = sh->wf;
	demux_packet_t *dp;

	if (stream_eof(demuxer->stream)) return 0;

	if(demuxer->movi_length==UINT_MAX && sh->i_bps)
	    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh->i_bps);

	len = wf->nBlockAlign;
	demuxer->filepos = stream_tell(demuxer->stream);

	dp = new_demux_packet(len);
	len=stream_read(demuxer->stream, dp->buffer, len);
	resize_demux_packet(dp,len);

	if(sh->i_bps)
	{
	    ra_priv->last_pts = ra_priv->last_pts < 0 ? 0 : ra_priv->last_pts + len/(float)sh->i_bps;
	    ds->pts = ra_priv->last_pts - (ds_tell_pts(demuxer->audio)-sh->a_in_buffer_len)/(float)sh->i_bps;
	}
	else dp->pts = demuxer->filepos / ra_priv->data_size;
	dp->pos = demuxer->filepos;
	dp->flags = DP_NONKEYFRAME;
	ds_add_packet(ds, dp);

	return 1;
}

static demuxer_t * ra_open(demuxer_t* demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
	sh_audio_t *sh;
	int i;
	char *buf;

  if ((ra_priv = (ra_priv_t *)malloc(sizeof(ra_priv_t))) == NULL) {
    MSG_ERR(MSGTR_OutOfMemory);
    return 0;
  }
	memset(ra_priv, 0, sizeof(ra_priv_t));

	demuxer->priv = ra_priv;
	sh = new_sh_audio(demuxer, 0);
	demuxer->audio->id = 0;
	sh->ds=demuxer->audio;
	demuxer->audio->sh = sh;

	ra_priv->version = stream_read_word(demuxer->stream);
	MSG_V("[RealAudio] File version: %d\n", ra_priv->version);
	if ((ra_priv->version < 3) || (ra_priv->version > 4)) {
		MSG_WARN("[RealAudio] ra version %d is not supported yet, please "
			"contact MPlayer developers\n", ra_priv->version);
		return 0;
	}
	if (ra_priv->version == 3) {
		ra_priv->hdr_size = stream_read_word(demuxer->stream);
		stream_skip(demuxer->stream, 10);
		ra_priv->data_size = stream_read_dword(demuxer->stream);
	} else {
		stream_skip(demuxer->stream, 2);
		ra_priv->dotranum = stream_read_dword(demuxer->stream);
		ra_priv->data_size = stream_read_dword(demuxer->stream);
		ra_priv->version2 = stream_read_word(demuxer->stream);
		ra_priv->hdr_size = stream_read_dword(demuxer->stream);
		ra_priv->codec_flavor = stream_read_word(demuxer->stream);
		ra_priv->coded_framesize = stream_read_dword(demuxer->stream);
		stream_skip(demuxer->stream, 4); // data size?
		stream_skip(demuxer->stream, 8);
		ra_priv->sub_packet_h = stream_read_word(demuxer->stream);
		ra_priv->frame_size = stream_read_word(demuxer->stream);
		MSG_V("[RealAudio] Frame size: %d\n", ra_priv->frame_size);
		ra_priv->sub_packet_size = stream_read_word(demuxer->stream);
		MSG_V("[RealAudio] Sub packet size: %d\n", ra_priv->sub_packet_size);
		stream_skip(demuxer->stream, 2);
		sh->samplerate = stream_read_word(demuxer->stream);
		stream_skip(demuxer->stream, 2);
		sh->samplesize = stream_read_word(demuxer->stream);
		sh->channels = stream_read_word(demuxer->stream);
		MSG_V("[RealAudio] %d channel, %d bit, %dHz\n", sh->channels,
			sh->samplesize, sh->samplerate);
		i = stream_read_char(demuxer->stream);
		*((unsigned int *)(ra_priv->genr)) = stream_read_dword(demuxer->stream);
		if (i != 4) {
			MSG_WARN("[RealAudio] Genr size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
		i = stream_read_char(demuxer->stream);
		sh->format = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			MSG_WARN("[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
		stream_skip(demuxer->stream, 3);
	}

	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, INFOT_NAME, buf);
		free(buf);
	}
	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, INFOT_AUTHOR, buf);
		free(buf);
	}
	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, INFOT_COPYRIGHT, buf);
		free(buf);
	}
	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, INFOT_COMMENTS, buf);
		free(buf);
	}

	if (ra_priv->version == 3) {
	    if(ra_priv->hdr_size + 8 > stream_tell(demuxer->stream)) {
		stream_skip(demuxer->stream, 1);
		i = stream_read_char(demuxer->stream);
		sh->format = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			MSG_WARN("[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
//		stream_skip(demuxer->stream, 3);

		if (sh->format != FOURCC_LPCJ) {
			MSG_WARN("[RealAudio] Version 3 with FourCC %8x, please report to "
				"MPlayer developers\n", sh->format);
		}

		sh->channels = 1;
		sh->samplesize = 16;
		sh->samplerate = 8000;
		ra_priv->frame_size = 240;
		sh->format = FOURCC_144;
	    } else {
		// If a stream does not have fourcc, let's assume it's 14.4
		sh->format = FOURCC_LPCJ;

		sh->channels = 1;
		sh->samplesize = 16;
		sh->samplerate = 8000;
		ra_priv->frame_size = 240;
		sh->format = FOURCC_144;
	    }
	}

	/* Fill WAVEFORMATEX */
	sh->wf = malloc(sizeof(WAVEFORMATEX));
	memset(sh->wf, 0, sizeof(WAVEFORMATEX));
	sh->wf->nChannels = sh->channels;
	sh->wf->wBitsPerSample = sh->samplesize;
	sh->wf->nSamplesPerSec = sh->samplerate;
	sh->wf->nAvgBytesPerSec = sh->samplerate*sh->samplesize/8;
	sh->wf->nBlockAlign = ra_priv->frame_size;
	sh->wf->cbSize = 0;
	sh->wf->wFormatTag = sh->format;

	switch (sh->format) {
		case FOURCC_144:
			MSG_V("Audio: 14_4\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=240;
			    ((short*)(sh->wf+1))[2]=0;
			    ((short*)(sh->wf+1))[3]=0x14;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_288:
			MSG_V("Audio: 28_8\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=ra_priv->sub_packet_h;
			    ((short*)(sh->wf+1))[2]=ra_priv->codec_flavor;
			    ((short*)(sh->wf+1))[3]=ra_priv->coded_framesize;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_DNET: /* direct support */
			break;
		default:
			MSG_V("Audio: Unknown (%d)\n", sh->format);
	}

	print_wave_header(sh->wf,sizeof(WAVEFORMATEX));

	/* disable seeking */
	demuxer->flags &= ~DEMUXF_SEEKABLE;

	if(!ds_fill_buffer(demuxer->audio))
		MSG_WARN("[RealAudio] No data.\n");

    return demuxer;
}



static void ra_close(demuxer_t *demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
 
	if (ra_priv)
		free(ra_priv);

	return;
}


#if 0
/* please upload RV10 samples WITH INDEX CHUNK */
int demux_seek_ra(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    real_priv_t *priv = demuxer->priv;
    demux_stream_t *d_audio = demuxer->audio;
    sh_audio_t *sh_audio = d_audio->sh;
    int aid = d_audio->id;
    int next_offset = 0;
    int rel_seek_frames = 0;
    int streams = 0;

    return stream_seek(demuxer->stream, next_offset);
}
#endif

static int ra_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return DEMUX_UNKNOWN;
}

demuxer_driver_t demux_ra =
{
    "Real audio parser",
    ".ra",
    NULL,
    ra_probe,
    ra_open,
    ra_demux,
    NULL,
    ra_close,
    ra_control
};
