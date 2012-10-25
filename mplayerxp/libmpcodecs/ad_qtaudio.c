#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <dlfcn.h>
#include "mp_config.h"

#include "ad_internal.h"
#include "bswap.h"
#include "codecs_ld.h"
#include "../mplayer.h"
#ifdef WIN32_LOADER
#include "../../loader/ldt_keeper.h"
#endif

#ifdef MACOSX
#include <QuickTime/QuickTimeComponents.h>
#endif

static const ad_info_t info =  {
	"QuickTime Audio Decoder",
	"qtaudio",
	"A'rpi & Sascha Sommer",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(qtaudio)

typedef struct OpaqueSoundConverter*    SoundConverter;
typedef unsigned long                   OSType;
typedef unsigned long                   UnsignedFixed;
typedef uint8_t                          Byte;
typedef struct SoundComponentData {
    long                            flags;
    OSType                          format;
    short                           numChannels;
    short                           sampleSize;
    UnsignedFixed                   sampleRate;
    long                            sampleCount;
    Byte *                          buffer;
    long                            reserved;
}SoundComponentData;

typedef int (__cdecl* LPFUNC1)(long flag);
typedef int (__cdecl* LPFUNC2)(const SoundComponentData *, const SoundComponentData *,SoundConverter *);
typedef int (__cdecl* LPFUNC3)(SoundConverter sc);
typedef int (__cdecl* LPFUNC4)(void);
typedef int (__cdecl* LPFUNC5)(SoundConverter sc, OSType selector,any_t* infoPtr);
typedef int (__cdecl* LPFUNC6)(SoundConverter sc,
				unsigned long inputBytesTarget,
				unsigned long *inputFrames,
				unsigned long *inputBytes,
				unsigned long *outputBytes );
typedef int (__cdecl* LPFUNC7)(SoundConverter sc,
				const any_t*inputPtr,
				unsigned long inputFrames,
				any_t*outputPtr,
				unsigned long *outputFrames,
				unsigned long *outputBytes );
typedef int (__cdecl* LPFUNC8)(SoundConverter sc,
				any_t*outputPtr,
                                unsigned long *outputFrames,
                                unsigned long *outputBytes);
typedef int (__cdecl* LPFUNC9)(SoundConverter         sc);

static HINSTANCE qtml_dll;
static LPFUNC1 InitializeQTML;
static LPFUNC2 SoundConverterOpen;
static LPFUNC3 SoundConverterClose;
static LPFUNC4 TerminateQTML;
static LPFUNC5 SoundConverterSetInfo;
static LPFUNC6 SoundConverterGetBufferSizes;
static LPFUNC7 SoundConverterConvertBuffer;
static LPFUNC8 SoundConverterEndConversion;
static LPFUNC9 SoundConverterBeginConversion;

#define siDecompressionParams 2002876005 // siDecompressionParams = FOUR_CHAR_CODE('wave')

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);

static int loader_init()
{
#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    qtml_dll = LoadLibraryA((LPCSTR)"qtmlClient.dll");
    if( qtml_dll == NULL )
    {
        MSG_ERR("failed loading dll\n" );
	return 1;
    }
#if 1
    InitializeQTML = (LPFUNC1)GetProcAddress(qtml_dll,(LPCSTR)"InitializeQTML");
	if ( InitializeQTML == NULL )
    {
        MSG_ERR("failed geting proc address InitializeQTML\n");
		return 1;
    }
    SoundConverterOpen = (LPFUNC2)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterOpen");
	if ( SoundConverterOpen == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterOpen\n");
		return 1;
    }
	SoundConverterClose = (LPFUNC3)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterClose");
	if ( SoundConverterClose == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterClose\n");
		return 1;
    }
	TerminateQTML = (LPFUNC4)GetProcAddress(qtml_dll,(LPCSTR)"TerminateQTML");
	if ( TerminateQTML == NULL )
    {
        MSG_ERR("failed getting proc address TerminateQTML\n");
		return 1;
    }
	SoundConverterSetInfo = (LPFUNC5)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterSetInfo");
	if ( SoundConverterSetInfo == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterSetInfo\n");
		return 1;
    }
	SoundConverterGetBufferSizes = (LPFUNC6)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterGetBufferSizes");
	if ( SoundConverterGetBufferSizes == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterGetBufferSizes\n");
		return 1;
    }
	SoundConverterConvertBuffer = (LPFUNC7)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterConvertBuffer");
	if ( SoundConverterConvertBuffer == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterConvertBuffer1\n");
		return 1;
    }
	SoundConverterEndConversion = (LPFUNC8)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterEndConversion");
	if ( SoundConverterEndConversion == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterEndConversion\n");
		return 1;
    }
	SoundConverterBeginConversion = (LPFUNC9)GetProcAddress(qtml_dll,(LPCSTR)"SoundConverterBeginConversion");
	if ( SoundConverterBeginConversion == NULL )
    {
        MSG_ERR("failed getting proc address SoundConverterBeginConversion\n");
		return 1;
    }
    MSG_V("Standard init done you may now call supported functions\n");
#endif
    MSG_V("loader_init DONE???\n");
    return 0;
}

static SoundConverter			myConverter = NULL;
static SoundComponentData		InputFormatInfo,OutputFormatInfo;

static int InFrameSize;
static int OutFrameSize;

static int preinit(sh_audio_t *sh){
    int error;
    unsigned long FramesToGet=0; //how many frames the demuxer has to get
    unsigned long InputBufferSize=0; //size of the input buffer
    unsigned long OutputBufferSize=0; //size of the output buffer
    unsigned long WantedBufferSize=0; //the size you want your buffers to be

    if(mp_conf.s_cache_size)
    {
	MSG_FATAL("Disabling sound:\nwin32 quicktime DLLs must be initialized in single-threaded mode! Try -nocache\n");
	return 0;
    }
    MSG_INFO("win32 libquicktime loader (c) Sascha Sommer\n");

#ifdef MACOSX
    EnterMovies();
#else
    if(loader_init()) return 0; // failed to load DLL

    MSG_V("loader_init DONE!\n");

    error = InitializeQTML(6+16);
    MSG_ERR("InitializeQTML:%i\n",error);
    if(error) return 0;
#endif

#if 1
	OutputFormatInfo.flags = InputFormatInfo.flags = 0;
	OutputFormatInfo.sampleCount = InputFormatInfo.sampleCount = 0;
	OutputFormatInfo.buffer = InputFormatInfo.buffer = NULL;
	OutputFormatInfo.reserved = InputFormatInfo.reserved = 0;
	OutputFormatInfo.numChannels = InputFormatInfo.numChannels = sh->wf->nChannels;
	InputFormatInfo.sampleSize = sh->wf->wBitsPerSample;
	OutputFormatInfo.sampleSize = 16;
	OutputFormatInfo.sampleRate = InputFormatInfo.sampleRate = sh->wf->nSamplesPerSec;
	InputFormatInfo.format =  bswap_32(sh->format); //1363430706;///*1768775988;//*/1902406962;//qdm2//1768775988;//FOUR_CHAR_CODE('ima4');
	OutputFormatInfo.format = 1313820229;// FOUR_CHAR_CODE('NONE');

    error = SoundConverterOpen(&InputFormatInfo, &OutputFormatInfo, &myConverter);
    MSG_V("SoundConverterOpen:%i\n",error);
    if(error) return 0;

    if(sh->codecdata){
	error = SoundConverterSetInfo(myConverter,siDecompressionParams,sh->codecdata);
	MSG_V("SoundConverterSetInfo:%i\n",error);
//	if(error) return 0;
    }

    WantedBufferSize=OutputFormatInfo.numChannels*OutputFormatInfo.sampleRate*2;
    error = SoundConverterGetBufferSizes(myConverter,
	WantedBufferSize,&FramesToGet,&InputBufferSize,&OutputBufferSize);
    MSG_V("SoundConverterGetBufferSizes:%i\n",error);
    MSG_V("WantedBufferSize = %li\n",WantedBufferSize);
    MSG_V("InputBufferSize  = %li\n",InputBufferSize);
    MSG_V("OutputBufferSize = %li\n",OutputBufferSize);
    MSG_V("FramesToGet = %li\n",FramesToGet);

    InFrameSize=(InputBufferSize+FramesToGet-1)/FramesToGet;
    OutFrameSize=OutputBufferSize/FramesToGet;

    MSG_V("FrameSize: %i -> %i\n",InFrameSize,OutFrameSize);

    error = SoundConverterBeginConversion(myConverter);
    MSG_V("SoundConverterBeginConversion:%i\n",error);
    if(error) return 0;

    sh->audio_out_minsize=OutputBufferSize;
    sh->audio_in_minsize=InputBufferSize;

    sh->channels=sh->wf->nChannels;
    sh->samplerate=sh->wf->nSamplesPerSec;
    sh->samplesize=2; //(sh->wf->wBitsPerSample+7)/8;

    sh->i_bps=sh->wf->nAvgBytesPerSec;
//InputBufferSize*WantedBufferSize/OutputBufferSize;

#endif

  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio)
{
    UNUSED(sh_audio);
    return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
    int error;
    unsigned long ConvertedFrames=0;
    unsigned long ConvertedBytes=0;
    UNUSED(sh);
    error=SoundConverterEndConversion(myConverter,NULL,&ConvertedFrames,&ConvertedBytes);
    MSG_V("SoundConverterEndConversion:%i\n",error);
    error = SoundConverterClose(myConverter);
    MSG_V("SoundConverterClose:%i\n",error);
//    error = TerminateQTML();
//    MSG_V("TerminateQTML:%i\n",error);
//    FreeLibrary( qtml_dll );
//    qtml_dll = NULL;
//    printf("qt dll loader uninit done\n");
#ifdef MACOSX
    ExitMovies();
#endif
}

static unsigned decode(sh_audio_t *sh,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts){
    int error;
    unsigned long FramesToGet=0; //how many frames the demuxer has to get
    unsigned long InputBufferSize=0; //size of the input buffer
    unsigned long ConvertedFrames=0;
    unsigned long ConvertedBytes=0;

    FramesToGet=minlen/OutFrameSize;
    if(FramesToGet*OutFrameSize<minlen &&
       (FramesToGet+1)*OutFrameSize<=maxlen) ++FramesToGet;
    if(FramesToGet*InFrameSize>sh->a_in_buffer_size)
	FramesToGet=sh->a_in_buffer_size/InFrameSize;

    InputBufferSize=FramesToGet*InFrameSize;

//    printf("FramesToGet = %li  (%li -> %li bytes)\n",FramesToGet,
//	InputBufferSize, FramesToGet*OutFrameSize);

    if(InputBufferSize>(unsigned)sh->a_in_buffer_len){
	int x=demux_read_data_r(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
	    InputBufferSize-sh->a_in_buffer_len,pts);
	if(x>0) sh->a_in_buffer_len+=x;
	if(InputBufferSize>(unsigned)sh->a_in_buffer_len)
	    FramesToGet=sh->a_in_buffer_len/InFrameSize; // not enough data!
    }

//    printf("\nSoundConverterConvertBuffer(myConv=%p,inbuf=%p,frames=%d,outbuf=%p,&convframes=%p,&convbytes=%p)\n",
//	myConverter,sh->a_in_buffer,FramesToGet,buf,&ConvertedFrames,&ConvertedBytes);
    error = SoundConverterConvertBuffer(myConverter,sh->a_in_buffer,
	FramesToGet,buf,&ConvertedFrames,&ConvertedBytes);
//    printf("SoundConverterConvertBuffer:%i\n",error);
//    printf("ConvertedFrames = %li\n",ConvertedFrames);
//    printf("ConvertedBytes = %li\n",ConvertedBytes);

//    InputBufferSize=(ConvertedBytes/OutFrameSize)*InFrameSize; // FIXME!!
    InputBufferSize=FramesToGet*InFrameSize;
    sh->a_in_buffer_len-=InputBufferSize;
    if(sh->a_in_buffer_len<0) sh->a_in_buffer_len=0; // should not happen...
    else if(sh->a_in_buffer_len>0){
	memcpy(sh->a_in_buffer,&sh->a_in_buffer[InputBufferSize],sh->a_in_buffer_len);
    }

    return ConvertedBytes;
}

static int control(sh_audio_t *sh,int cmd,any_t* arg, ...){
    // various optional functions you MAY implement:
  UNUSED(sh);
  UNUSED(cmd);
  UNUSED(arg);
  return CONTROL_UNKNOWN;
}
