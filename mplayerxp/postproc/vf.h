#ifndef __VF_H_INCLUDED
#define __VF_H_INCLUDED 1
#include <stdint.h>
#include "xmpcore/xmp_enums.h"
#include "xmpcore/xmp_image.h"
#include "libvo2/video_out.h" // for vo_flags_e
#include "vfcap.h"

struct vf_instance_t;
namespace mpxp {
    enum {
	VF_FLAGS_THREADS	=0x00000001UL, /**< Thread safe plugin (requires to be applied within of threads) */
	VF_FLAGS_OSD	=0x00000002UL, /**< requires to be applied during page flipping */
	VF_FLAGS_SLICES	=0x00000004UL /**< really can draw slices (requires to be applied on SMP etc) */
    };

    enum {
	VFCTRL_QUERY_MAX_PP_LEVEL=4, /* test for postprocessing support (max level) */
	VFCTRL_SET_PP_LEVEL	=5, /* set postprocessing level */
	VFCTRL_SET_EQUALIZER	=6, /* set color options (brightness,contrast etc) */
	VFCTRL_GET_EQUALIZER	=8, /* gset color options (brightness,contrast etc) */
	VFCTRL_START_FRAME	=7,
	VFCTRL_CHANGE_RECTANGLE	=9, /* Change the rectangle boundaries */
	VFCTRL_RESERVED		=10, /* Tell the vo to flip pages */
	VFCTRL_DUPLICATE_FRAME	=11, /* For encoding - encode zero-change frame */
	VFCTRL_SKIP_NEXT_FRAME	=12, /* For encoding - drop the next frame that passes thru */
	VFCTRL_FLUSH_PAGES	=13 /* For encoding - flush delayed frames */
    };

    struct vf_equalizer_t {
	const char *item;
	int value;
    };

    // Configuration switches
    struct vf_cfg_t{
	int force;	// Initialization type
	char* list;	/* list of names of filters that are added to filter
		   list during first initialization of stream */
    };
    extern vf_cfg_t vf_cfg; // Configuration for audio filters

    struct vf_conf_t {
	unsigned	w;
	unsigned	h;
	uint32_t	fourcc;
    };

    struct libinput_t;
    struct vf_stream_t {
	vf_stream_t(libinput_t& _libinput):libinput(_libinput) {}
	~vf_stream_t() {}

	vf_instance_t*	first;
	libinput_t&	libinput;
    };

    vf_stream_t*	__FASTCALL__ vf_init(libinput_t& libinput,const vf_conf_t* conf);
    void		__FASTCALL__ vf_uninit(vf_stream_t* s);
    void		__FASTCALL__ vf_reinit_vo(vf_stream_t* s,unsigned w,unsigned h,unsigned fmt,int reset_cache);

    void		__FASTCALL__ vf_showlist(vf_stream_t* s);
    void			     vf_help();

    int			__FASTCALL__ vf_query_flags(vf_stream_t* s);
    int			__FASTCALL__ vf_config(vf_stream_t* s,
				int width, int height, int d_width, int d_height,
				vo_flags_e flags, unsigned int outfmt);
    int			__FASTCALL__ vf_query_format(vf_stream_t* s,unsigned int fmt,unsigned w,unsigned h);
    void		__FASTCALL__ vf_get_image(vf_stream_t* s,mp_image_t *mpi);
    int			__FASTCALL__ vf_put_slice(vf_stream_t* s,mp_image_t *mpi);
    void		__FASTCALL__ vf_start_slice(vf_stream_t* s,mp_image_t *mpi);
    MPXP_Rc		__FASTCALL__ vf_control(vf_stream_t* s,int request, any_t* data);
    mp_image_t*		__FASTCALL__ vf_get_new_image(vf_stream_t* s, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h,unsigned idx);
    void		__FASTCALL__ vf_prepend_filter(vf_stream_t* s,const char *name,const vf_conf_t* conf,const char *args=NULL);
    void		__FASTCALL__ vf_remove_first(vf_stream_t* s);
    const char *	__FASTCALL__ vf_get_first_name(vf_stream_t* s);
} // namespace
#endif

