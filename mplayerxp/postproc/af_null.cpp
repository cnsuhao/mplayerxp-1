#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include "af.h"
#include "af_internal.h"

// Allocate memory and set function pointers
static MPXP_Rc af_open(af_instance_t* af){
    return MPXP_Error;
}

// Description of this filter
extern const af_info_t af_info_null = {
    "Null audio filter",
    "null",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
