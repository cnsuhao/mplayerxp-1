#include "../mp_config.h"
// prototypes:
//static ao_info_t info;
static int __FASTCALL__ control(ao_data_t*,int cmd,long arg);
static int __FASTCALL__ init(ao_data_t*,unsigned flags);
static int __FASTCALL__ configure(ao_data_t*,unsigned rate,unsigned channels,unsigned format);
static void uninit(ao_data_t*);
static void reset(ao_data_t*);
static unsigned get_space(ao_data_t*);
static unsigned __FASTCALL__ play(ao_data_t*,any_t* data,unsigned len,unsigned flags);
static float get_delay(ao_data_t*);
static void audio_pause(ao_data_t*);
static void audio_resume(ao_data_t*);

#define LIBAO_EXTERN(x) const ao_functions_t audio_out_##x =\
{\
	&info,\
	control,\
	init,\
	configure,\
        uninit,\
	reset,\
	get_space,\
	play,\
	get_delay,\
	audio_pause,\
	audio_resume\
};

