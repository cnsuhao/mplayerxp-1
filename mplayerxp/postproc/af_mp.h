/* Include file for mplayer specific defines and includes */
#ifndef __af_mp_h__
#define __af_mp_h__

#include "../mp_config.h"
#include "pp_msg.h"
#include "../cpudetect.h"
#include "../libao2/afmt.h"

/* Set the initialization type from mplayers cpudetect */
#ifdef AF_INIT_TYPE
#undef AF_INIT_TYPE
#define AF_INIT_TYPE \
  ((gCpuCaps.has3DNow || gCpuCaps.hasSSE)?AF_INIT_FAST:AF_INIT_SLOW)
#endif 

/* Decodes the format from mplayer format to libaf format */
extern int __FASTCALL__ af_format_decode(int format,unsigned *bps);
extern int __FASTCALL__ af_format_encode(any_t* fmt);

#endif /* __af_mp_h__ */
