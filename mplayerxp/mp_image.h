#ifndef __MP_IMAGE_H
#define __MP_IMAGE_H 1

#include "mp_config.h"

//--------- codec's requirements (filled by the codec/vf) ---------

//--- buffer content restrictions:
#define MP_IMGFLAG_PRESERVE	0x00001 // set if buffer content shouldn't be modified:
#define MP_IMGFLAG_READABLE	0x00002 // set if buffer content will be READ for next frame's MC: (I/P mpeg frames)

//--- buffer width/stride/plane restrictions: (used for direct rendering)
// stride _have_to_ be aligned to MB boundary:  [for DR restrictions]
#define MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE 0x00004 /* no flag - should be neg value of MP_IMGFLAG_ACCEPT_STRIDE */
// stride should be aligned to MB boundary:     [for buffer allocation]
#define MP_IMGFLAG_PREFER_ALIGNED_STRIDE 0x00008 /* sould be no flag - everything prefer aligned strides */
#define MP_IMGFLAG_ACCEPT_STRIDE	0x00010 // codec accept any stride (>=width):
#define MP_IMGFLAG_ACCEPT_WIDTH		0x00020 // codec accept any width (width*bpp=stride -> stride%bpp==0) (>=width):
//--- for planar formats only:
// uses only stride[0], and stride[1]=stride[2]=stride[0]>>mpi->chroma_x_shift
#define MP_IMGFLAG_COMMON_STRIDE	0x00040 /* UNUSED */
// uses only planes[0], and calculates planes[1,2] from width,height,imgfmt
#define MP_IMGFLAG_COMMON_PLANE		0x00080 /* UNUSED */
#define MP_IMGFLAGMASK_RESTRICTIONS	0x000FF

//--------- color info (filled by mp_image_setfmt() ) -----------
#define MP_IMGFLAG_PLANAR		0x00100 // set if number of planes > 1
#define MP_IMGFLAG_YUV			0x00200 // set if it's YUV colorspace
#define MP_IMGFLAG_SWAPPED		0x00400 // set if it's swapped (BGR or YVU) plane/byteorder
#define MP_IMGFLAG_RGB_PALETTE		0x00800 // using palette for RGB data
#define MP_IMGFLAGMASK_COLORS		0x00F00

// codec uses drawing/rendering callbacks (draw_slice()-like thing, DR method 2)
// [the codec will set this flag if it supports callbacks, and the vo _may_
//  clear it in get_image() if draw_slice() not implemented]
#define MP_IMGFLAG_DRAW_CALLBACK	0x01000
#define MP_IMGFLAG_DIRECT		0x02000 // set if it's in video buffer/memory: [set by vo/vf's get_image() !!!]
#define MP_IMGFLAG_ALLOCATED		0x04000 // set if buffer is allocated (used in destination images):
#define MP_IMGFLAG_TYPE_DISPLAYED	0x08000 // buffer type was printed (do NOT set this flag - it's for INTERNAL USE!!!)
#define MP_IMGFLAG_FINAL		0x10000 // buffer is video memory
#define MP_IMGFLAG_RENDERED		0x20000 // final buffer was already painted
#define MP_IMGFLAG_FINALIZED		0x40000 // indicates final step of image processing from CPU side!!!

/* codec doesn't support any form of direct rendering - it has own buffer */
#define MP_IMGTYPE_EXPORT		0 // allocation. so we just export its buffer pointers:
#define MP_IMGTYPE_STATIC		1 // codec requires a static WO buffer, but it does only partial updates later:
#define MP_IMGTYPE_TEMP			2 // codec just needs some WO memory, where it writes/copies the whole frame to:
#define MP_IMGTYPE_IP			3 // I+P type, requires 2+ independent static R/W buffers
#define MP_IMGTYPE_IPB			4 // I+P+B type, requires 2+ independent static R/W and 1+ temp WO buffers

#define MP_MAX_PLANES			4

#define MP_IMGFIELD_ORDERED		0x01
#define MP_IMGFIELD_TOP_FIRST		0x02
#define MP_IMGFIELD_REPEAT_FIRST	0x04
#define MP_IMGFIELD_TOP			0x08
#define MP_IMGFIELD_BOTTOM		0x10
#define MP_IMGFIELD_INTERLACED		0x20

#include <stdlib.h>
#include <limits.h>

#define XP_IDX_INVALID	UINT_MAX
typedef struct mp_image_s {
    unsigned		xp_idx; /* index of xp_frame associated with this image */
    unsigned int	flags;
    unsigned char	type;
    unsigned char	bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
    unsigned int	imgfmt;
    unsigned		width,height;  // stored dimensions
    int			x,y,w,h;  // slice dimensions
    unsigned		num_planes;
    unsigned char*	planes[MP_MAX_PLANES];
    unsigned int	stride[MP_MAX_PLANES];
    char *		qscale;
    unsigned		qstride;
    unsigned		qscale_type; // 0->mpeg1/4/h263, 1->mpeg2
    unsigned		pict_type; // 0->unknown, 1->I, 2->P, 3->B
    unsigned		fields;
    /* these are only used by planar formats Y,U(Cb),V(Cr) */
    int			chroma_width;
    int			chroma_height;
    int			chroma_x_shift; // horizontal
    int			chroma_y_shift; // vertical
    any_t*		priv; /* for private use by filter or vo driver (to store buffer id or dmpi) */
} mp_image_t;

extern void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt);
extern mp_image_t* new_mp_image(unsigned w,unsigned h,unsigned xp_idx);
extern void free_mp_image(mp_image_t* mpi);
extern mp_image_t* alloc_mpi(unsigned w, unsigned h, unsigned int fmt,unsigned xp_idx);
extern void mpi_alloc_planes(mp_image_t *mpi);
extern void copy_mpi(mp_image_t *dmpi,const mp_image_t *mpi);
extern void mpi_fake_slice(mp_image_t *dmpi,const mp_image_t *mpi,unsigned y,unsigned height);

#endif
