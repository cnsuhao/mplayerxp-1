// #define NEWBUFFERING
//#define DEBUG_RESYNC

/* 1 frame = 4608 byte PCM */

#ifdef __GNUC__
#define LOCAL static inline
#else
#define LOCAL static _inline
#endif

//#undef LOCAL
//#define LOCAL

#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include        <signal.h>
#include	<stdint.h>
#include        <math.h>

#include "mpg123.h"
#include "mp3.h"
#include "bswap.h"
#include "../mm_accel.h"

//static FILE* mp3_file=NULL;

/* exported stuff */
int MP3_frames=0;
int MP3_eof=0;
int MP3_pause=0;
int MP3_filesize=0;
int MP3_fpos=0;      // current file position
int MP3_framesize=0; // current framesize
int MP3_bitrate=0;   // current bitrate
uint32_t MP3_ave_framesize=0; // average framesize
uint32_t MP3_ave_bitrate=0;   // average bitrate
int MP3_samplerate=0;  // current samplerate
int MP3_resync=0;
int MP3_channels=0;

static float outscale = 1.0;

#if 1
static int (*mplayer_audio_read)(char *buf,int size,float *pts);

LOCAL int mp3_read(unsigned char *buf,int size,float *pts){
//  int len=fread(buf,1,size,mp3_file);
  int len=(*mplayer_audio_read)((char *)buf,size,pts);
  if(len>0) MP3_fpos+=len;
//  if(len!=size) MP3_eof=1;
  return len;
}
#else
extern int mp3_read(char *buf,int size,float *pts);
#endif

//void mp3_seek(int pos){
//  fseek(mp3_file,pos,SEEK_SET);
//  return (MP3_fpos=ftell(mp3_file));
//}

/*       Frame reader           */

#define MAXFRAMESIZE 1280
#define MAXFRAMESIZE2 (512+MAXFRAMESIZE)

static int fsizeold=0;
static unsigned char bsspace[2][MAXFRAMESIZE2]; /* !!!!! */
static unsigned char *bsbufold=bsspace[0]+512;
static unsigned char *bsbuf=bsspace[1]+512;
static int bsnum=0;

int bitindex;
unsigned char *wordpointer;
int bitsleft;

unsigned char *pcm_sample;   /* outbuffer address */
int pcm_point = 0;           /* outbuffer offset */

struct frame fr;

static int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

uint32_t freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };

uint32_t getbits(short number_of_bits)
{
  uint32_t rval;
//  if(MP3_frames>=7741) printf("getbits: bits=%d  bitsleft=%d  wordptr=%x\n",number_of_bits,bitsleft,wordpointer);
  if((bitsleft-=number_of_bits)<0) return 0;
  if(!number_of_bits) return 0;
         rval = wordpointer[0];
         rval <<= 8;
         rval |= wordpointer[1];
         rval <<= 8;
         rval |= wordpointer[2];
         rval <<= bitindex;
         rval &= 0xffffff;
         bitindex += number_of_bits;
         rval >>= (24-number_of_bits);
         wordpointer += (bitindex>>3);
         bitindex &= 7;
  return rval;
}


uint32_t getbits_fast(short number_of_bits)
{
  uint32_t rval;
//  if(MP3_frames>=7741) printf("getbits_fast: bits=%d  bitsleft=%d  wordptr=%x\n",number_of_bits,bitsleft,wordpointer);
  if((bitsleft-=number_of_bits)<0) return 0;
  if(!number_of_bits) return 0;
#if defined(CAN_COMPILE_X86_ASM)
  rval = bswap_16(*((unsigned short *)wordpointer));
#else
  /*
   * we may not be able to address unaligned 16-bit data on non-x86 cpus.
   * Fall back to some portable code.
   */
  rval = wordpointer[0] << 8 | wordpointer[1];
#endif
         rval <<= bitindex;
         rval &= 0xffff;
         bitindex += number_of_bits;
         rval >>= (16-number_of_bits);
         wordpointer += (bitindex>>3);
         bitindex &= 7;
  return rval;
}

unsigned int get1bit(void)
{
  unsigned char rval;
//  if(MP3_frames>=7741) printf("get1bit: bitsleft=%d  wordptr=%x\n",bitsleft,wordpointer);
  if((--bitsleft)<0) return 0;
  rval = *wordpointer << bitindex;
  bitindex++;
  wordpointer += (bitindex>>3);
  bitindex &= 7;
  return ((rval>>7)&1);
}

static unsigned ssize=0;
void set_pointer(int32_t backstep)
{
//  if(backstep!=512 && backstep>fsizeold)
//    printf("\rWarning! backstep (%d>%d)                                         \n",backstep,fsizeold);
  wordpointer = bsbuf + ssize - backstep;
  if (backstep) memcpy(wordpointer,bsbufold+fsizeold-backstep,backstep);
  bitindex = 0;
  bitsleft+=8*backstep;
//  printf("Backstep %d  (bitsleft=%d)\n",backstep,bitsleft);
}

LOCAL int stream_head_read(unsigned char *hbuf,uint32_t *newhead){
  float pts;
  if(mp3_read(hbuf,4,&pts) != 4) return FALSE;
#if defined(CAN_COMPILE_X86_ASM)
  *newhead = bswap_32(*((uint32_t *)hbuf));
#else
  /*
   * we may not be able to address unaligned 32-bit data on non-x86 cpus.
   * Fall back to some portable code.
   */
  *newhead =
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];
#endif
  return TRUE;
}

LOCAL int stream_head_shift(unsigned char *hbuf,uint32_t *head){
  float pts;
  *((uint32_t *)hbuf) >>= 8;
  if(mp3_read(hbuf+3,1,&pts) != 1) return 0;
  *head <<= 8;
  *head |= hbuf[3];
  return 1;
}

/*
 * decode a header and write the information
 * into the frame structure
 */
LOCAL int decode_header(struct frame *fr,uint32_t newhead){

    // head_check:
    if( (newhead & 0xffe00000) != 0xffe00000 ||
        (newhead & 0x0000fc00) == 0x0000fc00) return FALSE;

    fr->lay = 4-((newhead>>17)&3);
//    if(fr->lay!=3) return FALSE;

    if( newhead & ((uint32_t)1<<20) ) {
      fr->lsf = (newhead & ((uint32_t)1<<19)) ? 0x0 : 0x1;
      fr->mpeg25 = 0;
    } else {
      fr->lsf = 1;
      fr->mpeg25 = 1;
    }

    if(fr->mpeg25)
      fr->sampling_frequency = 6 + ((newhead>>10)&0x3);
    else
      fr->sampling_frequency = ((newhead>>10)&0x3) + (fr->lsf*3);

    if(fr->sampling_frequency>8) return FALSE;  // valid: 0..8

    fr->error_protection = ((newhead>>16)&0x1)^0x1;
    fr->bitrate_index = ((newhead>>12)&0xf);
    fr->padding   = ((newhead>>9)&0x1);
    fr->extension = ((newhead>>8)&0x1);
    fr->mode      = ((newhead>>6)&0x3);
    fr->mode_ext  = ((newhead>>4)&0x3);
    fr->copyright = ((newhead>>3)&0x1);
    fr->original  = ((newhead>>2)&0x1);
    fr->emphasis  = newhead & 0x3;

    MP3_channels = fr->stereo    = (fr->mode == MPG_MD_MONO) ? 1 : 2;

    if(!fr->bitrate_index){
//      fprintf(stderr,"Free format not supported.\n");
      return FALSE;
    }
    ssize=0;
    MP3_samplerate=freqs[fr->sampling_frequency];
    MP3_bitrate = tabsel_123[fr->lsf][fr->lay-1][fr->bitrate_index];
switch(fr->lay){
  case 1:
	fr->framesize = MP3_bitrate * 12000;
	fr->framesize /= MP3_samplerate;
	fr->framesize = ((fr->framesize+fr->padding)<<2)-4;
	MP3_framesize = fr->framesize;
	break;
  case 3:
	if(fr->lsf)	ssize = (fr->stereo == 1) ? 9 : 17;
	else		ssize = (fr->stereo == 1) ? 17 : 32;
	if(fr->error_protection) ssize += 2;
  case 2:
	fr->framesize = MP3_bitrate * 144000;
	fr->framesize /= MP3_samplerate;
	fr->framesize += fr->padding - 4;
	break;
  default:
//    fprintf(stderr,"Sorry, unsupported layer type.\n");
    fr->framesize=0;
    break;
}
    if(fr->framesize<=0 || fr->framesize>MAXFRAMESIZE) return FALSE;
    MP3_framesize = fr->framesize;
    if(!MP3_ave_bitrate) MP3_ave_bitrate=MP3_bitrate*1000;
    if(!MP3_ave_framesize) MP3_ave_framesize=MP3_framesize;

    return 1;
}

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
#define FRAMES_AND_BYTES (FRAMES_FLAG | BYTES_FLAG)
static void test_xing(struct frame *fr,unsigned char *body)
{
    unsigned off;
    uint32_t head_flags;
    float ave_frsize,ave_bitrate;
    const int sr_table[4] = { 44100, 48000, 32000, 99999 };
    fr->xing.scale=-1;
    if(!fr->lsf)off=fr->mode!=MPG_MD_MONO?32:17;
    else	off=fr->mode!=MPG_MD_MONO?17:9;/* mpeg2 */
    if(memcmp(&body[off],"Xing",4) != 0) return;
    off+=4;
    fr->xing.is_xing=1;
    head_flags = *(uint32_t *)(&body[off]); head_flags = be2me_32(head_flags);
    off += 4;
    if(head_flags & FRAMES_FLAG)	{ fr->xing.nframes=*(uint32_t *)(&body[off]); fr->xing.nframes=be2me_32(fr->xing.nframes); off+=4; }
    if(head_flags & BYTES_FLAG)		{ fr->xing.nbytes=*(uint32_t *)(&body[off]); fr->xing.nbytes=be2me_32(fr->xing.nbytes); off+=4; }
    if(head_flags & TOC_FLAG)		{ memcpy(fr->xing.toc,&body[off],100);  off+=100; }
    if(head_flags & VBR_SCALE_FLAG)	{ fr->xing.scale =*(uint32_t *)(&body[off]);fr->xing.scale=be2me_32(fr->xing.scale); off+=4; }
    fflush(stdout);
    ave_frsize=(float)(fr->xing.nbytes)/fr->xing.nframes;
    ave_bitrate=(ave_frsize*(float)(sr_table[fr->sampling_frequency&0x3]<<fr->lsf))/144000.;
    MP3_ave_framesize=(int)rintf(ave_frsize);
    /*
	Keep everything in bit/secs.
	Lets take 32kbit/sec VBR stream in AVI. This stream is 4000 bytes/sec.
	So if we have wrongly computed last digit (as 3999 or 4001)
	then it will cause 1 sec of AV resync per 1 hour 10 min.
	Well all modern streams have 128 kbit/sec at least that will cause
	1 sec of AV resync per 4 hour 26 min.
	256	8h52m
	320	11h6m
    */
    MP3_ave_bitrate=(int)rintf(ave_bitrate*1000);
}

LOCAL int stream_read_frame_body(int size,float *pts){

  /* flip/init buffer for Layer 3 */
  bsbufold = bsbuf;
  bsbuf = bsspace[bsnum]+512;
  bsnum = (bsnum + 1) & 1;

  if(mp3_read(bsbuf,size,pts) != size) return 0; // broken frame

  bitindex = 0;
  wordpointer = (unsigned char *) bsbuf;
  bitsleft=8*size;

  return 1;
}


/*****************************************************************
 * read next frame     return number of frames read.
 */
LOCAL int read_frame(struct frame *fr,float *pts){
  uint32_t newhead;
  unsigned char hbuf[8];
  int skipped,resyncpos;
  int frames=0;
  static int n_real_frames=0;

resync:
  skipped=MP3_fpos;
  resyncpos=MP3_fpos;

  set_pointer(512);
  fsizeold=fr->framesize;       /* for Layer3 */
  if(!stream_head_read(hbuf,&newhead)) return 0;
  if(!decode_header(fr,newhead)){
    // invalid header! try to resync stream!
#ifdef DEBUG_RESYNC
    printf("ReSync: searching for a valid header...  (pos=%X)\n",MP3_fpos);
#endif
retry1:
    while(!decode_header(fr,newhead)){
      if(!stream_head_shift(hbuf,&newhead)) return 0;
    }
    resyncpos=MP3_fpos-4;
    // found valid header
#ifdef DEBUG_RESYNC
    printf("ReSync: found valid hdr at %X  fsize=%ld  ",resyncpos,fr->framesize);
#endif
    if(!stream_read_frame_body(fr->framesize,pts)) return 0; // read body
    set_pointer(512);
    fsizeold=fr->framesize;       /* for Layer3 */
    if(!stream_head_read(hbuf,&newhead)) return 0;
    if(!decode_header(fr,newhead)){
      // invalid hdr! go back...
#ifdef DEBUG_RESYNC
      printf("INVALID\n");
#endif
//      mp3_seek(resyncpos+1);
      if(!stream_head_read(hbuf,&newhead)) return 0;
      goto retry1;
    }
#ifdef DEBUG_RESYNC
    printf("OK!\n");
    ++frames;
#endif
  }

  skipped=resyncpos-skipped;
//  if(skipped && !MP3_resync) printf("\r%d bad bytes skipped  (resync at 0x%X)                          \n",skipped,resyncpos);

//  printf("%8X [%08X] %d %d (%d)%s%s\n",MP3_fpos-4,newhead,fr->framesize,fr->mode,fr->mode_ext,fr->error_protection?" CRC":"",fr->padding?" PAD":"");

  /* read main data into memory */
  if(!stream_read_frame_body(fr->framesize,pts)){
    printf("\nBroken frame at 0x%X                                                  \n",resyncpos);
    return 0;
  }
  /* we should test only 1st mp3 header for xing */
  if(!n_real_frames) test_xing(fr,wordpointer);
  ++n_real_frames;
  ++frames;

  if(MP3_resync){
    MP3_resync=0;
    if(frames==1) goto resync;
  }

  return frames;
}

/******************************************************************************/
/*           PUBLIC FUNCTIONS                  */
/******************************************************************************/

static int tables_done_flag=0;
dct36_func_t dct36_func;

/* It's hidden from gcc in assembler */
static char *sopt="generic";
// Init decoder tables.  Call first, once!
void MP3_Init(int fakemono,unsigned accel,int (*audio_read)(char *buf,int size,float *pts),const char *param){
    mplayer_audio_read = audio_read;

    make_decode_tables(outscale);

    if (fakemono == 1)
        fr.synth = synth_1to1_l_32;
    else if (fakemono == 2)
        fr.synth = synth_1to1_r_32;
    else
        fr.synth = synth_1to1_32;
    fr.synth_mono = synth_1to1_mono2stereo_32;
    fr.down_sample = 0;
    fr.down_sample_sblimit = SBLIMIT>>(fr.down_sample);
    init_layer2();
    init_layer3(fr.down_sample_sblimit);
    tables_done_flag = 1;

    fr.dct12 = dct12;
    fr.dct36 = dct36;
    fr.dct64 = dct64;

#if defined( CAN_COMPILE_X86_ASM )
    if (accel & MM_ACCEL_X86_3DNOW)
    {
	fr.synth=synth_1to1_3dnow32;
	fr.dct36=dct36_3dnow;
	fr.dct64=dct64_3dnow;
	sopt="3DNow!Ex";
    }
#endif

    if (fakemono == 1)
        fr.synth=synth_1to1_l_32;
    else if (fakemono == 2)
        fr.synth=synth_1to1_r_32;
    else
        fr.synth=synth_1to1_32;
    fr.synth_mono=synth_1to1_mono2stereo_32;
    fr.down_sample=0;
    fr.down_sample_sblimit = SBLIMIT>>(fr.down_sample);
    init_layer2();
    init_layer3(fr.down_sample_sblimit);
    tables_done_flag=1;
}

#if 0

void MP3_Close(){
  MP3_eof=1;
  if(mp3_file) fclose(mp3_file);
  mp3_file=NULL;
}

// Open a file, init buffers. Call once per file!
int MP3_Open(char *filename,int buffsize){
  MP3_eof=1;   // lock decoding
  MP3_pause=1; // lock playing
  if(mp3_file) MP3_Close(); // close prev. file
  MP3_frames=0;

  mp3_file=fopen(filename,"rb");
//  printf("MP3_Open: file='%s'",filename);
//  if(!mp3_file){ printf(" not found!\n"); return 0;} else printf("Ok!\n");
  if(!mp3_file) return 0;

  MP3_filesize=MP3_PrintTAG();
  fseek(mp3_file,0,SEEK_SET);

  MP3_InitBuffers(buffsize);
  if(!tables_done_flag) MP3_Init();
  MP3_eof=0;  // allow decoding
  MP3_pause=0; // allow playing
  return MP3_filesize;
}

#endif

// Read & decode a single frame. Called by sound driver.
int MP3_DecodeFrame(unsigned char *hova,short single,float *pts){
   int retval;
   pcm_sample = hova;
   pcm_point = 0;
   if(!read_frame(&fr,pts)) return 0;
   if(single==-2){ set_pointer(512); return 1; }
   if(fr.error_protection) getbits(16); /* skip crc */
   fr.single=single;
   switch(fr.lay){
     case 1: retval=do_layer1(&fr,single);break;
     case 2: retval=do_layer2(&fr,single);break;
     case 3: retval=do_layer3(&fr,single);break;
     default: return 0; /* mp1 is unsupported */
   }
//   ++MP3_frames;
   return (retval<0)?4:(pcm_point?pcm_point:4);
}

// Prints last frame header in ascii.
void MP3_PrintHeader(){
        static char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
        static char *layers[4] = { "???" , "I", "II", "III" };

        printf("\rMPEG %s, Layer %s (%s), Hz=%d %d-kbit %s, BPF=%d Out=32-bit\n",
                fr.mpeg25 ? "2.5" : (fr.lsf ? "2.0" : "1.0"),
                layers[fr.lay],
		fr.xing.is_xing?"VBR":"CBR",
		freqs[fr.sampling_frequency],
		MP3_ave_bitrate/1000,
                modes[fr.mode],
		fr.xing.is_xing?MP3_ave_framesize:fr.framesize+4);
        printf("Chans=%d Copyrght=%s Orig=%s CRC=%s Emphas=%d Optimiz=%s\n",
                fr.stereo,fr.copyright?"Yes":"No",
                fr.original?"Yes":"No",fr.error_protection?"Yes":"No",
                fr.emphasis,sopt);
}

#if 0
#include "genre.h"

// Read & print ID3 TAG. Do not call when playing!!!  returns filesize.
int MP3_PrintTAG(){
        struct id3tag {
                char tag[3];
                char title[30];
                char artist[30];
                char album[30];
                char year[4];
                char comment[30];
                unsigned char genre;
        };
        struct id3tag tag;
        char title[31]={0,};
        char artist[31]={0,};
        char album[31]={0,};
        char year[5]={0,};
        char comment[31]={0,};
        char genre[31]={0,};
  int fsize;
  int ret;

  fseek(mp3_file,0,SEEK_END);
  fsize=ftell(mp3_file);
  if(fseek(mp3_file,-128,SEEK_END)) return fsize;
  ret=fread(&tag,128,1,mp3_file);
  if(ret!=1 || tag.tag[0]!='T' || tag.tag[1]!='A' || tag.tag[2]!='G') return fsize;

        strncpy(title,tag.title,30);
        strncpy(artist,tag.artist,30);
        strncpy(album,tag.album,30);
        strncpy(year,tag.year,4);
        strncpy(comment,tag.comment,30);

        if ( tag.genre <= sizeof(genre_table)/sizeof(*genre_table) ) {
                strncpy(genre, genre_table[tag.genre], 30);
        } else {
                strncpy(genre,"Unknown",30);
        }

//      printf("\n");
        printf("Title  : %30s  Artist: %s\n",title,artist);
        printf("Album  : %30s  Year  : %4s\n",album,year);
        printf("Comment: %30s  Genre : %s\n",comment,genre);
        printf("\n");
  return fsize-128;
}

#endif
