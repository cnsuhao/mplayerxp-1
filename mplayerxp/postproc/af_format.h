#ifndef AF_FORMAT_H_INCLUDED
#define AF_FORMAT_H_INCLUDED 1
/* The sample format system used lin libaf is based on bitmasks. The
   format definition only refers to the storage format not the
   resolution. */
enum {
// Endianess
    AF_FORMAT_BE	=0x00000000UL, // Big Endian
    AF_FORMAT_LE	=0x00000001UL, // Little Endian
    AF_FORMAT_END_MASK	=0x00000001UL,

#if WORDS_BIGENDIAN	// Native endian of cpu
    AF_FORMAT_NE	=AF_FORMAT_BE,
#else
    AF_FORMAT_NE	=AF_FORMAT_LE,
#endif

// Signed/unsigned
    AF_FORMAT_SI	=0x00000000UL, // SIgned
    AF_FORMAT_US	=0x00000002UL, // Un Signed
    AF_FORMAT_SIGN_MASK	=0x00000002UL,

// Fixed or floating point
    AF_FORMAT_I		=0x00000000UL, // Int
    AF_FORMAT_F		=0x00000004UL, // Foating point
    AF_FORMAT_POINT_MASK=0x00000004UL,

// Special flags refering to non pcm data
    AF_FORMAT_PCM	=0x00010000UL, //
    AF_FORMAT_A_LAW	=0x00060000UL, //
    AF_FORMAT_MU_LAW	=0x00070000UL, //
    AF_FORMAT_IMA_ADPCM	=0x00110000UL, // Same as 16 bit signed int 
    AF_FORMAT_MPEG2	=0x00500000UL, // MPEG1 layer2 audio
    AF_FORMAT_MPEG3	=0x00550000UL, // MPEG1 layer3 audio
    AF_FORMAT_AC3	=0x20000000UL, // Dolby Digital AC3
    AF_FORMAT_SPECIAL_MASK=0xFFFF0000UL
};
extern char* fmt2str(int format, unsigned bps, char* str, size_t size);
extern int str2fmt(const char *str,int *bps);
#endif
