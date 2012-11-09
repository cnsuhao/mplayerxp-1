#ifndef __VF_H
#define __VF_H 1
#include "xmpcore/xmp_enums.h"
#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

struct vf_instance_s;
struct vf_priv_s;

enum {
    VF_FLAGS_THREADS	=0x00000001UL, /**< Thread safe plugin (requires to be applied within of threads) */
    VF_FLAGS_OSD	=0x00000002UL, /**< requires to be applied during page flipping */
    VF_FLAGS_SLICES	=0x00000004UL /**< really can draw slices (requires to be applied on SMP etc) */
};

typedef struct vf_info_s {
    const char *info;
    const char *name;
    const char *author;
    const char *comment;
    const unsigned flags;
    MPXP_Rc (* __FASTCALL__ open)(struct vf_instance_s* vf,const char* args);
} vf_info_t;

typedef struct vf_image_context_s {
    mp_image_t* static_images[2];
    mp_image_t* temp_images[1];
    mp_image_t* export_images[1];
    int static_idx;
} vf_image_context_t;

typedef struct vf_instance_s {
    const vf_info_t* info;
    // funcs:
    int (* __FASTCALL__ config)(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune);
    MPXP_Rc (* __FASTCALL__ control)(struct vf_instance_s* vf,
        int request, any_t* data);
    int (* __FASTCALL__ query_format)(struct vf_instance_s* vf,
        unsigned int fmt,unsigned w,unsigned h);
    void (* __FASTCALL__ get_image)(struct vf_instance_s* vf,
        mp_image_t *mpi);
    int (* __FASTCALL__ put_slice)(struct vf_instance_s* vf,
        mp_image_t *mpi);
    void (* __FASTCALL__ start_slice)(struct vf_instance_s* vf,
        mp_image_t *mpi);
    void (* __FASTCALL__ uninit)(struct vf_instance_s* vf);
    // optional: maybe NULL
    void (* __FASTCALL__ print_conf)(struct vf_instance_s* vf);
    // caps:
    unsigned int default_caps; // used by default query_format()
    unsigned int default_reqs; // used by default config()
    // data:
    vf_image_context_t imgctx;
    struct vf_instance_s* next;
    struct vf_instance_s* prev;
    mp_image_t *dmpi;
    struct vf_priv_s* priv;
    sh_video_t *sh;
    /* configuration for outgoing stream */
    unsigned dw,dh,dfourcc;
} vf_instance_t;

// Configuration switches
typedef struct vf_cfg_s{
  int force;	// Initialization type
  char* list;	/* list of names of filters that are added to filter
		   list during first initialization of stream */
}vf_cfg_t;

// control codes:
#include "mpc_info.h"

typedef struct vf_seteq_s
{
    char *item;
    int value;
} vf_equalizer_t;

enum {
    VFCTRL_QUERY_MAX_PP_LEVEL	=4, /* test for postprocessing support (max level) */
    VFCTRL_SET_PP_LEVEL		=5, /* set postprocessing level */
    VFCTRL_SET_EQUALIZER	=6, /* set color options (brightness,contrast etc) */
    VFCTRL_GET_EQUALIZER	=8, /* gset color options (brightness,contrast etc) */
    VFCTRL_START_FRAME		=7,
    VFCTRL_CHANGE_RECTANGLE	=9, /* Change the rectangle boundaries */
    VFCTRL_SELECT_FRAME		=10, /* Tell the vo to flip pages */
    VFCTRL_DUPLICATE_FRAME	=11, /* For encoding - encode zero-change frame */
    VFCTRL_SKIP_NEXT_FRAME	=12, /* For encoding - drop the next frame that passes thru */
    VFCTRL_FLUSH_PAGES		=13 /* For encoding - flush delayed frames */
};
#include "vfcap.h"

// functions:

vf_instance_t* __FASTCALL__ vf_init(sh_video_t *sh);
void __FASTCALL__ vf_reinit_vo(unsigned w,unsigned h,unsigned fmt,int reset_cache);

void __FASTCALL__ vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h);
mp_image_t* __FASTCALL__ vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h,unsigned idx);
int __FASTCALL__ vf_query_format(vf_instance_t* vf, unsigned int fmt,unsigned width,unsigned height);

vf_instance_t* __FASTCALL__ vf_open_plugin(const vf_info_t** filter_list, vf_instance_t* next,sh_video_t *sh, char *name, char *args);
vf_instance_t* __FASTCALL__ vf_open_filter(vf_instance_t* next,sh_video_t *sh,char *name, char *args);
vf_instance_t* __FASTCALL__ vf_open_encoder(vf_instance_t* next, char *name, char *args);

unsigned int __FASTCALL__ vf_match_csp(vf_instance_t** vfp,unsigned int* list,unsigned int preferred,unsigned w,unsigned h);
void __FASTCALL__ vf_clone_mpi_attributes(mp_image_t* dst, mp_image_t* src);

// default wrappers:
int __FASTCALL__ vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune);
MPXP_Rc __FASTCALL__ vf_next_control(struct vf_instance_s* vf, int request, any_t* data);
int __FASTCALL__ vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h);
int __FASTCALL__ vf_next_put_slice(struct vf_instance_s* vf,mp_image_t *mpi);

vf_instance_t* __FASTCALL__ append_filters(vf_instance_t* last);

/* returns ANDed flags of whole chain */
unsigned __FASTCALL__ vf_query_flags(vf_instance_t*vfi);

void vf_help();

void __FASTCALL__ vf_uninit_filter(vf_instance_t* vf);
void __FASTCALL__ vf_uninit_filter_chain(vf_instance_t* vf);
void __FASTCALL__ vf_showlist(vf_instance_t* vf);

#endif
