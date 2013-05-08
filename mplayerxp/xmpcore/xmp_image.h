#ifndef __XMP_IMAGE_H_INCLUDED
#define __XMP_IMAGE_H_INCLUDED 1

#include <stdlib.h>
#include <limits.h>

#include "mpxp_config.h"

namespace	usr {
    //--- buffer content restrictions:
    enum {
	MP_IMGFLAG_PRESERVE=0x00001, // set if buffer content shouldn't be modified:
	MP_IMGFLAG_READABLE=0x00002  // set if buffer content will be READ for next frame's MC: (I/P mpeg frames)
    };

    //--- buffer width/stride/plane restrictions: (used for direct rendering)
    enum {
// stride _have_to_ be aligned to MB boundary:  [for DR restrictions]
	MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE=0x00004, /* no flag - should be neg value of MP_IMGFLAG_ACCEPT_STRIDE */
// stride should be aligned to MB boundary:     [for buffer allocation]
	MP_IMGFLAG_PREFER_ALIGNED_STRIDE=0x00008, /* sould be no flag - everything prefer aligned strides */
	MP_IMGFLAG_ACCEPT_STRIDE	=0x00010, // codec accept any stride (>=width):
	MP_IMGFLAG_ACCEPT_WIDTH		=0x00020, // codec accept any width (width*bpp=stride -> stride%bpp==0) (>=width):
//--- for planar formats only:
// uses only stride[0], and stride[1]=stride[2]=stride[0]>>mpi->chroma_x_shift
	MP_IMGFLAG_COMMON_STRIDE	=0x00040, /* UNUSED */
// uses only planes[0], and calculates planes[1,2] from width,height,imgfmt
	MP_IMGFLAG_COMMON_PLANE		=0x00080, /* UNUSED */
	MP_IMGFLAGMASK_RESTRICTIONS	=0x000FF,
//--------- color info (filled by mp_image_setfmt() ) -----------
	MP_IMGFLAG_PLANAR		=0x00100, // set if number of planes > 1
	MP_IMGFLAG_YUV			=0x00200, // set if it's YUV colorspace
	MP_IMGFLAG_SWAPPED		=0x00400, // set if it's swapped (BGR or YVU) plane/byteorder
	MP_IMGFLAG_RGB_PALETTE		=0x00800, // using palette for RGB data
	MP_IMGFLAGMASK_COLORS		=0x00F00,
// codec uses drawing/rendering callbacks (draw_slice()-like thing, DR method 2)
// [the codec will set this flag if it supports callbacks, and the vo _may_
//  clear it in get_image() if draw_slice() not implemented]
	MP_IMGFLAG_DRAW_CALLBACK	=0x01000,
	MP_IMGFLAG_DIRECT		=0x02000, // set if it's in video buffer/memory: [set by vo/vf's get_image() !!!]
	MP_IMGFLAG_ALLOCATED		=0x04000, // set if buffer is allocated (used in destination images):
	MP_IMGFLAG_TYPE_DISPLAYED	=0x08000, // buffer type was printed (do NOT set this flag - it's for INTERNAL USE!!!)
	MP_IMGFLAG_FINAL		=0x10000, // buffer is video memory
	MP_IMGFLAG_RENDERED		=0x20000, // final buffer was already painted
	MP_IMGFLAG_FINALIZED		=0x40000  // indicates final step of image processing from CPU side!!!
    };

    /* codec doesn't support any form of direct rendering - it has own buffer */
    enum {
	MP_IMGTYPE_EXPORT	=0, // allocation. so we just export its buffer pointers:
	MP_IMGTYPE_STATIC	=1, // codec requires a static WO buffer, but it does only partial updates later:
	MP_IMGTYPE_TEMP		=2, // codec just needs some WO memory, where it writes/copies the whole frame to:
	MP_IMGTYPE_IP		=3, // I+P type, requires 2+ independent static R/W buffers
	MP_IMGTYPE_IPB		=4, // I+P+B type, requires 2+ independent static R/W and 1+ temp WO buffers

	MP_MAX_PLANES		=4
    };
    enum {
	MP_IMGFIELD_ORDERED	=0x01,
	MP_IMGFIELD_TOP_FIRST	=0x02,
	MP_IMGFIELD_REPEAT_FIRST=0x04,
	MP_IMGFIELD_TOP		=0x08,
	MP_IMGFIELD_BOTTOM	=0x10,
	MP_IMGFIELD_INTERLACED	=0x20
    };

    enum { XP_IDX_INVALID=UINT_MAX };
    struct mp_image_t {
	unsigned		xp_idx; /* index of xp_frame associated with this image */
	unsigned int	flags;
	unsigned char	type;
	unsigned char	bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
	unsigned int	imgfmt;
	unsigned	width,height;  // stored dimensions
	int		x,y,w,h;  // slice dimensions
	unsigned	num_planes;
	unsigned char*	planes[MP_MAX_PLANES];
	unsigned int	stride[MP_MAX_PLANES];
	char *		qscale;
	unsigned	qstride;
	unsigned	qscale_type; // 0->mpeg1/4/h263, 1->mpeg2
	unsigned	pict_type; // 0->unknown, 1->I, 2->P, 3->B
	unsigned	fields;
    /* these are only used by planar formats Y,U(Cb),V(Cr) */
	int		chroma_width;
	int		chroma_height;
	int		chroma_x_shift; // horizontal
	int		chroma_y_shift; // vertical
	any_t*		priv; /* for private use by filter or vo driver (to store buffer id or dmpi) */
    };

    void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt);
    mp_image_t* new_mp_image(unsigned w,unsigned h,unsigned xp_idx);
    void free_mp_image(mp_image_t* mpi);
    mp_image_t* alloc_mpi(unsigned w, unsigned h, unsigned int fmt,unsigned xp_idx);
    void mpi_alloc_planes(mp_image_t *mpi);
    void copy_mpi(mp_image_t *dmpi,const mp_image_t *mpi);
    void mpi_fake_slice(mp_image_t *dmpi,const mp_image_t *mpi,unsigned y,unsigned height);
}// namespace
#endif
