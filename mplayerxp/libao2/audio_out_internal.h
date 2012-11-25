#include "mp_config.h"
// prototypes:
//static ao_info_t info;
static MPXP_Rc __FASTCALL__ control_ao(const ao_data_t*,int cmd,long arg);
static MPXP_Rc __FASTCALL__ init(ao_data_t*,unsigned flags);
static MPXP_Rc __FASTCALL__ config_ao(ao_data_t*,unsigned rate,unsigned channels,unsigned format);
static void __FASTCALL__ uninit(ao_data_t*);
static void __FASTCALL__ reset(ao_data_t*);
static unsigned __FASTCALL__ get_space(const ao_data_t*);
static unsigned __FASTCALL__ play(ao_data_t*,const any_t* data,unsigned len,unsigned flags);
static float __FASTCALL__ get_delay(const ao_data_t*);
static void __FASTCALL__ audio_pause(ao_data_t*);
static void __FASTCALL__ audio_resume(ao_data_t*);

#define LIBAO_EXTERN(x) extern const ao_functions_t audio_out_##x =\
{\
	&info,\
	control_ao,\
	init,\
	config_ao,\
	uninit,\
	reset,\
	get_space,\
	play,\
	get_delay,\
	audio_pause,\
	audio_resume\
};

