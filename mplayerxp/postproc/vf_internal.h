#ifndef __VF_H
#define __VF_H 1
#include "vf.h"

struct vf_instance_t;
struct vf_priv_t;

struct vf_info_t {
    const char *info;
    const char *name;
    const char *author;
    const char *comment;
    const unsigned flags;
    MPXP_Rc (* __FASTCALL__ open)(vf_instance_t* vf,const char* args);
};

struct vf_image_context_t {
    unsigned char* static_planes[2];
    int static_idx;
};

enum {
    VF_PIN=RND_NUMBER7+RND_CHAR7
};

typedef int (* __FASTCALL__ vf_config_fun_t)(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt);
struct vf_instance_t {
    vf_instance_t(libinput_t& _libinput):libinput(_libinput) {}
    ~vf_instance_t() {}

    const vf_info_t*	info;
    char		antiviral_hole[RND_CHAR5];
    unsigned		pin; // personal identification number
    // funcs:
    int (* __FASTCALL__ config_vf)(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt);
    MPXP_Rc (* __FASTCALL__ control_vf)(vf_instance_t* vf,
	int request, any_t* data);
    int (* __FASTCALL__ query_format)(vf_instance_t* vf,
	unsigned int fmt,unsigned w,unsigned h);
    void (* __FASTCALL__ get_image)(vf_instance_t* vf,
	mp_image_t *mpi);
    int (* __FASTCALL__ put_slice)(vf_instance_t* vf,
	mp_image_t *mpi);
    void (* __FASTCALL__ start_slice)(vf_instance_t* vf,
	mp_image_t *mpi);
    void (* __FASTCALL__ uninit)(vf_instance_t* vf);
    // optional: maybe NULL
    void (* __FASTCALL__ print_conf)(vf_instance_t* vf);
    // caps:
    unsigned int default_caps; // used by default query_format()
    unsigned int default_reqs; // used by default config()
    // data:
    vf_image_context_t imgctx;
    vf_stream_t* parent;
    vf_instance_t* next;
    vf_instance_t* prev;
    mp_image_t *dmpi;
    vf_priv_t* priv;
    /* configuration for outgoing stream */
    vf_conf_t	conf;
    /* event handler*/
    libinput_t&	libinput;
};

// control codes:
#include "mpc_info.h"

// functions:

vf_instance_t* __FASTCALL__ vf_init_filter(libinput_t& libinput,const vf_conf_t* conf);
vf_instance_t* __FASTCALL__ vf_reinit_vo(vf_instance_t* first,unsigned w,unsigned h,unsigned fmt,int reset_cache);

void __FASTCALL__ vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h);
mp_image_t* __FASTCALL__ vf_get_new_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h,unsigned idx);
mp_image_t* __FASTCALL__ vf_get_new_genome(vf_instance_t* vf, const mp_image_t* mpi);
mp_image_t* __FASTCALL__ vf_get_new_exportable_genome(vf_instance_t* vf, int mp_imgtype, int mp_imgflag, const mp_image_t* mpi);
mp_image_t* __FASTCALL__ vf_get_new_temp_genome(vf_instance_t* vf, const mp_image_t* mpi);
int __FASTCALL__ vf_query_format(vf_instance_t* vf, unsigned int fmt,unsigned width,unsigned height);

vf_instance_t* __FASTCALL__ vf_open_filter(vf_instance_t* next,const char *name,const char *args,libinput_t&libinput,const vf_conf_t* conf);
vf_instance_t* __FASTCALL__ vf_open_encoder(vf_instance_t* next,const char *name,const char *args);

unsigned int __FASTCALL__ vf_match_csp(vf_instance_t** vfp,unsigned int* list,const vf_conf_t* conf);
void __FASTCALL__ vf_clone_mpi_attributes(mp_image_t* dst, mp_image_t* src);

// default wrappers:
int __FASTCALL__ vf_next_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt);
MPXP_Rc __FASTCALL__ vf_next_control(vf_instance_t* vf, int request, any_t* data);
int __FASTCALL__ vf_next_query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h);
int __FASTCALL__ vf_next_put_slice(vf_instance_t* vf,mp_image_t *mpi);

vf_instance_t* __FASTCALL__ append_filters(vf_instance_t* last);

/* returns ANDed flags of whole chain */
unsigned __FASTCALL__ vf_query_flags(vf_instance_t*vfi);

void __FASTCALL__ vf_uninit_filter(vf_instance_t* vf);
void __FASTCALL__ vf_uninit_filter_chain(vf_instance_t* vf);
void __FASTCALL__ vf_showlist(vf_instance_t* vf);

vf_instance_t* __FASTCALL__ vf_first(const vf_instance_t*);
vf_instance_t* __FASTCALL__ vf_last(const vf_instance_t*);

#endif
