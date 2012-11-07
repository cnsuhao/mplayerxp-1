#include "mp_aframe.h"
#include "osdep/mplib.h"

mp_aframe_t* new_mp_aframe(unsigned rate,unsigned nch,unsigned format,unsigned xp_idx) {
    mp_aframe_t*  mpaf = mp_mallocz(sizeof(mp_aframe_t));
    if(!mpaf) return NULL;
    mpaf->rate = rate;
    mpaf->nch = nch;
    mpaf->format = format;
    mpaf->xp_idx = xp_idx;
    return mpaf;
}

int free_mp_aframe(mp_aframe_t* mpaf) {
    if(!mpaf) return 0;
    if(mpaf->audio) mp_free(mpaf->audio);
    mp_free(mpaf);
    return 1;
}
