#ifndef __ASPECT_H
#define __ASPECT_H
/* Stuff for correct aspect scaling. */
#include <stdint.h>
#include "../mp_config.h"

void __FASTCALL__ aspect_save_orig(uint32_t orgw, uint32_t orgh);
void __FASTCALL__ aspect_save_prescale(uint32_t prew, uint32_t preh);
void __FASTCALL__ aspect_save_screenres(uint32_t scrw, uint32_t scrh);

enum {
    A_ZOOM	=1,
    A_NOZOOM	=0
};
void __FASTCALL__ aspect(uint32_t *srcw, uint32_t *srch, int zoom);

#endif

