#include "../config.h"
// prototypes:
//static ao_info_t info;
static int __FASTCALL__ control(int cmd,long arg);
static int __FASTCALL__ init(int flags);
static int __FASTCALL__ configure(int rate,int channels,int format);
static void uninit(void);
static void reset(void);
static int get_space(void);
static int __FASTCALL__ play(void* data,int len,int flags);
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

