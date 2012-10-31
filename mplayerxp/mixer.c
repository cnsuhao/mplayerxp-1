#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "mp_config.h"
#include "mixer.h"
#include "libao2/audio_out.h"

void mixer_getvolume(ao_data_t* ao, float *l,float *r )
{
  ao_control_vol_t vol;
  *l=0; *r=0;
  if(CONTROL_OK != ao_control(ao,AOCONTROL_GET_VOLUME,(long)&vol)) return;
  *r=vol.right;
  *l=vol.left;
}

void mixer_setvolume(ao_data_t* ao,float l,float r )
{
  ao_control_vol_t vol;
  vol.right=r; vol.left=l;
  ao_control(ao,AOCONTROL_SET_VOLUME,(long)&vol);
}

#define MIXER_CHANGE 3

void mixer_incvolume(ao_data_t* ao)
{
 float mixer_l, mixer_r;
 mixer_getvolume(ao, &mixer_l,&mixer_r );
 mixer_l += MIXER_CHANGE;
 if ( mixer_l > 100 ) mixer_l = 100;
 mixer_r += MIXER_CHANGE;
 if ( mixer_r > 100 ) mixer_r = 100;
 mixer_setvolume(ao, mixer_l,mixer_r );
}

void mixer_decvolume(ao_data_t* ao)
{
 float mixer_l, mixer_r;
 mixer_getvolume(ao, &mixer_l,&mixer_r );
 mixer_l -= MIXER_CHANGE;
 if ( mixer_l < 0 ) mixer_l = 0;
 mixer_r -= MIXER_CHANGE;
 if ( mixer_r < 0 ) mixer_r = 0;
 mixer_setvolume(ao, mixer_l,mixer_r );
}

float mixer_getbothvolume(ao_data_t* ao)
{
 float mixer_l, mixer_r;
 mixer_getvolume(ao, &mixer_l,&mixer_r );
 return ( mixer_l + mixer_r ) / 2;
}

static int muted=0;
static float mute_l,mute_r;
void mixer_mute(ao_data_t* ao)
{
 if ( muted ) { mixer_setvolume(ao, mute_l,mute_r ); muted=0; }
  else
   {
    mixer_getvolume(ao, &mute_l,&mute_r );
    mixer_setvolume(ao, 0,0 );
    muted=1;
   }
}
