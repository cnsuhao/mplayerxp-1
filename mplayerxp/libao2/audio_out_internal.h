#include "../mp_config.h"
// prototypes:
//static ao_info_t info;
static int __FASTCALL__ control(int cmd,long arg);
static int __FASTCALL__ init(unsigned flags);
static int __FASTCALL__ configure(unsigned rate,unsigned channels,unsigned format);
static void uninit(void);
static void reset(void);
static unsigned get_space(void);
static unsigned __FASTCALL__ play(void* data,unsigned len,unsigned flags);
static float get_delay(void);
static void audio_pause(void);
static void audio_resume(void);

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

