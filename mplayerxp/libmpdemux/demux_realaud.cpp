#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    Realaudio demuxer for MPlayer
		(c) 2003 Roberto Togni
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "help_mp.h"

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "osdep/bswap.h"
#include "libao3/afmt.h"
#include "aviprint.h"
#include "demux_msg.h"

#define FOURCC_DOTRA mmioFOURCC('.','r','a', 0xfd)
#define FOURCC_144 mmioFOURCC('1','4','_','4')
#define FOURCC_288 mmioFOURCC('2','8','_','8')
#define FOURCC_DNET mmioFOURCC('d','n','e','t')
#define FOURCC_LPCJ mmioFOURCC('l','p','c','J')

struct realaud_priv_t : public Opaque {
    public:
	realaud_priv_t() {}
	virtual ~realaud_priv_t() {}

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
};

static MPXP_Rc realaud_probe(Demuxer* demuxer)
{
    unsigned int chunk_id;

    chunk_id = demuxer->stream->read_dword_le();
    if (chunk_id == FOURCC_DOTRA)
	return MPXP_Ok;
    else
	return MPXP_False;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int realaud_demux(Demuxer *demuxer,Demuxer_Stream *__ds)
{
	realaud_priv_t *realaud_priv = static_cast<realaud_priv_t*>(demuxer->priv);
	int len;
	Demuxer_Stream *ds = demuxer->audio;
	sh_audio_t *sh = reinterpret_cast<sh_audio_t*>(ds->sh);
	WAVEFORMATEX *wf = sh->wf;

	if (demuxer->stream->eof()) return 0;

	if(demuxer->movi_length==UINT_MAX && sh->i_bps)
	    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh->i_bps);

	len = wf->nBlockAlign;
	demuxer->filepos = demuxer->stream->tell();

	Demuxer_Packet *dp = new(zeromem) Demuxer_Packet(len);
	len=demuxer->stream->read( dp->buffer(), len);
	dp->resize(len);

	if(sh->i_bps)
	{
	    realaud_priv->last_pts = realaud_priv->last_pts < 0 ? 0 : realaud_priv->last_pts + len/(float)sh->i_bps;
	    ds->pts = realaud_priv->last_pts - (demuxer->audio->tell_pts()-sh->a_in_buffer_len)/(float)sh->i_bps;
	}
	else dp->pts = demuxer->filepos / realaud_priv->data_size;
	dp->pos = demuxer->filepos;
	dp->flags = DP_NONKEYFRAME;
	ds->add_packet(dp);

	return 1;
}

static Opaque* realaud_open(Demuxer* demuxer)
{
    realaud_priv_t* realaud_priv = static_cast<realaud_priv_t*>(demuxer->priv);
    sh_audio_t *sh;
    int i;
    char *buf;

    if ((realaud_priv = (realaud_priv_t *)mp_mallocz(sizeof(realaud_priv_t))) == NULL) {
	MSG_ERR(MSGTR_OutOfMemory);
	return 0;
    }

	demuxer->priv = realaud_priv;
	sh = demuxer->new_sh_audio();
	demuxer->audio->id = 0;
	sh->ds=demuxer->audio;
	demuxer->audio->sh = sh;

	realaud_priv->version = demuxer->stream->read_word();
	MSG_V("[RealAudio] File version: %d\n", realaud_priv->version);
	if ((realaud_priv->version < 3) || (realaud_priv->version > 4)) {
		MSG_WARN("[RealAudio] ra version %d is not supported yet, please "
			"contact MPlayer developers\n", realaud_priv->version);
		return 0;
	}
	if (realaud_priv->version == 3) {
		realaud_priv->hdr_size = demuxer->stream->read_word();
		demuxer->stream->skip( 10);
		realaud_priv->data_size = demuxer->stream->read_dword();
	} else {
		demuxer->stream->skip( 2);
		realaud_priv->dotranum = demuxer->stream->read_dword();
		realaud_priv->data_size = demuxer->stream->read_dword();
		realaud_priv->version2 = demuxer->stream->read_word();
		realaud_priv->hdr_size = demuxer->stream->read_dword();
		realaud_priv->codec_flavor = demuxer->stream->read_word();
		realaud_priv->coded_framesize = demuxer->stream->read_dword();
		demuxer->stream->skip( 4); // data size?
		demuxer->stream->skip( 8);
		realaud_priv->sub_packet_h = demuxer->stream->read_word();
		realaud_priv->frame_size = demuxer->stream->read_word();
		MSG_V("[RealAudio] Frame size: %d\n", realaud_priv->frame_size);
		realaud_priv->sub_packet_size = demuxer->stream->read_word();
		MSG_V("[RealAudio] Sub packet size: %d\n", realaud_priv->sub_packet_size);
		demuxer->stream->skip( 2);
		sh->rate = demuxer->stream->read_word();
		demuxer->stream->skip( 2);
		sh->afmt = bps2afmt(demuxer->stream->read_word());
		sh->nch = demuxer->stream->read_word();
		MSG_V("[RealAudio] %d channel, %d bit, %dHz\n", sh->nch,
			afmt2bps(sh->afmt), sh->rate);
		i = demuxer->stream->read_char();
		*((unsigned int *)(realaud_priv->genr)) = demuxer->stream->read_dword();
		if (i != 4) {
			MSG_WARN("[RealAudio] Genr size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			demuxer->stream->skip( i - 4);
		}
		i = demuxer->stream->read_char();
		sh->wtag = demuxer->stream->read_dword_le();
		if (i != 4) {
			MSG_WARN("[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			demuxer->stream->skip( i - 4);
		}
		demuxer->stream->skip( 3);
	}

	if ((i = demuxer->stream->read_char()) != 0) {
		buf = new char [i+1];
		demuxer->stream->read( buf, i);
		buf[i] = 0;
		demuxer->info().add( INFOT_NAME, buf);
		delete buf;
	}
	if ((i = demuxer->stream->read_char()) != 0) {
		buf = new char [i+1];
		demuxer->stream->read( buf, i);
		buf[i] = 0;
		demuxer->info().add( INFOT_AUTHOR, buf);
		delete buf;
	}
	if ((i = demuxer->stream->read_char()) != 0) {
		buf = new char [i+1];
		demuxer->stream->read( buf, i);
		buf[i] = 0;
		demuxer->info().add( INFOT_COPYRIGHT, buf);
		delete buf;
	}
	if ((i = demuxer->stream->read_char()) != 0) {
		buf = new char [i+1];
		demuxer->stream->read( buf, i);
		buf[i] = 0;
		demuxer->info().add( INFOT_COMMENTS, buf);
		delete buf;
	}

	if (realaud_priv->version == 3) {
	    if(realaud_priv->hdr_size + 8 > demuxer->stream->tell()) {
		demuxer->stream->skip( 1);
		i = demuxer->stream->read_char();
		sh->wtag = demuxer->stream->read_dword_le();
		if (i != 4) {
			MSG_WARN("[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			demuxer->stream->skip( i - 4);
		}
//		demuxer->stream->skip( 3);

		if (sh->wtag != FOURCC_LPCJ) {
			MSG_WARN("[RealAudio] Version 3 with FourCC %8x, please report to "
				"MPlayer developers\n", sh->wtag);
		}

		sh->nch = 1;
		sh->afmt = bps2afmt(2);
		sh->rate = 8000;
		realaud_priv->frame_size = 240;
		sh->wtag = FOURCC_144;
	    } else {
		// If a stream does not have fourcc, let's assume it's 14.4
		sh->wtag = FOURCC_LPCJ;

		sh->nch = 1;
		sh->afmt = bps2afmt(2);
		sh->rate = 8000;
		realaud_priv->frame_size = 240;
		sh->wtag = FOURCC_144;
	    }
	}

	/* Fill WAVEFORMATEX */
	sh->wf = new(zeromem) WAVEFORMATEX;
	sh->wf->nChannels = sh->nch;
	sh->wf->wBitsPerSample = afmt2bps(sh->afmt);
	sh->wf->nSamplesPerSec = sh->rate;
	sh->wf->nAvgBytesPerSec = sh->rate*afmt2bps(sh->afmt);
	sh->wf->nBlockAlign = realaud_priv->frame_size;
	sh->wf->cbSize = 0;
	sh->wf->wFormatTag = sh->wtag;

	switch (sh->wtag) {
		case FOURCC_144:
			MSG_V("Audio: 14_4\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = (WAVEFORMATEX*)mp_realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=240;
			    ((short*)(sh->wf+1))[2]=0;
			    ((short*)(sh->wf+1))[3]=0x14;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_288:
			MSG_V("Audio: 28_8\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = (WAVEFORMATEX*)mp_realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=realaud_priv->sub_packet_h;
			    ((short*)(sh->wf+1))[2]=realaud_priv->codec_flavor;
			    ((short*)(sh->wf+1))[3]=realaud_priv->coded_framesize;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_DNET: /* direct support */
			break;
		default:
			MSG_V("Audio: Unknown (%d)\n", sh->wtag);
	}

	print_wave_header(sh->wf,sizeof(WAVEFORMATEX));

	/* disable seeking */
	demuxer->flags &= ~Demuxer::Seekable;

	if(!demuxer->audio->fill_buffer())
		MSG_WARN("[RealAudio] No data.\n");
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return realaud_priv;
}



static void realaud_close(Demuxer *demuxer)
{
    realaud_priv_t* realaud_priv = static_cast<realaud_priv_t*>(demuxer->priv);
    if (realaud_priv)
	delete realaud_priv;
    return;
}


#if 0
/* please upload RV10 samples WITH INDEX CHUNK */
int demux_seek_ra(Demuxer *demuxer,const seek_args_t* seeka)
{
    real_priv_t *priv = demuxer->priv;
    Demuxer_Stream *d_audio = demuxer->audio;
    sh_audio_t *sh_audio = d_audio->sh;
    int aid = d_audio->id;
    int next_offset = 0;
    int rel_seek_frames = 0;
    int streams = 0;

    return stream_seek(demuxer->stream, next_offset);
}
#endif

static MPXP_Rc realaud_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_realaud =
{
    "realaud",
    "Real audio parser",
    ".ra",
    NULL,
    realaud_probe,
    realaud_open,
    realaud_demux,
    NULL,
    realaud_close,
    realaud_control
};
