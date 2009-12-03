// this code is based on a52dec/libao/audio_out_oss.c

static float bias,level;

static inline int16_t convert (int32_t i)
{
    if (i > 0x43c07fff)
	return 32767;
    else if (i < 0x43bf8000)
	return -32768;
    else
	return i - 0x43c00000;
}

static int dca_resample_MONO_to_5_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[5*i] = s16[5*i+1] = s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i]);
	}
    return 5*256;
}

static int dca_resample_MONO_to_1_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[i] = convert (f[i]);
	}
    return 1*256;
}

static int dca_resample_STEREO_to_2_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[2*i] = convert (f[i]);
	    s16[2*i+1] = convert (f[i+256]);
	}
    return 2*256;
}

static int dca_resample_3F_to_5_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i+256]);
	}
    return 5*256;
}

static int dca_resample_2F_2R_to_4_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[4*i] = convert (f[i]);
	    s16[4*i+1] = convert (f[i+256]);
	    s16[4*i+2] = convert (f[i+512]);
	    s16[4*i+3] = convert (f[i+768]);
	}
    return 4*256;
}

static int dca_resample_3F_2R_to_5_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = convert (f[i+768]);
	    s16[5*i+3] = convert (f[i+1024]);
	    s16[5*i+4] = convert (f[i+256]);
	}
    return 5*256;
}

static int dca_resample_MONO_LFE_to_6_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[6*i] = s16[6*i+1] = s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+256]);
	    s16[6*i+5] = convert (f[i]);
	}
    return 6*256;
}

static int dca_resample_STEREO_LFE_to_6_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = s16[6*i+3] = s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
    return 6*256;
}

static int dca_resample_3F_LFE_to_6_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
    return 6*256;
}

static int dca_resample_2F_2R_LFE_to_6_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = convert (f[i+768]);
	    s16[6*i+3] = convert (f[i+1024]);
	    s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
    return 6*256;
}

static int dca_resample_3F_2R_LFE_to_6_C(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = convert (f[i+1024]);
	    s16[6*i+3] = convert (f[i+1280]);
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
    return 6*256;
}


static void* dca_resample_C(dca_state_t * state,int flags, int ch){
    bias=state->bias;
    level=state->level;
    switch (flags) {
    case DCA_MONO:
	if(ch==5) return dca_resample_MONO_to_5_C;
	if(ch==1) return dca_resample_MONO_to_1_C;
	break;
    case DCA_CHANNEL:
    case DCA_STEREO:
    case DCA_DOLBY:
	if(ch==2) return dca_resample_STEREO_to_2_C;
	break;
    case DCA_3F:
	if(ch==5) return dca_resample_3F_to_5_C;
	break;
    case DCA_2F2R:
	if(ch==4) return dca_resample_2F_2R_to_4_C;
	break;
    case DCA_3F2R:
	if(ch==5) return dca_resample_3F_2R_to_5_C;
	break;
    case DCA_MONO | DCA_LFE:
	if(ch==6) return dca_resample_MONO_LFE_to_6_C;
	break;
    case DCA_CHANNEL | DCA_LFE:
    case DCA_STEREO | DCA_LFE:
    case DCA_DOLBY | DCA_LFE:
	if(ch==6) return dca_resample_STEREO_LFE_to_6_C;
	break;
    case DCA_3F | DCA_LFE:
	if(ch==6) return dca_resample_3F_LFE_to_6_C;
	break;
    case DCA_2F2R | DCA_LFE:
	if(ch==6) return dca_resample_2F_2R_LFE_to_6_C;
	break;
    case DCA_3F2R | DCA_LFE:
	if(ch==6) return dca_resample_3F_2R_LFE_to_6_C;
	break;
    }
    return NULL;
}

typedef float real_t;

static inline real_t normalize_f (float i) 
{
    return  (i-bias)/level;
}

static int dca_resample_MONO_to_5_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[5*i] = s32[5*i+1] = s32[5*i+2] = s32[5*i+3] = 0;
	    s32[5*i+4] = normalize_f (f[i]);
	}
    return 5*256;
}

static int dca_resample_MONO_to_1_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[i] = normalize_f (f[i]);
	}
    return 1*256;
}

static int dca_resample_STEREO_to_2_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[2*i] = normalize_f (f[i]);
	    s32[2*i+1] = normalize_f (f[i+256]);
	}
    return 2*256;
}

static int dca_resample_3F_to_5_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[5*i] = normalize_f (f[i]);
	    s32[5*i+1] = normalize_f (f[i+512]);
	    s32[5*i+2] = s32[5*i+3] = 0;
	    s32[5*i+4] = normalize_f (f[i+256]);
	}
    return 5*256;
}

static int dca_resample_2F_2R_to_4_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[4*i] = normalize_f (f[i]);
	    s32[4*i+1] = normalize_f (f[i+256]);
	    s32[4*i+2] = normalize_f (f[i+512]);
	    s32[4*i+3] = normalize_f (f[i+768]);
	}
    return 4*256;
}

static int dca_resample_3F_2R_to_5_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[5*i] = normalize_f (f[i]);
	    s32[5*i+1] = normalize_f (f[i+512]);
	    s32[5*i+2] = normalize_f (f[i+768]);
	    s32[5*i+3] = normalize_f (f[i+1024]);
	    s32[5*i+4] = normalize_f (f[i+256]);
	}
    return 5*256;
}

static int dca_resample_MONO_LFE_to_6_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[6*i] = s32[6*i+1] = s32[6*i+2] = s32[6*i+3] = 0;
	    s32[6*i+4] = normalize_f (f[i+256]);
	    s32[6*i+5] = normalize_f (f[i]);
	}
    return 6*256;
}

static int dca_resample_STEREO_LFE_to_6_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[6*i] = normalize_f (f[i+256]);
	    s32[6*i+1] = normalize_f (f[i+512]);
	    s32[6*i+2] = s32[6*i+3] = s32[6*i+4] = 0;
	    s32[6*i+5] = normalize_f (f[i]);
	}
    return 6*256;
}

static int dca_resample_3F_LFE_to_6_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[6*i] = normalize_f (f[i+256]);
	    s32[6*i+1] = normalize_f (f[i+768]);
	    s32[6*i+2] = s32[6*i+3] = 0;
	    s32[6*i+4] = normalize_f (f[i+512]);
	    s32[6*i+5] = normalize_f (f[i]);
	}
    return 6*256;
}

static int dca_resample_2F_2R_LFE_to_6_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[6*i] = normalize_f (f[i+256]);
	    s32[6*i+1] = normalize_f (f[i+512]);
	    s32[6*i+2] = normalize_f (f[i+768]);
	    s32[6*i+3] = normalize_f (f[i+1024]);
	    s32[6*i+4] = 0;
	    s32[6*i+5] = normalize_f (f[i]);
	}
    return 6*256;
}

static int dca_resample_3F_2R_LFE_to_6_f32(float * f, real_t * s32){
    int i;
	for (i = 0; i < 256; i++) {
	    s32[6*i] = normalize_f (f[i+256]);
	    s32[6*i+1] = normalize_f (f[i+768]);
	    s32[6*i+2] = normalize_f (f[i+1024]);
	    s32[6*i+3] = normalize_f (f[i+1280]);
	    s32[6*i+4] = normalize_f (f[i+512]);
	    s32[6*i+5] = normalize_f (f[i]);
	}
    return 6*256;
}

static void* dca_resample_f32(dca_state_t * state,int flags, int ch){
    bias=state->bias;
    level=state->level;
    switch (flags) {
    case DCA_MONO:
	if(ch==5) return dca_resample_MONO_to_5_f32;
	if(ch==1) return dca_resample_MONO_to_1_f32;
	break;
    case DCA_CHANNEL:
    case DCA_STEREO:
    case DCA_DOLBY:
	if(ch==2) return dca_resample_STEREO_to_2_f32;
	break;
    case DCA_3F:
	if(ch==5) return dca_resample_3F_to_5_f32;
	break;
    case DCA_2F2R:
	if(ch==4) return dca_resample_2F_2R_to_4_f32;
	break;
    case DCA_3F2R:
	if(ch==5) return dca_resample_3F_2R_to_5_f32;
	break;
    case DCA_MONO | DCA_LFE:
	if(ch==6) return dca_resample_MONO_LFE_to_6_f32;
	break;
    case DCA_CHANNEL | DCA_LFE:
    case DCA_STEREO | DCA_LFE:
    case DCA_DOLBY | DCA_LFE:
	if(ch==6) return dca_resample_STEREO_LFE_to_6_f32;
	break;
    case DCA_3F | DCA_LFE:
	if(ch==6) return dca_resample_3F_LFE_to_6_f32;
	break;
    case DCA_2F2R | DCA_LFE:
	if(ch==6) return dca_resample_2F_2R_LFE_to_6_f32;
	break;
    case DCA_3F2R | DCA_LFE:
	if(ch==6) return dca_resample_3F_2R_LFE_to_6_f32;
	break;
    }
    return NULL;
}
