#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * scaletempo audio filter
 * Copyright (c) 2007 Robert Juliano
 *
 * This file is part of MPlayer.
 *
 * MPlayer is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * scale tempo while maintaining pitch
 * (WSOLA technique with cross correlation)
 * inspired by SoundTouch library by Olli Parviainen
 *
 * basic algorithm
 *   - produce 'stride' output samples per loop
 *   - consume stride*scale input samples per loop
 *
 * to produce smoother transitions between strides, blend next overlap
 * samples from last stride with correlated samples of current input
 *
 */
#include <algorithm>

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "osdep/fastmemcpy.h"

#include "af.h"
#include "af_internal.h"
#include "pp_msg.h"

// Data for specific instances of this filter
struct af_scaletempo_t
{
  // stride
  float   scale;
  float   speed;
  float   frames_stride_scaled;
  float   frames_stride_error;
  int     bytes_per_frame;
  int     bytes_stride;
  float   bytes_stride_scaled;
  int     bytes_queue;
  int     bytes_queued;
  int     bytes_to_slide;
  int8_t* buf_queue;
  // overlap
  int     samples_overlap;
  int     samples_standing;
  int     bytes_overlap;
  int     bytes_standing;
  int8_t* buf_overlap;
  int8_t* table_blend;
  void    (*output_overlap)(af_scaletempo_t* s, int8_t* out_buf, int bytes_off);
  // best overlap
  int     frames_search;
  int     num_channels;
  int8_t* buf_pre_corr;
  int8_t* table_window;
  int     (*best_overlap_offset)(af_scaletempo_t* s);
  short   shift_corr;
  // command line
  float   scale_nominal;
  float   ms_stride;
  float   percent_overlap;
  float   ms_search;
  short   speed_tempo;
  short   speed_pitch;
};

static int fill_queue(af_instance_t* af,const mp_aframe_t* data, int offset)
{
  af_scaletempo_t* s = reinterpret_cast<af_scaletempo_t*>(af->setup);
  int bytes_in = data->len - offset;
  int offset_unchanged = offset;

  if (s->bytes_to_slide > 0) {
    if (s->bytes_to_slide < s->bytes_queued) {
      int bytes_move = s->bytes_queued - s->bytes_to_slide;
      memmove(s->buf_queue,
	      s->buf_queue + s->bytes_to_slide,
	      bytes_move);
      s->bytes_to_slide = 0;
      s->bytes_queued = bytes_move;
    } else {
      int bytes_skip;
      s->bytes_to_slide -= s->bytes_queued;
      bytes_skip = std::min(s->bytes_to_slide, bytes_in);
      s->bytes_queued = 0;
      s->bytes_to_slide -= bytes_skip;
      offset += bytes_skip;
      bytes_in -= bytes_skip;
    }
  }

  if (bytes_in > 0) {
    int bytes_copy = std::min(s->bytes_queue - s->bytes_queued, bytes_in);
    memcpy(s->buf_queue + s->bytes_queued,
	   (int8_t*)data->audio + offset,
	   bytes_copy);
    s->bytes_queued += bytes_copy;
    offset += bytes_copy;
  }

  return offset - offset_unchanged;
}

#define UNROLL_PADDING (4*4)

static int best_overlap_offset_float(af_scaletempo_t* s)
{
  float *pw, *po, *ppc, *search_start;
  float best_corr = INT_MIN;
  int best_off = 0;
  int i, off;

  pw  = (float*)s->table_window;
  po  = (float*)s->buf_overlap + s->num_channels;
  ppc = (float*)s->buf_pre_corr;
  for (i=s->num_channels; i<s->samples_overlap; i++) {
    *ppc++ = *pw++ * *po++;
  }

  search_start = (float*)s->buf_queue + s->num_channels;
  for (off=0; off<s->frames_search; off++) {
    float corr = 0;
    float* ps = search_start;
    ppc = (float*)s->buf_pre_corr;
    for (i=s->num_channels; i<s->samples_overlap; i++) {
      corr += *ppc++ * *ps++;
    }
    if (corr > best_corr) {
      best_corr = corr;
      best_off  = off;
    }
    search_start += s->num_channels;
  }

  return best_off * 4 * s->num_channels;
}

static void output_overlap_float(af_scaletempo_t* s, int8_t* buf_out,
				  int bytes_off)
{
  float* pout = (float*)buf_out;
  float* pb   = (float*)s->table_blend;
  float* po   = (float*)s->buf_overlap;
  float* pin  = (float*)(s->buf_queue + bytes_off);
  int i;
  for (i=0; i<s->samples_overlap; i++) {
    *pout++ = *po - *pb++ * ( *po - *pin++ ); po++;
  }
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(af_instance_t* af,const mp_aframe_t* ind)
{
    af_scaletempo_t* s = reinterpret_cast<af_scaletempo_t*>(af->setup);
    unsigned	offset_in;
    int8_t*	pout;

    if (s->scale == 1.0) return const_cast<mp_aframe_t*>(ind);

    mp_aframe_t* out = new_mp_aframe_genome(ind);
    out->len = ((int)(ind->len/s->bytes_stride_scaled)+1)*s->bytes_stride;
    mp_alloc_aframe(out);

    offset_in = fill_queue(af, ind, 0);
    pout = reinterpret_cast<int8_t*>(out->audio);
    while (s->bytes_queued >= s->bytes_queue) {
	int ti;
	float tf;
	int bytes_off = 0;

	// output stride
	if (s->output_overlap) {
	    if (s->best_overlap_offset)
		bytes_off = s->best_overlap_offset(s);
	    s->output_overlap(s, pout, bytes_off);
	}
	if(out->flags&MP_AFLG_FINALIZED)
	stream_copy(pout + s->bytes_overlap,
	   s->buf_queue + bytes_off + s->bytes_overlap,
	   s->bytes_standing);
	else
	memcpy(pout + s->bytes_overlap,
	   s->buf_queue + bytes_off + s->bytes_overlap,
	   s->bytes_standing);
	pout += s->bytes_stride;

	// input stride
	memcpy(s->buf_overlap,
	   s->buf_queue + bytes_off + s->bytes_stride,
	   s->bytes_overlap);
	tf = s->frames_stride_scaled + s->frames_stride_error;
	ti = (int)tf;
	s->frames_stride_error = tf - ti;
	s->bytes_to_slide = ti * s->bytes_per_frame;

	offset_in += fill_queue(af, ind, offset_in);
    }

    // This filter can have a negative delay when scale > 1:
    // output corresponding to some length of input can be decided and written
    // after receiving only a part of that input.
    af->delay = s->bytes_queued - s->bytes_to_slide;

    out->len  = pout - (int8_t *)out->audio;
    return out;
}

static MPXP_Rc __FASTCALL__ af_config(af_instance_t* af,const af_conf_t* arg)
{
    af_scaletempo_t* s = reinterpret_cast<af_scaletempo_t*>(af->setup);
    float srate = arg->rate / 1000;
    int nch = arg->nch;
    int bps = arg->format&MPAF_BPS_MASK;
    int frames_stride, frames_overlap;
    int i, j;

    MSG_V("[af_scaletempo] %.3f speed * %.3f scale_nominal = %.3f\n",
	   s->speed, s->scale_nominal, s->scale);

    if (s->scale == 1.0) {
      if (s->speed_tempo && s->speed_pitch)
	return MPXP_Detach;
      memcpy(&af->conf, arg, sizeof(af_conf_t));
      return af_test_output(af, arg);
    }

    af->conf.rate = arg->rate;
    af->conf.nch  = arg->nch;
    af->conf.format = MPAF_NE|MPAF_F|MPAF_BPS_4;

    frames_stride           = srate * s->ms_stride;
    s->bytes_stride         = frames_stride * bps * nch;
    s->bytes_stride_scaled  = s->scale * s->bytes_stride;
    s->frames_stride_scaled = s->scale * frames_stride;
    s->frames_stride_error  = 0;
    af->mul.n               = s->bytes_stride;
    af->mul.d               = s->bytes_stride_scaled;

    frames_overlap = frames_stride * s->percent_overlap;
    if (frames_overlap <= 0) {
      s->bytes_standing   = s->bytes_stride;
      s->samples_standing = s->bytes_standing / bps;
      s->output_overlap   = NULL;
    } else {
      s->samples_overlap  = frames_overlap * nch;
      s->bytes_overlap    = frames_overlap * nch * bps;
      s->bytes_standing   = s->bytes_stride - s->bytes_overlap;
      s->samples_standing = s->bytes_standing / bps;
      s->buf_overlap      = (int8_t*)mp_realloc(s->buf_overlap, s->bytes_overlap);
      s->table_blend      = (int8_t*)mp_realloc(s->table_blend, s->bytes_overlap * 4);
      if(!s->buf_overlap || !s->table_blend) {
	MSG_FATAL("[af_scaletempo] Out of memory\n");
	return MPXP_Error;
      }
      bzero(s->buf_overlap, s->bytes_overlap);
      {
	float* pb = (float*)s->table_blend;
	for (i=0; i<frames_overlap; i++) {
	  float v = i / (float)frames_overlap;
	  for (j=0; j<nch; j++) {
	    *pb++ = v;
	  }
	}
	s->output_overlap = output_overlap_float;
      }
    }
    s->frames_search = (frames_overlap > 1) ? srate * s->ms_search : 0;
    if (s->frames_search <= 0) {
      s->best_overlap_offset = NULL;
    } else {
	float* pw;
	s->buf_pre_corr = (int8_t*)mp_realloc(s->buf_pre_corr, s->bytes_overlap);
	s->table_window = (int8_t*)mp_realloc(s->table_window, s->bytes_overlap - nch * bps);
	if(!s->buf_pre_corr || !s->table_window) {
	  MSG_FATAL( "[af_scaletempo] Out of memory\n");
	  return MPXP_Error;
	}
	pw = (float*)s->table_window;
	for (i=1; i<frames_overlap; i++) {
	  float v = i * (frames_overlap - i);
	  for (j=0; j<nch; j++) {
	    *pw++ = v;
	  }
	}
	s->best_overlap_offset = best_overlap_offset_float;
    }

    s->bytes_per_frame = bps * nch;
    s->num_channels    = nch;

    s->bytes_queue
      = (s->frames_search + frames_stride + frames_overlap) * bps * nch;
    s->buf_queue = (int8_t*)mp_realloc(s->buf_queue, s->bytes_queue + UNROLL_PADDING);
    if(!s->buf_queue) {
      MSG_FATAL("[af_scaletempo] Out of memory\n");
      return MPXP_Error;
    }

    MSG_V ( "[af_scaletempo] "
	    "%.2f stride_in, %i stride_out, %i standing, "
	    "%i overlap, %i search, %i queue\n",
	    s->frames_stride_scaled,
	    (int)(s->bytes_stride / nch / bps),
	    (int)(s->bytes_standing / nch / bps),
	    (int)(s->bytes_overlap / nch / bps),
	    s->frames_search,
	    (int)(s->bytes_queue / nch / bps));

    return af_test_output(af,arg);
}
// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
  af_scaletempo_t* s = reinterpret_cast<af_scaletempo_t*>(af->setup);
  switch(cmd){
  case AF_CONTROL_PLAYBACK_SPEED | AF_CONTROL_SET:{
    if (s->speed_tempo) {
      if (s->speed_pitch) {
	break;
      }
      s->speed = *(float*)arg;
      s->scale = s->speed * s->scale_nominal;
    } else {
      if (s->speed_pitch) {
	s->speed = 1 / *(float*)arg;
	s->scale = s->speed * s->scale_nominal;
	break;
      }
    }
    return MPXP_Ok;
  }
  case AF_CONTROL_SCALETEMPO_AMOUNT | AF_CONTROL_SET:{
    s->scale = *(float*)arg;
    s->scale = s->speed * s->scale_nominal;
    return MPXP_Ok;
  }
  case AF_CONTROL_SCALETEMPO_AMOUNT | AF_CONTROL_GET:
    *(float*)arg = s->scale;
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    char speedstr[80]="tempo";
    if(arg)
    sscanf((char*)arg,"%f:%f:%f:%f:%s",
	 &s->scale_nominal,
	 &s->ms_stride,
	 &s->percent_overlap,
	 &s->ms_search,
	 &speedstr[0]);
    if (s->scale_nominal <= 0) {
      MSG_ERR("[af_scaletempo] Error: scale_nominal is out of range: > 0\n");
      return MPXP_Error;
    }
    if (s->ms_stride <= 0) {
      MSG_ERR("[af_scaletempo] Error: ms_stride is out of range: > 0\n");
      return MPXP_Error;
    }
    if (s->percent_overlap < 0 || s->percent_overlap > 1) {
      MSG_ERR("[af_scaletempo] Error: percent_overlap is out of range: [0..1]\n");
      return MPXP_Error;
    }
    if (s->ms_search < 0) {
      MSG_ERR("[af_scaletempo] Error: ms_search is out of range: >= 0\n");
      return MPXP_Error;
    }
    if (strlen(speedstr) > 0) {
      if (strcmp(speedstr, "pitch") == 0) {
	s->speed_tempo = 0;
	s->speed_pitch = 1;
      } else if (strcmp(speedstr, "tempo") == 0) {
	s->speed_tempo = 1;
	s->speed_pitch = 0;
      } else if (strcmp(speedstr, "none") == 0) {
	s->speed_tempo = 0;
	s->speed_pitch = 0;
      } else if (strcmp(speedstr, "both") == 0) {
	s->speed_tempo = 1;
	s->speed_pitch = 1;
      } else {
	MSG_ERR("[af_scaletempo] Error: speed is out of range: [pitch|tempo|none|both]\n");
	return MPXP_Error;
      }
    }
    s->scale = s->speed * s->scale_nominal;
    return MPXP_Ok;
  }
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_scaletempo] %6.3f scale, %6.2f stride, %6.2f overlap, %6.2f search, speed = %s\n", s->scale_nominal, s->ms_stride, s->percent_overlap, s->ms_search, (s->speed_tempo?(s->speed_pitch?"tempo and speed":"tempo"):(s->speed_pitch?"pitch":"none")));
    return MPXP_Ok;
  }
  return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(af_instance_t* af)
{
  af_scaletempo_t* s = reinterpret_cast<af_scaletempo_t*>(af->setup);
  delete s->buf_queue;
  delete s->buf_overlap;
  delete s->buf_pre_corr;
  delete s->table_blend;
  delete s->table_window;
  delete af->setup;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af_scaletempo_t* s;

  af->config_af    = af_config;
  af->control_af   = control_af;
  af->uninit    = uninit;
  af->play      = play;
  af->mul.d     = 1;
  af->mul.n     = 1;
  af->setup     = mp_calloc(1,sizeof(af_scaletempo_t));
  if(af->setup == NULL) return MPXP_Error;

  s = reinterpret_cast<af_scaletempo_t*>(af->setup);
  s->scale = s->speed = s->scale_nominal = 1.0;
  s->speed_tempo = 1;
  s->speed_pitch = 0;
  s->ms_stride = 60;
  s->percent_overlap = .20;
  s->ms_search = 14;
    check_pin("afilter",af->pin,AF_PIN);

  return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_scaletempo = {
  "Scale audio tempo while maintaining pitch",
  "scaletempo",
  "Robert Juliano",
  "",
  AF_FLAGS_REENTRANT,
  af_open
};
