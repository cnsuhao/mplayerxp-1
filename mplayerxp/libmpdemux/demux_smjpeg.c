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

#include "mp_config.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "osdep/bswap.h"
#include "demux_msg.h"
#include "osdep/mplib.h"

static MPXP_Rc smjpeg_probe(demuxer_t* demuxer){
    int orig_pos = stream_tell(demuxer->stream);
    char buf[8];
    int version;

    MSG_V("Checking for SMJPEG\n");

    if (stream_read_word(demuxer->stream) == 0xA) {
	stream_read(demuxer->stream, buf, 6);
	buf[7] = 0;

	if (strncmp("SMJPEG", buf, 6)) {
	    MSG_DBG2("Failed: SMJPEG\n");
	    return MPXP_False;
	}
    }
    else
	return MPXP_False;

    version = stream_read_dword(demuxer->stream);
    if (version != 0) {
	MSG_ERR("Unknown version (%d) of SMJPEG. Please report!\n",version);
	return MPXP_False;
    }

    stream_seek(demuxer->stream, orig_pos);
    demuxer->file_format=DEMUXER_TYPE_SMJPEG;

    return MPXP_Ok;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int smjpeg_demux(demuxer_t *demux,demux_stream_t *__ds)
{
    int dtype, dsize, dpts;

    demux->filepos = stream_tell(demux->stream);

    dtype = stream_read_dword_le(demux->stream);
    dpts = stream_read_dword(demux->stream);
    dsize = stream_read_dword(demux->stream);

    switch(dtype)
    {
	case mmioFOURCC('s','n','d','D'):
	    /* fixme, but no decoder implemented yet */
	    ds_read_packet(demux->audio, demux->stream, dsize,
		(float)dpts/1000.0, demux->filepos, DP_NONKEYFRAME);
	    break;
	case mmioFOURCC('v','i','d','D'):
	    ds_read_packet(demux->video, demux->stream, dsize,
		(float)dpts/1000.0, demux->filepos, DP_NONKEYFRAME);
	    break;
	case mmioFOURCC('D','O','N','E'):
	    return 1;
	default:
	    return 0;
    }

    return 1;
}

static demuxer_t* smjpeg_open(demuxer_t* demuxer){
    sh_video_t* sh_video;
    sh_audio_t* sh_audio;
    unsigned int htype = 0, hleng;
    int i = 0;

    /* file header */
    stream_skip(demuxer->stream, 8); /* \x00\x0aSMJPEG */
    stream_skip(demuxer->stream, 4);

    MSG_V("This clip is %d seconds\n",
	stream_read_dword(demuxer->stream));

    /* stream header */
    while (i < 3)
    {
	i++;
	htype = stream_read_dword_le(demuxer->stream);
	if (htype == mmioFOURCC('H','E','N','D'))
	    break;
	hleng = (stream_read_word(demuxer->stream)<<16)|stream_read_word(demuxer->stream);
	switch(htype)
	{
	case mmioFOURCC('_','V','I','D'):
	    sh_video = new_sh_video(demuxer, 0);
	    demuxer->video->sh = sh_video;
	    sh_video->ds = demuxer->video;

	    sh_video->bih = mp_mallocz(sizeof(BITMAPINFOHEADER));

	    stream_skip(demuxer->stream, 4); /* number of frames */
//	    sh_video->fps = 24;
	    sh_video->src_w = stream_read_word(demuxer->stream);
	    sh_video->src_h = stream_read_word(demuxer->stream);
	    sh_video->fourcc = stream_read_dword_le(demuxer->stream);

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
	    sh_audio = new_sh_audio(demuxer, 0);
	    demuxer->audio->sh = sh_audio;
	    sh_audio->ds = demuxer->audio;

	    sh_audio->wf = mp_mallocz(sizeof(WAVEFORMATEX));

	    sh_audio->rate = stream_read_word(demuxer->stream);
	    sh_audio->wf->wBitsPerSample = stream_read_char(demuxer->stream);
	    sh_audio->nch = stream_read_char(demuxer->stream);
	    sh_audio->wtag = stream_read_dword_le(demuxer->stream);
	    sh_audio->wf->wFormatTag = sh_audio->wtag;
	    sh_audio->wf->nChannels = sh_audio->nch;
	    sh_audio->wf->nSamplesPerSec = sh_audio->rate;
	    sh_audio->wf->nAvgBytesPerSec = sh_audio->wf->nChannels*
	    sh_audio->wf->wBitsPerSample*sh_audio->wf->nSamplesPerSec/8;
	    sh_audio->wf->nBlockAlign = sh_audio->nch*2;
	    sh_audio->wf->cbSize = 0;
	    break;
	case mmioFOURCC('_','T','X','T'):
	    stream_skip(demuxer->stream, stream_read_dword(demuxer->stream));
	    break;
	}
    }

    demuxer->flags &= ~DEMUXF_SEEKABLE;

    return demuxer;
}

static void smjpeg_close(demuxer_t *demuxer) {}

static MPXP_Rc smjpeg_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

demuxer_driver_t demux_smjpeg =
{
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
