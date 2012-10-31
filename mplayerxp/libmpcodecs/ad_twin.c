#include "mp_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "codecs_ld.h"
#include "loader/ldt_keeper.h"
#include "loader/wine/windef.h"
#include "libao2/afmt.h"
#include "libmpdemux/aviprint.h"
#include "osdep/mplib.h"

#include "help_mp.h"

static const ad_info_t info = 
{
	"TWinVQ decoder",
	"vqf",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(twin)

/************************/
/*** General settings ***/
/************************/
/* Initialization error code */
enum INIT_ERROR_CODE {
	TVQ_NO_ERROR = 0,     // no error
	TVQ_ERROR,            // general
	TVQ_ERROR_VERSION,    // wrong version
	TVQ_ERROR_CHANNEL,    // channel setting error
	TVQ_ERROR_MODE,       // wrong coding mode
	TVQ_ERROR_PARAM,      // inner parameter setting error
	TVQ_ERROR_N_CAN,      // wrong number of VQ pre-selection candidates, used only in encoder
};

/* version ID */
#define TVQ_UNKNOWN_VERSION  -1
#define V2                    0
#define V2PP                  1

#define N_VERSIONS            2

/* window types */
enum WINDOW_TYPE {
  ONLY_LONG_WINDOW = 0,
  LONG_SHORT_WINDOW,
  ONLY_SHORT_WINDOW,
  SHORT_LONG_WINDOW,
  SHORT_MEDIUM_WINDOW,
  MEDIUM_LONG_WINDOW,
  LONG_MEDIUM_WINDOW,
  MEDIUM_SHORT_WINDOW,
  ONLY_MEDIUM_WINDOW,
};

/* block types */
enum BLOCK_TYPE {
	BLK_SHORT = 0,
	BLK_MEDIUM,
	BLK_LONG,
	BLK_PPC,
};
#define N_BTYPE     3  // number of block types
#define N_INTR_TYPE 4  // number of interleave types, enum BLOCK_TYPE is commonly used for detecting interleave types.

/* maximum number of channels */
#define N_CH_MAX     2

/* type definition of code information interface */
typedef struct {
	/* block type */
    int  w_type;
    int  btype;

	/* FBC info */
    int  *segment_sw[ N_CH_MAX ];
    int  *band_sw[ N_CH_MAX ];
    int	 *fg_intensity[ N_CH_MAX ];

	/* VQ info */
    int  *wvq;

	/* BSE info */
    int  *fw;
    int  *fw_alf;

	/* gain info */
    int  *pow;

	/* LSP info */
    int  *lsp[ N_CH_MAX ];

	/* PPC info */
    int  pit[ N_CH_MAX ];
    int  *pls;
    int  pgain[ N_CH_MAX ];

	/* EBC info */
    int  *bc[ N_CH_MAX ];

    any_t*manager;
} INDEX;

/***********************************************/
/*** Definitions about program configuration ***/
/***********************************************/
/* type definition of tvqConfInfoSubBlock */
typedef struct {
	int sf_sz;         // subframe size
	int nsf;           // number of subframes
	int ndiv;          // number of division of weighted interleave vector quantization
	int ncrb;          // number of Bark-scale subbands
	int fw_ndiv;       // number of division of BSE VQ
	int fw_nbit;       // number of bits for BSE VQ
	int nsubg;         // number of sub-blocks for gain coding
	int ppc_enable;    // PPC switch
	int ebc_enable;    // EBC switch
	int ebc_crb_base;  // EBC base band
	int ebc_bits;      // EBC bits
	int fbc_enable;    // FBC switch
	int fbc_n_segment; // FBC number of segments
	int fbc_nband;     // FBC number of subbands
	int *fbc_crb_tbl;  // FBC subband table
} tvqConfInfoSubBlock;

/* type definition of tvqConfInfo */
typedef struct {
  /* frame configuration */
  int N_CH;
  /* window type coding */
  int BITS_WTYPE;
  /* LSP coding */
  int LSP_BIT0;
  int LSP_BIT1;
  int LSP_BIT2;
  int LSP_SPLIT;
  /* Bark-scale envelope coding */
  int FW_ARSW_BITS;
  /* gain coding */
  int GAIN_BITS;
  int SUB_GAIN_BITS;
  /* pitch excitation */
  int N_DIV_P;
  int BASF_BIT;
  int PGAIN_BIT;

  /* block type dependent parameters */
  tvqConfInfoSubBlock cfg[N_BTYPE];

} tvqConfInfo;


#define	KEYWORD_BYTES	4
#define	VERSION_BYTES	8
#define ELEM_BYTES      sizeof(unsigned long)
/*
 */
typedef struct{
	char		ID[KEYWORD_BYTES+VERSION_BYTES+1];
	int size;
	/* Common Chunk */
	int channelMode;   /* channel mode (mono:0/stereo:1) */
	int bitRate;       /* bit rate (kbit/s) */
	int samplingRate;  /* sampling rate (44.1 kHz -> 44) */
	int securityLevel; /* security level (always 0) */
	/* Text Chunk */
	char	Name[BUFSIZ];
	char	Comt[BUFSIZ];
	char	Auth[BUFSIZ];
	char	Cpyr[BUFSIZ];
	char	File[BUFSIZ];
	char	Extr[BUFSIZ];  // add by OKAMOTO 99.12.21
	/* Data size chunk*/
	int		Dsiz;
} headerInfo;

extern HMODULE   WINAPI LoadLibraryA(LPCSTR);
extern FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
extern int       WINAPI FreeLibrary(HMODULE);

static int (__cdecl* TvqInitialize_ptr)( headerInfo *setupInfo, INDEX *index, int dispErrorMessageBox );
#define TvqInitialize(a,b,c) (*TvqInitialize_ptr)(a,b,c)
static void (__cdecl* TvqTerminate_ptr)( INDEX *index );
#define TvqTerminate(a) (*TvqTerminate_ptr)(a)
static void (__cdecl* TvqGetVectorInfo_ptr)(int *bits0[], int *bits1[]);
#define TvqGetVectorInfo(a,b) (*TvqGetVectorInfo_ptr)(a,b)

static void (__cdecl* TvqDecodeFrame_ptr)(INDEX  *indexp, float out[]);
#define TvqDecodeFrame(a,b) (*TvqDecodeFrame_ptr)(a,b)
static int  (__cdecl* TvqWtypeToBtype_ptr)( int w_type, int *btype );
#define TvqWtypeToBtype(a,b) (*TvqWtypeToBtype_ptr)(a,b)
static void (__cdecl* TvqUpdateVectorInfo_ptr)(int varbits, int *ndiv, int bits0[], int bits1[]);
#define TvqUpdateVectorInfo(a,b,c,d) (*TvqUpdateVectorInfo_ptr)(a,b,c,d)

static int   (__cdecl* TvqCheckVersion_ptr)(char *versionID);
#define TvqCheckVersion(a) (*TvqCheckVersion_ptr)(a)
static void  (__cdecl* TvqGetConfInfo_ptr)(tvqConfInfo *cf);
#define TvqGetConfInfo(a) (*TvqGetConfInfo_ptr)(a)
static int   (__cdecl* TvqGetFrameSize_ptr)();
#define TvqGetFrameSize() (*TvqGetFrameSize_ptr)()
static int   (__cdecl* TvqGetNumFixedBitsPerFrame_ptr)();
#define TvqGetNumFixedBitsPerFrame() (*TvqGetNumFixedBitsPerFrame_ptr)()

#define	BYTE_BIT	8
#define	BBUFSIZ		1024		/* Bit buffer size (bytes) */
#define	BBUFLEN		(BBUFSIZ*BYTE_BIT)	/* Bit buffer length (bits) */
typedef struct vqf_priv_s
{
  float pts;
  WAVEFORMATEX o_wf;   // out format
  INDEX index;
  tvqConfInfo cf;
  headerInfo hi;
  int *bits_0[N_INTR_TYPE], *bits_1[N_INTR_TYPE];
  unsigned framesize;
  /* stream related */
  int readable;
  int ptr;           /* current point in the bit buffer */
  int nbuf;          /* bit buffer size */
  char buf[BBUFSIZ];  /* the bit buffer */
}vqf_priv_t;

static HINSTANCE vqf_dll;

static int load_dll( const char *libname )
{
#ifdef WIN32_LOADER
    Setup_LDT_Keeper_ptr();
#endif
    vqf_dll = LoadLibraryA((LPCSTR)libname);
    if( vqf_dll == NULL )
    {
        MSG_ERR("failed loading dll\n" );
	return 0;
    }
  TvqInitialize_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqInitialize");
  TvqTerminate_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqTerminate");
  TvqGetVectorInfo_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqGetVectorInfo");
  TvqDecodeFrame_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqDecodeFrame");
  TvqWtypeToBtype_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqWtypeToBtype");
  TvqUpdateVectorInfo_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqUpdateVectorInfo");
  TvqCheckVersion_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqCheckVersion");
  TvqGetConfInfo_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqGetConfInfo");
  TvqGetFrameSize_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqGetFrameSize");
  TvqGetNumFixedBitsPerFrame_ptr = GetProcAddress(vqf_dll,(LPCSTR)"TvqGetNumFixedBitsPerFrame");
  return TvqInitialize_ptr && TvqTerminate_ptr && TvqGetVectorInfo_ptr &&
	 TvqDecodeFrame_ptr && TvqWtypeToBtype_ptr && TvqUpdateVectorInfo_ptr &&
	 TvqCheckVersion_ptr && TvqGetConfInfo_ptr && TvqGetFrameSize_ptr &&
	 TvqGetNumFixedBitsPerFrame_ptr;
}

static int init_vqf_audio_codec(sh_audio_t *sh_audio){
    WAVEFORMATEX *in_fmt=sh_audio->wf;
    vqf_priv_t*priv=sh_audio->context;
    int ver;
    MSG_V("======= Win32 (TWinVQ) AUDIO Codec init =======\n");

    priv->o_wf.nChannels=in_fmt->nChannels;
    priv->o_wf.nSamplesPerSec=in_fmt->nSamplesPerSec;
    priv->o_wf.nAvgBytesPerSec=in_fmt->nSamplesPerSec;
    priv->o_wf.wFormatTag=0x01;
    priv->o_wf.nBlockAlign=4*in_fmt->nChannels;
    priv->o_wf.wBitsPerSample=in_fmt->wBitsPerSample;
    priv->o_wf.cbSize=0;
    sh_audio->channels=in_fmt->nChannels;
    sh_audio->samplerate=in_fmt->nSamplesPerSec;
    sh_audio->samplesize=4;
    sh_audio->sample_format=AFMT_FLOAT32;

    if(mp_conf.verbose)
    {
	MSG_V("Input format:\n");
	print_wave_header(in_fmt,sizeof(WAVEFORMATEX));
	MSG_V("Output fmt:\n");
	print_wave_header(&priv->o_wf,sizeof(WAVEFORMATEX));
    }
    memcpy(&priv->hi,&in_fmt[1],sizeof(headerInfo));
    if((ver=TvqInitialize(&priv->hi,&priv->index,0))){
	const char *tvqe[]={
	"No errors",
	"General error",
	"Wrong version",
	"Channel setting error",
	"Wrong coding mode",
	"Inner parameter setting error",
	"Wrong number of VQ pre-selection candidates, used only in encoder" };
	MSG_ERR("Tvq initialization error: %s\n",ver>=0&&ver<7?tvqe[ver]:"Unknown");
	return 0;
    }
    ver=TvqCheckVersion(priv->hi.ID);
    if(ver==TVQ_UNKNOWN_VERSION){
	MSG_ERR("Tvq unknown version of stream\n" );
	return 0;
    }
    TvqGetConfInfo(&priv->cf);
    TvqGetVectorInfo(priv->bits_0,priv->bits_1);
    priv->framesize=TvqGetFrameSize();
    sh_audio->audio_in_minsize=priv->framesize*in_fmt->nChannels;
    sh_audio->a_in_buffer_size=4*sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=mp_malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;


    return 1;
}

static int close_vqf_audio_codec(sh_audio_t *sh_audio)
{
    vqf_priv_t*priv=sh_audio->context;
    TvqTerminate(&priv->index);
    return 1;
}

int init(sh_audio_t *sh_audio)
{
    UNUSED(sh_audio);
    return 1;
}

int preinit(sh_audio_t *sh_audio)
{
  /* Win32 VQF audio codec: */
  vqf_priv_t *priv;
  if(!(sh_audio->context=mp_malloc(sizeof(vqf_priv_t)))) return 0;
  priv=sh_audio->context;
  if(!load_dll((const char *)sh_audio->codec->dll_name))
  {
    MSG_ERR("win32.dll looks broken :(\n");
    return 0;
  }
  if(!init_vqf_audio_codec(sh_audio)){
    MSG_ERR("TWinVQ initialization fail\n");
    return 0;
  }
  MSG_V("INFO: TWinVQ audio codec init OK!\n");
  return 1;
}

void uninit(sh_audio_t *sh)
{
  close_vqf_audio_codec(sh);
  mp_free(sh->context);
  FreeLibrary(vqf_dll);
}

ControlCodes control(sh_audio_t *sh_audio,int cmd,any_t* arg, ...)
{
  int skip;
  UNUSED(arg);
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	{
		float pts;
		    skip=sh_audio->wf->nBlockAlign;
		    if(skip<16){
		      skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		      if(skip<16) skip=16;
		    }
		    demux_read_data_r(sh_audio->ds,NULL,skip,&pts);
	  return CONTROL_TRUE;
	}
      default:
	  return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

static int bread(char	*data,    /* Output: Output data array */
		  int	size,     /* Input:  Length of each data */
		  int	nbits,    /* Input:  Number of bits to write */
		  sh_audio_t *sh,  /* Input:  File pointer */
		  float *pts)
{
    /*--- Variables ---*/
    int	 ibits, iptr, idata, ibufadr, ibufbit, icl;
    unsigned char mask, tmpdat;
    int  retval;
    vqf_priv_t *priv=sh->context;
	
    /*--- Main operation ---*/
    retval = 0;
    mask = 0x1;
    for ( ibits=0; ibits<nbits; ibits++ ){
		if ( priv->readable == 0 ){  /* when the file data buffer is empty */
			priv->nbuf = demux_read_data_r(sh->ds, priv->buf, BBUFSIZ, &priv->pts);
			priv->nbuf *= 8;
			priv->readable = 1;
		}
		*pts=FIX_APTS(sh,priv->pts,priv->ptr);
		iptr = priv->ptr;           /* current file data buffer pointer */
		if ( iptr >= priv->nbuf )   /* If data file is empty then return */
			return(retval);
		ibufadr = iptr/BYTE_BIT;      /* current file data buffer address */
		ibufbit = iptr%BYTE_BIT;      /* current file data buffer bit */
		/*	tmpdat = stream->buf[ibufadr] >> (BYTE_BIT-ibufbit-1); */
		tmpdat = (unsigned char)priv->buf[ibufadr];
		tmpdat >>= (BYTE_BIT-ibufbit-1);
		/* current data bit */
		
		idata = ibits*size;                   /* output data address */
		data[idata] = (char)(tmpdat & mask);  /* set output data */
		for (icl=1; icl<size; icl++)
			data[idata+icl] = 0; /* clear the rest output data buffer */
		priv->ptr += 1;       /* update data buffer pointer */
		if (priv->ptr == BBUFLEN){
			priv->ptr = 0;
			priv->readable = 0;
		}
		++retval;
    }
    return(retval);
}

#define	BITS_INT	(sizeof(int)*8)

static int get_bstm(int	*data,          /* Input: input data */
		    unsigned nbits,         /* Input: number of bits */
		    sh_audio_t *sh,          /* Input: bit file pointer */
		    float *pts)
{
    unsigned	ibit;
    unsigned	mask;
    unsigned	work;
    char	tmpbit[BITS_INT];
    int		retval;
	
    if ( nbits > BITS_INT ){
		MSG_ERR( "get_bstm(): %d: %d Error.\n",
			nbits, BITS_INT);
		exit(1);
    }
    retval = bread(tmpbit, sizeof(*tmpbit), nbits, sh, pts );
    for (ibit=retval; ibit<nbits; ibit++){
		tmpbit[ibit] = 0;
    }
    mask = 0x1<<(nbits-1);
    work=0;
    for ( ibit=0; ibit<nbits; ibit++ ){
		work += mask*tmpbit[ibit];
		mask >>= 1;
    }
    *data = work;
    return(retval);
}

static int GetVqInfo( tvqConfInfoSubBlock *cfg,
			int bits0[],
			int bits1[],
			int variableBits,
			INDEX *index,
			sh_audio_t *sh)
{
	int idiv;
	int bitcount = 0;
	float pts;
	if ( index->btype == BLK_LONG ){
		TvqUpdateVectorInfo( variableBits, &cfg->ndiv, bits0, bits1 ); // re-calculate VQ bits
	}
	for ( idiv=0; idiv<cfg->ndiv; idiv++ ){
		bitcount += get_bstm(&index->wvq[idiv],bits0[idiv],sh,&pts); /* CB 0 */
		bitcount += get_bstm(&index->wvq[idiv+cfg->ndiv],bits1[idiv],sh,&pts); /* CB 1 */
	}
	return bitcount;
}

static int GetBseInfo( tvqConfInfo *cf, tvqConfInfoSubBlock *cfg, INDEX *index, sh_audio_t *sh)
{
	int i_sup, isf, itmp, idiv;
	int bitcount = 0;
	float pts;
	for ( i_sup=0; i_sup<cf->N_CH; i_sup++ ){
		for ( isf=0; isf<cfg->nsf; isf++ ){
			for ( idiv=0; idiv<cfg->fw_ndiv; idiv++ ){
				itmp = idiv + ( isf + i_sup * cfg->nsf ) * cfg->fw_ndiv;
				bitcount += get_bstm(&index->fw[itmp],cfg->fw_nbit,sh,&pts);
			}
		}
	}
	for ( i_sup=0; i_sup<cf->N_CH; i_sup++ ){
		for ( isf=0; isf<cfg->nsf; isf++ ){
			bitcount += get_bstm(&index->fw_alf[i_sup * cfg->nsf + isf],cf->FW_ARSW_BITS,sh,&pts);
		}
	}
	return bitcount;
}

static int GetGainInfo(tvqConfInfo *cf, tvqConfInfoSubBlock *cfg, INDEX *index, sh_audio_t *sh )
{
	int i_sup, iptop, isf;
	int bitcount = 0;
	float pts;
	for ( i_sup=0; i_sup<cf->N_CH; i_sup++ ){
		iptop = ( cfg->nsubg + 1 ) * i_sup;
		bitcount += get_bstm(&index->pow[iptop], cf->GAIN_BITS,sh,&pts);
		for ( isf=0; isf<cfg->nsubg; isf++ ){
			bitcount += get_bstm(&index->pow[iptop+isf+1], cf->SUB_GAIN_BITS,sh,&pts);
		}
	}
	return bitcount;
}

static int GetLspInfo( tvqConfInfo *cf, INDEX *index, sh_audio_t *sh )
{
	int i_sup, itmp;
	int bitcount = 0;
	float pts;

	for ( i_sup=0; i_sup<cf->N_CH; i_sup++ ){
		bitcount += get_bstm(&index->lsp[i_sup][0], cf->LSP_BIT0,sh,&pts); /* pred. switch */
		bitcount += get_bstm(&index->lsp[i_sup][1], cf->LSP_BIT1,sh,&pts); /* first stage */
		for ( itmp=0; itmp<cf->LSP_SPLIT; itmp++ ){         /* second stage */
			bitcount += get_bstm(&index->lsp[i_sup][itmp+2], cf->LSP_BIT2,sh,&pts);
		}
	}

	return bitcount;
}

static int GetPpcInfo( tvqConfInfo *cf, INDEX *index, sh_audio_t *sh)
{
	int idiv, i_sup;
	int bitcount = 0;
	vqf_priv_t*priv=sh->context;
	float pts;
	
	for ( idiv=0; idiv<cf->N_DIV_P; idiv++ ){
		bitcount += get_bstm(&(index->pls[idiv]), priv->bits_0[BLK_PPC][idiv],sh,&pts);       /*CB0*/
		bitcount += get_bstm(&(index->pls[idiv+cf->N_DIV_P]), priv->bits_1[BLK_PPC][idiv],sh,&pts);/*CB1*/
	}
	for (i_sup=0; i_sup<cf->N_CH; i_sup++){
		bitcount += get_bstm(&(index->pit[i_sup]), cf->BASF_BIT,sh,&pts);
		bitcount += get_bstm(&(index->pgain[i_sup]), cf->PGAIN_BIT,sh,&pts);
	}
	
	return bitcount;
}

static int GetEbcInfo( tvqConfInfo *cf, tvqConfInfoSubBlock *cfg, INDEX *index, sh_audio_t *sh)
{
	int i_sup, isf, itmp;
	int bitcount = 0;
	float pts;

	for ( i_sup=0; i_sup<cf->N_CH; i_sup++ ){
		for ( isf=0; isf<cfg->nsf; isf++){
			int indexSfOffset = isf * ( cfg->ncrb - cfg->ebc_crb_base ) - cfg->ebc_crb_base;
			for ( itmp=cfg->ebc_crb_base; itmp<cfg->ncrb; itmp++ ){
				bitcount += get_bstm(&index->bc[i_sup][itmp+indexSfOffset], cfg->ebc_bits,sh,&pts);
			}
		}
	}
	
	return bitcount;
}

static int vqf_read_frame(sh_audio_t *sh,INDEX *index,float *pts)
{
	/*--- Variables ---*/
	tvqConfInfoSubBlock *cfg;
	int variableBits;
	int bitcount;
	int numFixedBitsPerFrame = TvqGetNumFixedBitsPerFrame();
	int btype;
	vqf_priv_t *priv=sh->context;
	
	/*--- Initialization ---*/
	variableBits = 0;
	bitcount = 0;

	/*--- read block independent factors ---*/
	/* Window type */
	bitcount += get_bstm( &index->w_type, priv->cf.BITS_WTYPE, sh,pts);
	if ( TvqWtypeToBtype( index->w_type, &index->btype ) ) {
		MSG_ERR("Error: unknown window type: %d\n", index->w_type);
		return 0;
	}
	btype = index->btype;

	/*--- read block dependent factors ---*/
	cfg = &priv->cf.cfg[btype]; // set the block dependent paremeters table

	bitcount += variableBits;
	
	/* Interleaved vector quantization */
	bitcount += GetVqInfo( cfg, priv->bits_0[btype], priv->bits_1[btype], variableBits, index, sh );
	
	/* Bark-scale envelope */
	bitcount += GetBseInfo( &priv->cf, cfg, index, sh );
	/* Gain */
	bitcount += GetGainInfo( &priv->cf, cfg, index, sh );
	/* LSP */
	bitcount += GetLspInfo( &priv->cf, index, sh );
	/* PPC */
	if ( cfg->ppc_enable ){
		bitcount += GetPpcInfo( &priv->cf, index, sh );
	}
	/* Energy Balance Calibration */
	if ( cfg->ebc_enable ){
		bitcount += GetEbcInfo( &priv->cf, cfg, index, sh );
	}
	
	return bitcount == numFixedBitsPerFrame ? bitcount/8 : 0;
}

static void frtobuf(float out[],       /* Input  --- input data frame */
		float  bufout[],    /* Output --- output data buffer array */
		unsigned frameSize,   /* Input  --- frame size */
		unsigned numChannels) /* Input  --- number of channels */
{
	/*--- Variables ---*/
	unsigned ismp, ich;
	float *ptr;
	register float dtmp;
	
	for ( ich=0; ich<numChannels; ich++ ){
		ptr = out+ich*frameSize;
		for ( ismp=0; ismp<frameSize; ismp++ ){
			dtmp = ptr[ismp];
			if ( dtmp >= 0. )
				bufout[ismp*numChannels+ich] = (dtmp+0.5)/32767.;
			else
				bufout[ismp*numChannels+ich] = (dtmp-0.5)/32767.;
		}
	}
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
	unsigned l,len=0;
	float null_pts;
	vqf_priv_t *priv=sh_audio->context;
	UNUSED(maxlen);
	while(len<minlen)
	{
	    float out[priv->framesize*sh_audio->channels];
	    l=vqf_read_frame(sh_audio,&priv->index,len?&null_pts:pts);
	    if(!l) break;
	    TvqDecodeFrame(&priv->index, out);
	    frtobuf(out, (float *)buf, priv->framesize, sh_audio->channels);
	    len += priv->framesize*sh_audio->channels*4;
	    buf += priv->framesize*sh_audio->channels*4;
	}
	return len;
}
