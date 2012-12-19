#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include "afmt.h"

const char* afmt2str(unsigned fmt) {
    const char* rs = "AFMT_Unknown";
    switch (fmt) {
	default: rs = "AFMT_Special"; break;
	case AFMT_U8: return "AFMT_U8";
	case AFMT_S8: return "AFMT_S8";
	case AFMT_S16_LE: return "AFMT_S16_LE";
	case AFMT_S16_BE: return "AFMT_S16_BE";
	case AFMT_U16_LE: return "AFMT_U16_LE";
	case AFMT_U16_BE: return "AFMT_U16_BE";
	case AFMT_S24_LE: return "AFMT_S24_LE";
	case AFMT_S24_BE: return "AFMT_S24_BE";
	case AFMT_U24_LE: return "AFMT_U24_LE";
	case AFMT_U24_BE: return "AFMT_U24_BE";
	case AFMT_FLOAT32:return "AFMT_FLOAT32";
	case AFMT_S32_LE: return "AFMT_S32_LE";
	case AFMT_S32_BE: return "AFMT_S32_BE";
	case AFMT_U32_LE: return "AFMT_U32_LE";
	case AFMT_U32_BE: return "AFMT_U32_BE";
    }
    return rs;
}

unsigned afmt2bps(unsigned fmt) {
    unsigned rc;
    switch (fmt) {
	default:
	case AFMT_U8:
	case AFMT_S8: rc = 1; break;
	case AFMT_S16_LE:
	case AFMT_S16_BE:
	case AFMT_U16_LE:
	case AFMT_U16_BE: return 2;
	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE: return 3;
	case AFMT_FLOAT32:
	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE: return 4;
    }
    return rc;
}

/* very approximate prediction */
unsigned bps2afmt(unsigned bps) {
    unsigned rc;
    switch (bps) {
	default:
	case 1: rc = AFMT_S8; break;
	case 2: return AFMT_S16_LE; break;
	case 3: return AFMT_S24_LE; break;
	case 4: return AFMT_S32_LE; break;
    }
    return rc;
}