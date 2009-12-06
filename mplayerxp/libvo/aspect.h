#ifndef __ASPECT_H
#define __ASPECT_H
/* Stuff for correct aspect scaling. */
#include "../mp_config.h"

void __FASTCALL__ aspect_save_orig(int orgw, int orgh);
void __FASTCALL__ aspect_save_prescale(int prew, int preh);
void __FASTCALL__ aspect_save_screenres(int scrw, int scrh);

#define A_ZOOM 1
#define A_NOZOOM 0

void __FASTCALL__ aspect(int *srcw, int *srch, int zoom);

#endif

