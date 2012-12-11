#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>
#include <stdio.h>

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "demux_msg.h"

namespace mpxp {
struct dd_priv_t : public Opaque {
    public:
	dd_priv_t() {}
	virtual ~dd_priv_t();

	Demuxer* vd;
	Demuxer* ad;
	Demuxer* sd;
};

dd_priv_t::~dd_priv_t() {
    if(vd) { delete vd; vd=NULL; }
    if(ad && ad != vd) { delete ad; ad=NULL; }
    if(sd && sd != vd && sd != ad) { delete sd; sd=NULL; }
}

Demuxer*  new_demuxers_demuxer(Demuxer* vd, Demuxer* ad, Demuxer* sd) {
    Demuxer* ret;
    dd_priv_t* priv;

    ret = new(zeromem) Demuxer;

    priv = new(zeromem) dd_priv_t;
    priv->vd = vd;
    priv->ad = ad;
    priv->sd = sd;
    ret->priv = priv;

    ret->file_format = Demuxer::Type_DEMUXERS;
    // Video is the most important :-)
    ret->stream = vd->stream;
    ret->flags = (vd->flags&Demuxer::Seekable) && (ad->flags&Demuxer::Seekable) && (sd->flags&Demuxer::Seekable)?Demuxer::Seekable:Demuxer::NonSeekable;

    ret->video = vd->video;
    ret->audio = ad->audio;
    ret->sub = sd->sub;

    check_pin("demuxer",ad->pin,DEMUX_PIN);
    check_pin("demuxer",vd->pin,DEMUX_PIN);
    check_pin("demuxer",sd->pin,DEMUX_PIN);
    return ret;
}

static int demux_demuxers_fill_buffer(Demuxer *demux,Demuxer_Stream *ds) {
  dd_priv_t* priv=static_cast<dd_priv_t*>(demux->priv);

  if(ds->demuxer == priv->vd)
    return priv->vd->fill_buffer(ds);
  else if(ds->demuxer == priv->ad)
    return priv->ad->fill_buffer(ds);
  else if(ds->demuxer == priv->sd)
    return priv->sd->fill_buffer(ds);

  MSG_ERR("Demux demuxers fill_buffer error : bad demuxer : not vd, ad nor sd\n");
  return 0;
}

static void demux_demuxers_seek(Demuxer *demuxer,const seek_args_t* seeka) {
  dd_priv_t* priv=static_cast<dd_priv_t*>(demuxer->priv);
  float pos;

  seek_args_t seek_p = { seeka->secs, 1 };

  priv->ad->stream->eof(0);
  priv->sd->stream->eof(0);

  // Seek video
  priv->vd->seek(seeka);
  // Get the new pos
  pos = demuxer->video->pts;

  if(priv->ad != priv->vd) {
    sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;
    priv->ad->seek(&seek_p);
    // In case the demuxer don't set pts
    if(!demuxer->audio->pts)
      demuxer->audio->pts = pos-((demuxer->audio->tell_pts()-sh->a_in_buffer_len)/(float)sh->i_bps);
    if(sh->timer)
      sh->timer = 0;
  }

  if(priv->sd != priv->vd)
      priv->sd->seek(&seek_p);
}

static void demux_close_demuxers(Demuxer* demuxer) {
  dd_priv_t* priv = static_cast<dd_priv_t*>(demuxer->priv);

  delete priv;
  delete demuxer;
}
} // namespace mpxp
