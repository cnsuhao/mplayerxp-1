#ifndef __XMP_AUDIO_FRAME_INCLUDED_H
#define __XMP_AUDIO_FRAME_INCLUDED_H 1

#include "mpxp_config.h"
#include <stdlib.h>
#include <stdint.h>

namespace	usr {

    /* The sample format system is based on bitmasks. The
	format definition only refers to the storage format not the
	resolution. */
    typedef enum mpaf_format_enum{
	MPAF_BPS_MASK	=0x00000FFFUL, /* byte per sample */
	MPAF_BPS_1	=0x00000001UL,
	MPAF_BPS_2	=0x00000002UL,
	MPAF_BPS_3	=0x00000003UL,
	MPAF_BPS_4	=0x00000004UL,
// Endianess
	MPAF_BE		=0x00000000UL, // Big Endian
	MPAF_LE		=0x00001000UL, // Little Endian
	MPAF_END_MASK	=0x00001000UL,
#if WORDS_BIGENDIAN	// Native endian of cpu
	MPAF_NE		=MPAF_BE,
#else
	MPAF_NE		=MPAF_LE,
#endif
// Signed/unsigned
	MPAF_SI		=0x00000000UL, // SIgned
	MPAF_US		=0x00002000UL, // Un Signed
	MPAF_SIGN_MASK	=0x00002000UL,
// Fixed or floating point
	MPAF_I		=0x00000000UL, // Integer
	MPAF_F		=0x00004000UL, // Foating point
	MPAF_POINT_MASK	=0x00004000UL,
// Special flags refering to non pcm data
	MPAF_PCM	=0x00000000UL, //
	MPAF_IMA_ADPCM	=0x00110000UL, // Same as 16 bit signed int
	MPAF_MPEG2	=0x00500000UL, // MPEG1 layer2 audio
	MPAF_MPEG3	=0x00550000UL, // MPEG1 layer3 audio
	MPAF_AC3	=0x20000000UL, // Dolby Digital AC3
	MPAF_SPECIAL_MASK=0xFFFF0000UL
    }mpaf_format_e;
    inline mpaf_format_e operator~(mpaf_format_e a) { return static_cast<mpaf_format_e>(~static_cast<unsigned>(a)); }
    inline mpaf_format_e operator|(mpaf_format_e a, mpaf_format_e b) { return static_cast<mpaf_format_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline mpaf_format_e operator&(mpaf_format_e a, mpaf_format_e b) { return static_cast<mpaf_format_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline mpaf_format_e operator^(mpaf_format_e a, mpaf_format_e b) { return static_cast<mpaf_format_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline mpaf_format_e operator|=(mpaf_format_e& a, mpaf_format_e b) { return (a=static_cast<mpaf_format_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline mpaf_format_e operator&=(mpaf_format_e& a, mpaf_format_e b) { return (a=static_cast<mpaf_format_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline mpaf_format_e operator^=(mpaf_format_e& a, mpaf_format_e b) { return (a=static_cast<mpaf_format_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    /* Decodes the format from mplayer format to libaf format */
    mpaf_format_e __FASTCALL__ afmt2mpaf(unsigned format);
    unsigned __FASTCALL__      mpaf2afmt(mpaf_format_e fmt);
    char* __FASTCALL__         mpaf_fmt2str(mpaf_format_e format, char* str, size_t size);
    mpaf_format_e __FASTCALL__ mpaf_str2fmt(const char *str);

    inline int mpaf_test(mpaf_format_e f,unsigned bits) { return f&bits; }
    inline int mpaf_testa(mpaf_format_e f,unsigned bits) { return (f&bits)==bits; }

    enum {
	MP_AFLG_FINALIZED		=0x80000000
    };

    struct mp_aframe_t {
	unsigned	flags; /* currently unused */
	float		pts;   /* PTS if this frame */
	unsigned	xp_idx;/* index in ring buffer */
	uint8_t*	audio; /* data of audio frame */
	unsigned	len;   /* length of data */
    /*------ stream description ----------*/
	unsigned	rate;  /* rate of audio */
	unsigned	nch;   /* number of channels */
	mpaf_format_e	format;/* PCM format of audio */
    };

    mp_aframe_t*	new_mp_aframe(unsigned rate,unsigned nch,mpaf_format_e format,unsigned xp_idx);
    mp_aframe_t*	new_mp_aframe_genome(const mp_aframe_t* in);
    void		mp_alloc_aframe(mp_aframe_t* it);
    int			free_mp_aframe(mp_aframe_t* mpaf);
} // namespace
#endif
