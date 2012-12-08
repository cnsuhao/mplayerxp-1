#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 SMJPEG file parser by Alex Beregszaszi

 Only for testing some files.
 Commited only for Nexus' request.

 Based on text by Arpi (SMJPEG-wtag.txt) and later on
 http://www.lokigames.com/development/download/smjpeg/SMJPEG.txt

 TODO: demuxer->movi_length
 TODO: DP_KEYFRAME
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strtok */

#include "help_mp.h"

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "osdep/bswap.h"
#include "demux_msg.h"

static MPXP_Rc smjpeg_probe(Demuxer* demuxer){
    int orig_pos = demuxer->stream->tell();
    char buf[8];
    int version;

    MSG_V("Checking for SMJPEG\n");

    if (demuxer->stream->read_word() == 0xA) {
	demuxer->stream->read( buf, 6);
	buf[7] = 0;

	if (strncmp("SMJPEG", buf, 6)) {
	    MSG_DBG2("Failed: SMJPEG\n");
	    return MPXP_False;
	}
    }
    else
	return MPXP_False;

    version = demuxer->stream->read_dword();
    if (version != 0) {
	MSG_ERR("Unknown version (%d) of SMJPEG. Please report!\n",version);
	return MPXP_False;
    }

    demuxer->stream->seek( orig_pos);
    demuxer->file_format=Demuxer::Type_SMJPEG;

    return MPXP_Ok;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int smjpeg_demux(Demuxer *demux,Demuxer_Stream *__ds)
{
    int dtype, dsize, dpts;

    demux->filepos = demux->stream->tell();

    dtype = demux->stream->read_dword_le();
    dpts = demux->stream->read_dword();
    dsize = demux->stream->read_dword();

    switch(dtype)
    {
	case mmioFOURCC('s','n','d','D'):
	    /* fixme, but no decoder implemented yet */
	    demux->audio->read_packet(demux->stream, dsize,
		(float)dpts/1000.0, demux->filepos, DP_NONKEYFRAME);
	    break;
	case mmioFOURCC('v','i','d','D'):
	    demux->video->read_packet(demux->stream, dsize,
		(float)dpts/1000.0, demux->filepos, DP_NONKEYFRAME);
	    break;
	case mmioFOURCC('D','O','N','E'):
	    return 1;
	default:
	    return 0;
    }

    return 1;
}

static Opaque* smjpeg_open(Demuxer* demuxer){
    sh_video_t* sh_video;
    sh_audio_t* sh_audio;
    unsigned int htype = 0, hleng;
    int i = 0;

    /* file header */
    demuxer->stream->skip( 8); /* \x00\x0aSMJPEG */
    demuxer->stream->skip( 4);

    MSG_V("This clip is %d seconds\n",
	demuxer->stream->read_dword());

    /* stream header */
    while (i < 3)
    {
	i++;
	htype = demuxer->stream->read_dword_le();
	if (htype == mmioFOURCC('H','E','N','D'))
	    break;
	hleng = (demuxer->stream->read_word()<<16)|demuxer->stream->read_word();
	switch(htype)
	{
	case mmioFOURCC('_','V','I','D'):
	    sh_video = demuxer->new_sh_video();
	    demuxer->video->sh = sh_video;
	    sh_video->ds = demuxer->video;

	    sh_video->bih = new(zeromem) BITMAPINFOHEADER;

	    demuxer->stream->skip( 4); /* number of frames */
//	    sh_video->fps = 24;
	    sh_video->src_w = demuxer->stream->read_word();
	    sh_video->src_h = demuxer->stream->read_word();
	    sh_video->fourcc = demuxer->stream->read_dword_le();

	    /* these are false values */
	    sh_video->bih->biSize = 40;
	    sh_video->bih->biWidth = sh_video->src_w;
	    sh_video->bih->biHeight = sh_video->src_h;
	    sh_video->bih->biPlanes = 3;
	    sh_video->bih->biBitCount = 12;
	    sh_video->bih->biCompression = sh_video->fourcc;
	    sh_video->bih->biSizeImage = sh_video->src_w*sh_video->src_h;
	    break;
	case mmioFOURCC('_','S','N','D'):
	    sh_audio = demuxer->new_sh_audio();
	    demuxer->audio->sh = sh_audio;
	    sh_audio->ds = demuxer->audio;

	    sh_audio->wf = new(zeromem) WAVEFORMATEX;

	    sh_audio->rate = demuxer->stream->read_word();
	    sh_audio->wf->wBitsPerSample = demuxer->stream->read_char();
	    sh_audio->nch = demuxer->stream->read_char();
	    sh_audio->wtag = demuxer->stream->read_dword_le();
	    sh_audio->wf->wFormatTag = sh_audio->wtag;
	    sh_audio->wf->nChannels = sh_audio->nch;
	    sh_audio->wf->nSamplesPerSec = sh_audio->rate;
	    sh_audio->wf->nAvgBytesPerSec = sh_audio->wf->nChannels*
	    sh_audio->wf->wBitsPerSample*sh_audio->wf->nSamplesPerSec/8;
	    sh_audio->wf->nBlockAlign = sh_audio->nch*2;
	    sh_audio->wf->cbSize = 0;
	    break;
	case mmioFOURCC('_','T','X','T'):
	    demuxer->stream->skip( demuxer->stream->read_dword());
	    break;
	}
    }

    demuxer->flags &= ~Demuxer::Seekable;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return demuxer;
}

static void smjpeg_close(Demuxer *demuxer) { UNUSED(demuxer); }

static MPXP_Rc smjpeg_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_smjpeg =
{
    "smjpeg",
    "SMJPEG parser",
    ".smjpeg",
    NULL,
    smjpeg_probe,
    smjpeg_open,
    smjpeg_demux,
    NULL,
    smjpeg_close,
    smjpeg_control
};
