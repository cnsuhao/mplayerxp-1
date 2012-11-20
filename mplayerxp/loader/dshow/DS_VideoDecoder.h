#ifndef AVIFILE_DS_VIDEODECODER_H
#define AVIFILE_DS_VIDEODECODER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DS_VideoDecoder DS_VideoDecoder;

int DS_VideoDecoder_GetCapabilities(DS_VideoDecoder *self);

DS_VideoDecoder * DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER * format, int flip, int maxauto);

void DS_VideoDecoder_Destroy(DS_VideoDecoder *self);

void DS_VideoDecoder_StartInternal(DS_VideoDecoder *self);

void DS_VideoDecoder_StopInternal(DS_VideoDecoder *self);

int DS_VideoDecoder_DecodeInternal(DS_VideoDecoder *self, const any_t* src, int size, int is_keyframe, char* pImage);

/*
 * bits == 0   - leave unchanged
 */
//int SetDestFmt(DS_VideoDecoder * self, int bits = 24, fourcc_t csp = 0);
int DS_VideoDecoder_SetDestFmt(DS_VideoDecoder *self, int bits, unsigned int csp);
int DS_VideoDecoder_SetDirection(DS_VideoDecoder *self, int d);
int DS_VideoDecoder_GetValue(DS_VideoDecoder *self, const char* name, int* value);
int DS_VideoDecoder_SetValue(DS_VideoDecoder *self, const char* name, int value);
int DS_SetAttr_DivX(const char* attribute, int value);

#ifdef __cplusplus
}
#endif
#endif /* AVIFILE_DS_VIDEODECODER_H */
