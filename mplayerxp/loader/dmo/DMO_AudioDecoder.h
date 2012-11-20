#ifndef AVIFILE_DMO_AUDIODECODER_H
#define AVIFILE_DMO_AUDIODECODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DMO_AudioDecoder DMO_AudioDecoder;

//DMO_AudioDecoder * DMO_AudioDecoder_Create(const CodecInfo * info, const WAVEFORMATEX* wf);
DMO_AudioDecoder * DMO_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf,int out_channels);

void DMO_AudioDecoder_Destroy(DMO_AudioDecoder *self);

int DMO_AudioDecoder_Convert(DMO_AudioDecoder *self, const any_t* in_data, unsigned int in_size,
			     any_t* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written);

int DMO_AudioDecoder_GetSrcSize(DMO_AudioDecoder *self, int dest_size);

#ifdef __cplusplus
}
#endif
#endif // AVIFILE_DMO_AUDIODECODER_H
