#include "../config.h"
#include "mpg123.h"

#undef HAVE_3DNOW
#if defined ( CAN_COMPILE_X86_ASM )
#define HAVE_3DNOW
#include "decode_3dnow.h"
#endif
