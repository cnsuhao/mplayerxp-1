#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * video_out.c,
 *
 * Copyright (C) Aaron Holtzman - June 2000
 *
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "video_out.h"
#include "video_out_internal.h"

#include "osdep/shmem.h"
#include "mpxp_conf_lavc.h"
#include "xmpcore/xmp_core.h"
#include "mplayerxp.h"
#include "osdep/fastmemcpy.h"
#include "img_format.h"
#include "screenshot.h"
#include "osdep/bswap.h"
#include "dri_vo.h"
#include "osd_render.h"
#include "sub.h"
#include "postproc/vf.h"
#include "vo_msg.h"

namespace	usr{

VO_Config::VO_Config() {
    movie_aspect=-1.0;
    softzoom=1;
    flip=-1;
    xp_buffs=64;
    dbpp=0;
    xinerama_screen=0;
}
VO_Config vo_conf;

//
// Externally visible list of all vo drivers
//
extern const vo_info_t x11_vo_info;
extern const vo_info_t xv_vo_info;
extern const vo_info_t sdl_vo_info;
extern const vo_info_t null_vo_info;
extern const vo_info_t fbdev_vo_info;
extern const vo_info_t opengl_vo_info;
extern const vo_info_t vesa_vo_info;

static const vo_info_t* vo_infos[] =
{
#ifdef HAVE_XV
	&xv_vo_info,
#endif
#ifdef HAVE_OPENGL
	&opengl_vo_info,
#endif
#ifdef HAVE_X11
	&x11_vo_info,
#endif
#ifdef HAVE_SDL
	&sdl_vo_info,
#endif
#ifdef HAVE_VESA
	&vesa_vo_info,
#endif
#ifdef HAVE_FBDEV
	&fbdev_vo_info,
#endif
	&null_vo_info,
	NULL
};

typedef struct dri_priv_s {
    unsigned		flags;
    int			has_dri;
    unsigned		bpp;
    dri_surface_cap_t	cap;
    unsigned		num_xp_frames;
    int			dr,planes_eq,is_planar,accel;
    unsigned		sstride;
    uint32_t		d_width,d_height;
    unsigned		off[4]; /* offsets for y,u,v if DR on non fully fitted surface */
}dri_priv_t;

struct vo_priv_t : public video_private {
    vo_priv_t();
    virtual ~vo_priv_t();

    Opaque			unusable;
    uint32_t			srcFourcc,image_format,image_width,image_height;
    uint32_t			org_width,org_height;
    unsigned			ps_off[4]; /* offsets for y,u,v in panscan mode */
    unsigned long long int	frame_counter;
    vo_format_desc		vod;
    dri_priv_t			dri;
    const vo_info_t*		video_out;
    class VO_Interface*		vo_iface;
    std::vector<const vo_info_t*> list;
    const OSD_Render*		draw_alpha;
    vf_stream_t*		parent;
};

vo_priv_t::vo_priv_t() {
#ifdef HAVE_XV
	list.push_back(&xv_vo_info);
#endif
#ifdef HAVE_OPENGL
	list.push_back(&opengl_vo_info);
#endif
#ifdef HAVE_X11
	list.push_back(&x11_vo_info);
#endif
#ifdef HAVE_SDL
	list.push_back(&sdl_vo_info);
#endif
#ifdef HAVE_VESA
	list.push_back(&vesa_vo_info);
#endif
#ifdef HAVE_FBDEV
	list.push_back(&fbdev_vo_info);
#endif
	list.push_back(&null_vo_info);
    dri.num_xp_frames=1;
}

vo_priv_t::~vo_priv_t() {
    if(draw_alpha) delete draw_alpha;
    delete vo_iface;
}

Video_Output::Video_Output()
	    :vo_priv(*new(zeromem) vo_priv_t) {
    inited=0;
    osd_progbar_type=-1;
    osd_progbar_value=100;   // 0..256
}

Video_Output::~Video_Output() {
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_uninit"<<std::endl;
    inited--;
    delete &priv;
}

void Video_Output::print_help() {
    unsigned i;
    mpxp_info<<"Available video output drivers:"<<std::endl;
    i=0;
    while (vo_infos[i]) {
	const vo_info_t *info = vo_infos[i++];
	mpxp_info<<"\t"<<info->short_name<<"\t"<<info->name<<std::endl;
    }
    mpxp_info<<std::endl;
}

const vo_info_t* Video_Output::get_info() const {
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    return priv.video_out;
}

MPXP_Rc Video_Output::init(const std::string& driver_name) const {
    size_t offset;
    std::string drv_name;
    std::string subdev;
    if(!driver_name.empty()) {
	drv_name=driver_name;
	offset=drv_name.find(':');
	if(offset!=std::string::npos) subdev = drv_name.substr(offset+1);
    }
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    size_t i,sz=priv.list.size();
    if(drv_name.empty()) priv.video_out=priv.list[0];
    else
    for (i=0; i<sz; i++){
	const vo_info_t *info = priv.list[i];
	if(info->short_name==drv_name){
	    priv.video_out = priv.list[i];
	    break;
	}
    }
    priv.frame_counter=0;
    if(priv.video_out) {
	priv.vo_iface=priv.video_out->query_interface(subdev);
    }
    return priv.vo_iface?MPXP_Ok:MPXP_False;
}

void Video_Output::dri_config(uint32_t fourcc) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    priv.dri.is_planar = vo_describe_fourcc(fourcc,&priv.vod);
    priv.dri.bpp=priv.vod.bpp;
    if(!priv.dri.bpp) priv.dri.has_dri=0; /*unknown fourcc*/
    if(priv.dri.has_dri) {
	priv.dri.num_xp_frames=priv.vo_iface->get_num_frames();
	priv.dri.num_xp_frames=std::min(priv.dri.num_xp_frames,unsigned(MAX_DRI_BUFFERS));
    }
}

void Video_Output::ps_tune(unsigned width,unsigned height) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    int src_is_planar;
    unsigned src_stride,ps_x,ps_y;
    vo_format_desc vd;
    ps_x = (priv.org_width - width)/2;
    ps_y = (priv.org_height - height)/2;
    src_is_planar = vo_describe_fourcc(priv.srcFourcc,&vd);
    src_stride=src_is_planar?priv.org_width:priv.org_width*((vd.bpp+7)/8);
    priv.ps_off[0] = priv.ps_off[1] = priv.ps_off[2] = priv.ps_off[3] = 0;
    if(!src_is_planar)
	priv.ps_off[0] = ps_y*src_stride+ps_x*((vd.bpp+7)/8);
    else {
	priv.ps_off[0] = ps_y*src_stride+ps_x;
	if(vd.bpp==12) { /*YV12 series*/
	    priv.ps_off[1] = (ps_y/2)*(src_stride/2)+ps_x/2;
	    priv.ps_off[2] = (ps_y/2)*(src_stride/2)+ps_x/2;
	}
	else if(vd.bpp==9) { /*YVU9 series*/
	    priv.ps_off[1] = (ps_y/4)*(src_stride/4)+ps_x/4;
	    priv.ps_off[2] = (ps_y/4)*(src_stride/4)+ps_x/4;
	}
    }
}

void Video_Output::dri_tune(unsigned width,unsigned height) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    priv.dri.sstride=priv.dri.is_planar?width:width*((priv.dri.bpp+7)/8);
    priv.dri.off[0] = priv.dri.off[1] = priv.dri.off[2] = priv.dri.off[3] = 0;
    if(!priv.dri.is_planar) {
	priv.dri.planes_eq = priv.dri.sstride == priv.dri.cap.strides[0];
	priv.dri.off[0] = priv.dri.cap.y*priv.dri.cap.strides[0]+priv.dri.cap.x*((priv.dri.bpp+7)/8);
    }
    else {
	unsigned long y_off,u_off,v_off;
	dri_surface_t surf;
	surf.idx=0;
	priv.vo_iface->get_surface(&surf);
	y_off = (unsigned long)surf.planes[0];
	u_off = (unsigned long)std::min(surf.planes[1],surf.planes[2]);
	v_off = (unsigned long)std::max(surf.planes[1],surf.planes[2]);
	priv.dri.off[0] = priv.dri.cap.y*priv.dri.cap.strides[0]+priv.dri.cap.x;
	if(priv.dri.bpp==12) { /*YV12 series*/
	    priv.dri.planes_eq = width == priv.dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*5/4 == v_off - y_off &&
			priv.dri.cap.strides[0]/2 == priv.dri.cap.strides[1] &&
			priv.dri.cap.strides[0]/2 == priv.dri.cap.strides[2];
	    priv.dri.off[1] = (priv.dri.cap.y/2)*priv.dri.cap.strides[1]+priv.dri.cap.x/2;
	    priv.dri.off[2] = (priv.dri.cap.y/2)*priv.dri.cap.strides[2]+priv.dri.cap.x/2;
	}
	else if(priv.dri.bpp==9) { /*YVU9 series*/
	    priv.dri.planes_eq = width == priv.dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*17/16 == v_off - y_off &&
			priv.dri.cap.strides[0]/4 == priv.dri.cap.strides[1] &&
			priv.dri.cap.strides[0]/4 == priv.dri.cap.strides[2];
	    priv.dri.off[1] = (priv.dri.cap.y/4)*priv.dri.cap.strides[1]+priv.dri.cap.x/4;
	    priv.dri.off[2] = (priv.dri.cap.y/4)*priv.dri.cap.strides[2]+priv.dri.cap.x/4;
	} else if(priv.dri.bpp==8) /*Y800 series*/
	    priv.dri.planes_eq = width == priv.dri.cap.strides[0];
    }
    priv.dri.accel=(priv.dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER))==
			    (DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER);
    priv.dri.dr = priv.srcFourcc == priv.dri.cap.fourcc && !(priv.dri.flags & VOFLAG_FLIPPING) &&
			    !priv.ps_off[0] && !priv.ps_off[1] && !priv.ps_off[2] && !priv.ps_off[3];
    if(priv.dri.dr && priv.dri.cap.w < width)
	priv.dri.dr = priv.dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(priv.dri.dr && priv.dri.cap.w > width)
	priv.dri.dr = priv.dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(priv.dri.dr && priv.dri.cap.h < height)
	priv.dri.dr = priv.dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_VERTSCALER)?1:0;
    if(priv.dri.dr && priv.dri.cap.h > height)
	priv.dri.dr = priv.dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER)?1:0;
}

void Video_Output::dri_reconfig(int is_resize ) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    priv.dri.has_dri = 1;
    priv.vo_iface->get_surface_caps(&priv.dri.cap);
    dri_config(priv.dri.cap.fourcc);
    /* ugly workaround of swapped BGR-fourccs. Should be removed in the future */
    if(!priv.dri.has_dri) {
	priv.dri.has_dri=1;
	priv.dri.cap.fourcc = bswap_32(priv.dri.cap.fourcc);
	dri_config(priv.dri.cap.fourcc);
    }
    dri_tune(priv.image_width,priv.image_height);
    /* TODO: smart analizer of scaling possibilities of vo_driver */
    if(is_resize) {
	mpxp_context().engine().xp_core->in_resize=1;
	vf_reinit_vo(priv.parent,priv.dri.cap.w,priv.dri.cap.h,priv.dri.cap.fourcc,1);
    }
    vf_reinit_vo(priv.parent,priv.dri.cap.w,priv.dri.cap.h,priv.dri.cap.fourcc,0);
}

MPXP_Rc Video_Output::configure(vf_stream_t* s,uint32_t width, uint32_t height, uint32_t d_width,
		   uint32_t d_height, vo_flags_e _fullscreen, const std::string& title,
		   uint32_t format)
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    priv.parent=s;
    MPXP_Rc retval;
    unsigned dest_fourcc,w,d_w,h,d_h;
    mpxp_dbg3<<"dri_vo_dbg: vo_config"<<std::endl;
    if(inited) {
	mpxp_fatal<<"!!!priv.video_out internal fatal error: priv.video_out is initialized more than once!!!"<<std::endl;
	return MPXP_False;
    }
    inited++;
    flags=_fullscreen;
    dest_fourcc = format;
    priv.org_width = width;
    priv.org_height = height;

    w = width;
    d_w = d_width;
    h = height;
    d_h = d_height;

    priv.dri.d_width = d_w;
    priv.dri.d_height = d_h;
    mpxp_v<<"priv.video_out->config("<<w<<","<<h<<","<<d_w<<","<<d_h
	  <<", 0x"<<std::hex<<_fullscreen<<",'"<<title<<"',"<<vo_format_name(dest_fourcc)<<")"<<std::endl;
    retval = priv.vo_iface->configure(w,h,d_w,d_h,_fullscreen,title,dest_fourcc);
    priv.srcFourcc=format;
    if(retval == MPXP_Ok) {
	priv.vo_iface->get_surface_caps(&priv.dri.cap);
	priv.image_format = format;
	priv.image_width = w;
	priv.image_height = h;
	ps_tune(priv.image_width,priv.org_height);
	dri_reconfig(0);
	mpxp_v<<"dri_vo_caps: driver does "<<(priv.dri.has_dri?"":"not")<<" support DRI"<<std::endl;
	mpxp_v<<"dri_vo_caps: caps="<<std::hex<<std::setfill('0')<<std::setw(8)<<priv.dri.cap.caps
	      <<" fourcc="<<std::hex<<std::setfill('0')<<std::setw(8)<<priv.dri.cap.fourcc
	      <<"("<<vo_format_name(priv.dri.cap.fourcc)<<") x,y,w,h("
	      <<priv.dri.cap.x<<" "<<priv.dri.cap.y<<" "<<priv.dri.cap.w<<" "<<priv.dri.cap.h<<")"<<std::endl;
	mpxp_v<<"dri_vo_caps: width,height("<<priv.dri.cap.width<<","<<priv.dri.cap.height
	      <<") strides("
		<<priv.dri.cap.strides[0]<<priv.dri.cap.strides[1]
		<<priv.dri.cap.strides[2]<<priv.dri.cap.strides[3]
	        <<") priv.dri.bpp="<<priv.dri.bpp<<std::endl;
	mpxp_v<<"dri_vo_src: w,h("<<width<<","<<height
	      <<") d_w,d_h("<<d_width<<","<<d_height<<std::endl;
	mpxp_v<<"dri_vo_src: flags="<<std::hex<<std::setfill('0')<<std::setw(8)<<_fullscreen
	      <<" fourcc="<<std::hex<<std::setfill('0')<<std::setw(8)<<format
	      <<"("<<vo_format_name(format)<<")"<<std::endl;
	priv.dri.flags = _fullscreen;
    }
    return retval;
}

/* if vo_driver doesn't support dri then it won't work with this logic */
uint32_t Video_Output::query_format(uint32_t* fourcc, unsigned src_w, unsigned src_h) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    uint32_t dri_forced_fourcc;
    vo_query_fourcc_t qfourcc;
    mpxp_dbg3<<"dri_vo_dbg: vo_query_format("<<std::hex<<std::setfill('0')<<std::setw(8)<<*fourcc<<")"<<std::endl;
    qfourcc.fourcc = *fourcc;
    qfourcc.w = src_w;
    qfourcc.h = src_h;
    if(priv.vo_iface->query_format(&qfourcc)==MPXP_False)
	qfourcc.flags=VOCAP_NA;
    mpxp_v<<"dri_vo: request for "<<vo_format_name(*fourcc)<<" fourcc: "<<qfourcc.flags<<std::endl;
    dri_forced_fourcc = *fourcc;
    return qfourcc.flags;
}

MPXP_Rc Video_Output::reset() const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_reset"<<std::endl;
    return priv.vo_iface->reset();
}

MPXP_Rc Video_Output::screenshot(unsigned idx) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_screenshot"<<std::endl;
    std::ostringstream oss;
    oss<<priv.frame_counter;
    dri_surface_t surf;
    surf.idx=idx;
    priv.vo_iface->get_surface(&surf);
    return gr_screenshot(oss.str(),const_cast<const uint8_t**>(surf.planes),priv.dri.cap.strides,priv.dri.cap.fourcc,priv.dri.cap.width,priv.dri.cap.height);
}

MPXP_Rc Video_Output::pause() const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_pause"<<std::endl;
    return priv.vo_iface->pause();
}

MPXP_Rc Video_Output::resume() const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_resume"<<std::endl;
    return priv.vo_iface->resume();
}

MPXP_Rc Video_Output::get_surface_caps(dri_surface_cap_t*caps) const {
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    priv.vo_iface->get_surface_caps(caps);
    return MPXP_Ok;
}

MPXP_Rc Video_Output::get_surface(mp_image_t* mpi) const 
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    int width_less_stride;
    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface type="<<std::hex<<mpi->type<<" flg="<<std::hex<<mpi->flags<<std::endl;
    width_less_stride = 0;
    if(mpi->flags & MP_IMGFLAG_PLANAR) {
	width_less_stride = mpi->w <= priv.dri.cap.strides[0] &&
			    (mpi->w>>mpi->chroma_x_shift) <= priv.dri.cap.strides[1] &&
			    (mpi->w>>mpi->chroma_x_shift) <= priv.dri.cap.strides[2];
    }
    else width_less_stride = mpi->w*mpi->bpp <= priv.dri.cap.strides[0];
    if(priv.dri.has_dri) {
	/* static is singlebuffered decoding */
	if(mpi->type==MP_IMGTYPE_STATIC && priv.dri.num_xp_frames>1) {
	    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface FAIL mpi->type==MP_IMGTYPE_STATIC && priv.dri.num_xp_frames>1"<<std::endl;
	    return MPXP_False;
	}
	/*I+P requires 2+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IP && (priv.dri.num_xp_frames < 2 || (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED)) {
	    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IP && priv.dri.num_xp_frames < 2) || (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED"<<std::endl;
	    return MPXP_False;
	}
	/*I+P+B requires 3+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IPB && (priv.dri.num_xp_frames != 3 || (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED)) {
	    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IPB && priv.dri.num_xp_frames != 3) || (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED"<<std::endl;
	    return MPXP_False;
	}
	/* video surface is bad thing for reading */
	if(((mpi->flags&MP_IMGFLAG_READABLE)||(mpi->type==MP_IMGTYPE_TEMP)) && (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED) {
	    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface FAIL mpi->flags&MP_IMGFLAG_READABLE && (priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED"<<std::endl;
	    return MPXP_False;
	}
	/* it seems that surfaces are equal */
	if((((mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE) && width_less_stride) || priv.dri.planes_eq) && priv.dri.dr) {
	    dri_surface_t surf;
	    surf.idx=mpi->xp_idx;
	    priv.vo_iface->get_surface(&surf);
	    mpi->planes[0]=surf.planes[0]+priv.dri.off[0];
	    mpi->planes[1]=surf.planes[1]+priv.dri.off[1];
	    mpi->planes[2]=surf.planes[2]+priv.dri.off[2];
	    mpi->stride[0]=priv.dri.cap.strides[0];
	    mpi->stride[1]=priv.dri.cap.strides[1];
	    mpi->stride[2]=priv.dri.cap.strides[2];
	    mpi->flags|=MP_IMGFLAG_DIRECT;
	    mpxp_dbg2<<"dri_vo_dbg: vo_get_surface OK"<<std::endl;
	    return MPXP_True;
	}
	mpxp_dbg2<<"dri_vo_dbg: vo_get_surface FAIL (mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE && width_less_stride) || priv.dri.planes_eq) && priv.dri.dr"<<std::endl;
	return MPXP_False;
    }
    else return MPXP_False;
}

int Video_Output::adjust_size(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: adjust_size was called "<<cw<<" "<<ch<<" "<<*nw<<" "<<*nh<<std::endl;
    if((priv.dri.flags & VOFLAG_SWSCALE) && (cw != *nw || ch != *nh) && !(priv.dri.flags & VOFLAG_FULLSCREEN))
    {
	float aspect,newv;
	aspect = (float)priv.dri.d_width / (float)priv.dri.d_height;
	if(abs(cw-*nw) > abs(ch-*nh))
	{
	    newv = ((float)(*nw))/aspect;
	    *nh = newv;
	    if(newv-(float)(unsigned)newv > 0.5) (*nh)++;
	}
	else
	{
	    newv = ((float)(*nh))*aspect;
	    *nw = newv;
	    if(newv-(float)(unsigned)newv > 0.5) (*nw)++;
	}
	mpxp_dbg3<<"dri_vo_dbg: adjust_size returns "<<*nw<<" "<<*nh<<std::endl;
	return 1;
    }
    return 0;
}

static int __FASTCALL__ adjust_size(const Video_Output*vo,unsigned cw,unsigned ch,unsigned *nw,unsigned *nh)
{
    return vo->adjust_size(cw,ch,nw,nh);
}

int Video_Output::check_events() const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    uint32_t retval;
    int need_repaint;
    vo_resize_t vrest;
    mpxp_dbg3<<"dri_vo_dbg: vo_check_events"<<std::endl;
    vrest.event_type = 0;
    vrest.vo = this;
    vrest.adjust_size = ::adjust_size;
    retval = vrest.event_type = priv.vo_iface->check_events(&vrest);
    /* it's ok since accelerated drivers doesn't touch surfaces
       but there is only one driver (vo_x11) which changes surfaces
       on 'fullscreen' key */
    need_repaint=0;
    if(priv.dri.has_dri && retval == MPXP_True && (vrest.event_type & VO_EVENT_RESIZE) == VO_EVENT_RESIZE) {
	need_repaint=1;
	dri_reconfig(1);
    }
    return (need_repaint && !priv.dri.accel) || (vrest.event_type&VO_EVENT_FORCE_UPDATE);
}

MPXP_Rc Video_Output::fullscreen() const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    MPXP_Rc retval;
    mpxp_dbg3<<"dri_vo_dbg: vo_fullscreen"<<std::endl;
    retval = priv.vo_iface->toggle_fullscreen();
    if(priv.dri.has_dri && retval == MPXP_True)
	dri_reconfig(1);
    if(retval == MPXP_True) priv.dri.flags ^= VOFLAG_FULLSCREEN;
    return retval;
}

unsigned Video_Output::get_num_frames() const {
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    return priv.dri.num_xp_frames;
}

MPXP_Rc Video_Output::draw_slice(const mp_image_t& smpi) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    unsigned i,_w[4],_h[4],x,y;
    mpxp_dbg3<<"dri_vo_dbg: vo_draw_slice smpi.xywh="<<smpi.x<<" "<<smpi.y<<" "<<smpi.w<<" "<<smpi.h<<std::endl;
    if(priv.dri.has_dri) {
	uint8_t *dst[4];
	const uint8_t *ps_src[4];
	int dstStride[4];
	int finalize=is_final();
	unsigned idx = smpi.xp_idx;
	dri_surface_t surf;
	surf.idx=idx;
	priv.vo_iface->get_surface(&surf);
	for(i=0;i<4;i++) {
	    dst[i]=surf.planes[i]+priv.dri.off[i];
	    dstStride[i]=priv.dri.cap.strides[i];
	    dst[i]+=((smpi.y*dstStride[i])*priv.vod.y_mul[i])/priv.vod.y_div[i];
	    dst[i]+=(smpi.x*priv.vod.x_mul[i])/priv.vod.x_div[i];
	    _w[i]=(smpi.w*priv.vod.x_mul[i])/priv.vod.x_div[i];
	    _h[i]=(smpi.h*priv.vod.y_mul[i])/priv.vod.y_div[i];
	    y = i?(smpi.y>>smpi.chroma_y_shift):smpi.y;
	    x = i?(smpi.x>>smpi.chroma_x_shift):smpi.x;
	    ps_src[i] = smpi.planes[i]+(y*smpi.stride[i])+x+priv.ps_off[i];
	}
	for(i=0;i<4;i++) {
	    if(smpi.stride[i] && dstStride[i]) {
		if(finalize)
		    stream_copy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],smpi.stride[i]);
		else
		    memcpy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],smpi.stride[i]);
	    }
	}
	return MPXP_Ok;
    }
    return MPXP_False;
}

void Video_Output::select_frame(unsigned play_idx) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg2<<"dri_vo_dbg: vo_select_frame(play_idx="<<play_idx<<")"<<std::endl;
    priv.vo_iface->select_frame(play_idx);
}

void Video_Output::flush_page(unsigned decoder_idx) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_flush_pages [idx="<<decoder_idx<<"]"<<std::endl;
    priv.frame_counter++;
    if((priv.dri.cap.caps & DRI_CAP_VIDEO_MMAPED)!=DRI_CAP_VIDEO_MMAPED)
	priv.vo_iface->flush_page(decoder_idx);
}

/* DRAW OSD */
void Video_Output::clear_rect(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const
{
  vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<priv.dri.cap.y||_y0+i>=priv.dri.cap.y+priv.dri.cap.h) memset(dest,filler,stride);
      dest += dstride;
  }
}

void Video_Output::clear_rect2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const
{
  vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
  unsigned i;
  unsigned _y1 = priv.dri.cap.y/2;
  unsigned _y2 = (priv.dri.cap.y+priv.dri.cap.h)/2;
  for(i=0;i<h;i++)
  {
      if(_y0+i<_y1||_y0+i>=_y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

void Video_Output::clear_rect4(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const
{
  vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
  unsigned i;
  unsigned _y1 = priv.dri.cap.y/4;
  unsigned _y2 = (priv.dri.cap.y+priv.dri.cap.h)/4;
  for(i=0;i<h;i++)
  {
      if(_y0+i<_y1||_y0+i>=_y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

void Video_Output::clear_rect_rgb(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride) const
{
  vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<priv.dri.cap.y||_y0+i>=priv.dri.cap.y+priv.dri.cap.h) memset(dest,0,stride);
      dest += dstride;
  }
}

void Video_Output::clear_rect_yuy2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride) const
{
  vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
  unsigned i;
  for(i=0;i<h;i++)
  {
	if(_y0+i<priv.dri.cap.y||_y0+i>=priv.dri.cap.y+priv.dri.cap.h)
	{
	    uint32_t *dst32;
	    unsigned j,size32;
	    size32=stride/4;
	    dst32=(uint32_t*)dest;
	    for(j=0;j<size32;j+=4)
		dst32[j]=dst32[j+1]=dst32[j+2]=dst32[j+3]=0x80108010;
	    for(;j<size32;j+=4)
		dst32[j]=0x80108010;
	}
	dest += dstride;
  }
}

void Video_Output::dri_remove_osd(unsigned idx,int x0,int _y0, int w,int h) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    dri_surface_t surf;
    surf.idx=idx;
    priv.vo_iface->get_surface(&surf);
    if(unsigned(x0+w)<=priv.dri.cap.width&&unsigned(_y0+h)<=priv.dri.cap.height)
    switch(priv.dri.cap.fourcc)
    {
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
		clear_rect_rgb(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0*((priv.dri.bpp+7)/8),
			    w*(priv.dri.bpp+7)/8,priv.dri.cap.strides[0]);
		break;
	case IMGFMT_YVYU:
	case IMGFMT_YUY2:
		clear_rect_yuy2(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0*2,
			    w*2,priv.dri.cap.strides[0]);
		break;
	case IMGFMT_UYVY:
		clear_rect_yuy2(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0*2+1,
			    w*2,priv.dri.cap.strides[0]);
		break;
	case IMGFMT_Y800:
		clear_rect(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0,
			    w,priv.dri.cap.strides[0],0x10);
		break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		clear_rect(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0,
			    w,priv.dri.cap.strides[0],0x10);
		clear_rect2(_y0/2,h/2,surf.planes[1]+_y0/2*priv.dri.cap.strides[1]+x0/2,
			    w/2,priv.dri.cap.strides[1],0x80);
		clear_rect2(_y0/2,h/2,surf.planes[2]+_y0/2*priv.dri.cap.strides[2]+x0/2,
			    w/2,priv.dri.cap.strides[2],0x80);
		break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
		clear_rect(_y0,h,surf.planes[0]+_y0*priv.dri.cap.strides[0]+x0,
			    w,priv.dri.cap.strides[0],0x10);
		clear_rect4(_y0/4,h/4,surf.planes[1]+_y0/4*priv.dri.cap.strides[1]+x0/4,
			    w/4,priv.dri.cap.strides[1],0x80);
		clear_rect4(_y0/4,h/4,surf.planes[2]+_y0/4*priv.dri.cap.strides[2]+x0/4,
			    w/4,priv.dri.cap.strides[2],0x80);
		break;
    }
}

void Video_Output::dri_draw_osd(unsigned idx,int x0,int _y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    int finalize=is_final();
    if(unsigned(x0+w)<=priv.dri.cap.width&&unsigned(_y0+h)<=priv.dri.cap.height)
    {
	if(!priv.draw_alpha) priv.draw_alpha=new(zeromem) OSD_Render(priv.dri.cap.fourcc);
	if(priv.draw_alpha) {
	    dri_surface_t surf;
	    surf.idx=idx;
	    priv.vo_iface->get_surface(&surf);
	    priv.draw_alpha->render(w,h,src,srca,stride,
			    surf.planes[0]+priv.dri.cap.strides[0]*_y0+x0*((priv.dri.bpp+7)/8),
			    priv.dri.cap.strides[0],finalize);
	}
    }
}

static void dri_remove_osd(const Video_Output* vo,unsigned idx,int x0,int _y0, int w,int h) {
    return vo->dri_remove_osd(idx,x0,_y0,w,h);
}
static void dri_draw_osd(const Video_Output* vo,unsigned idx,int x0,int _y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride) {
    return vo->dri_draw_osd(idx,x0,_y0,w,h,src,srca,stride);
}

void Video_Output::draw_osd(unsigned idx) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_draw_osd"<<std::endl;
    if(priv.dri.has_dri && !(priv.dri.cap.caps & DRI_CAP_HWOSD))
    {
	if( priv.dri.cap.x || priv.dri.cap.y ||
	    priv.dri.cap.w != priv.dri.cap.width || priv.dri.cap.h != priv.dri.cap.height)
		    vo_remove_text(this,idx,priv.dri.cap.width,priv.dri.cap.height,::dri_remove_osd);
	vo_draw_text(this,idx,priv.dri.cap.width,priv.dri.cap.height,::dri_draw_osd);
    }
}

void Video_Output::draw_spudec_direct(unsigned idx) const
{
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    mpxp_dbg3<<"dri_vo_dbg: vo_draw_osd"<<std::endl;
    if(priv.dri.has_dri && !(priv.dri.cap.caps & DRI_CAP_HWOSD))
    {
//	if( priv.dri.cap.x || priv.dri.cap.y ||
//	    priv.dri.cap.w != priv.dri.cap.width || priv.dri.cap.h != priv.dri.cap.height)
//		    vo_remove_text(idx,priv.dri.cap.width,priv.dri.cap.height,dri_remove_osd);
	vo_draw_spudec(this,idx,priv.dri.cap.width,priv.dri.cap.height,::dri_draw_osd);
    }
}


MPXP_Rc Video_Output::ctrl(uint32_t request, any_t*data) const
{
    MPXP_Rc rval;
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    rval=priv.vo_iface->ctrl(request,data);
    mpxp_dbg3<<"dri_vo_dbg: "<<rval<<"=vo_control("<<request<<","<<std::hex<<reinterpret_cast<long>(data)<<std::endl;
    return rval;
}

int Video_Output::is_final() const {
    vo_priv_t& priv=static_cast<vo_priv_t&>(vo_priv);
    int mmaped=priv.dri.cap.caps&DRI_CAP_VIDEO_MMAPED;
    int busmaster=priv.dri.cap.caps&DRI_CAP_BUSMASTERING;
    return mmaped||busmaster||(priv.dri.num_xp_frames>1);
}

int __FASTCALL__ vo_describe_fourcc(uint32_t fourcc,vo_format_desc *vd)
{
    int is_planar;
    is_planar=0;
    vd->x_mul[0]=vd->x_mul[1]=vd->x_mul[2]=vd->x_mul[3]=1;
    vd->x_div[0]=vd->x_div[1]=vd->x_div[2]=vd->x_div[3]=1;
    vd->y_mul[0]=vd->y_mul[1]=vd->y_mul[2]=vd->y_mul[3]=1;
    vd->y_div[0]=vd->y_div[1]=vd->y_div[2]=vd->y_div[3]=1;
	switch(fourcc)
	{
		case IMGFMT_Y800:
		    is_planar=1;
		case IMGFMT_RGB8:
		case IMGFMT_BGR8:
		    vd->bpp = 8;
		    break;
		case IMGFMT_YVU9:
		case IMGFMT_IF09:
		    vd->bpp = 9;
		    vd->x_div[1]=vd->x_div[2]=4;
		    vd->y_div[1]=vd->y_div[2]=4;
		    is_planar=1;
		    break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
		    vd->bpp = 12;
		    vd->x_div[1]=vd->x_div[2]=2;
		    vd->y_div[1]=vd->y_div[2]=2;
		    is_planar=1;
		    break;
		case IMGFMT_YUY2:
		case IMGFMT_YVYU:
		case IMGFMT_UYVY:
		    vd->x_mul[0]=2;
		    vd->bpp = 16;
		    break;
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
		    vd->bpp = 15;
		    vd->x_mul[0]=2;
		    break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
		    vd->bpp = 16;
		    vd->x_mul[0]=2;
		    break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
		    vd->bpp = 24;
		    vd->x_mul[0]=3;
		    break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
		    vd->bpp = 32;
		    vd->x_mul[0]=4;
		    break;
		default:
		    /* unknown fourcc */
		    vd->bpp=0;
		    break;
	}
    return is_planar;
}

} // namespace	usr
