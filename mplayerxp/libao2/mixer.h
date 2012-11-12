#ifndef __MPLAYER_MIXER
#define __MPLAYER_MIXER
#include "libao2/audio_out.h"

extern void mixer_getvolume(ao_data_t* ao,float *l,float *r );
extern void mixer_setvolume(ao_data_t* ao,float l,float r );
extern void mixer_incvolume(ao_data_t* ao);
extern void mixer_decvolume(ao_data_t* ao);
extern float mixer_getbothvolume(ao_data_t* ao);
void mixer_mute(ao_data_t* ao);

//extern void mixer_setbothvolume( int v );
static inline void mixer_setbothvolume(ao_data_t* ao, float v ) { mixer_setvolume(ao,v,v); }

#endif
