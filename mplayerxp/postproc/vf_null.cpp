#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include "vf.h"
#include "vf_internal.h"

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    return MPXP_False;
}

extern const vf_info_t vf_info_null = {
    "null filter",
    "null",
    "Nickols_K",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
