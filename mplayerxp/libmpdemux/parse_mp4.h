/* parse_mp4.h - Headerfile for MP4 file format parser code
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */

#ifndef __PARSE_MP4_H
#define __PARSE_MP4_H 1

#include <inttypes.h>

/* one byte tag identifiers */
enum {
    MP4ODescrTag		=0x01,
    MP4IODescrTag		=0x02,
    MP4ESDescrTag		=0x03,
    MP4DecConfigDescrTag	=0x04,
    MP4DecSpecificDescrTag	=0x05,
    MP4SLConfigDescrTag		=0x06,
    MP4ContentIdDescrTag	=0x07,
    MP4SupplContentIdDescrTag	=0x08,
    MP4IPIPtrDescrTag		=0x09,
    MP4IPMPPtrDescrTag		=0x0A,
    MP4IPMPDescrTag		=0x0B,
    MP4RegistrationDescrTag	=0x0D,
    MP4ESIDIncDescrTag		=0x0E,
    MP4ESIDRefDescrTag		=0x0F,
    MP4FileIODescrTag		=0x10,
    MP4FileODescrTag		=0x11,
    MP4ExtProfileLevelDescrTag	=0x13,
    MP4ExtDescrTagsStart	=0x80,
    MP4ExtDescrTagsEnd		=0xFE
};
/* object type identifiers in the ESDS */
/* See http://gpac.sourceforge.net/tutorial/mediatypes.htm */
enum {
    MP4OTI_MPEG4Systems1	=0x01, /* BIFS stream version 1 */
    MP4OTI_MPEG4Systems2	=0x02, /* BIFS stream version 2 */
    MP4OTI_MPEG4Visual		=0x20, /* MPEG-4 visual stream */
    MP4OTI_MPEG4Audio		=0x40, /* MPEG-4 audio stream */
    MP4OTI_MPEG2VisualSimple	=0x60, /* MPEG-2 visual streams with various profiles */
    MP4OTI_MPEG2VisualMain	=0x61,
    MP4OTI_MPEG2VisualSNR	=0x62,
    MP4OTI_MPEG2VisualSpatial	=0x63,
    MP4OTI_MPEG2VisualHigh	=0x64,
    MP4OTI_MPEG2Visual422	=0x65,
    MP4OTI_MPEG2AudioMain	=0x66, /* MPEG-2 audio stream part 7 ("AAC") with various profiles */
    MP4OTI_MPEG2AudioLowComplexity=0x67,
    MP4OTI_MPEG2AudioScaleableSamplingRate=0x68,
    MP4OTI_MPEG2AudioPart3	=0x69, /* MPEG-2 audio part 3 ("MP3") */
    MP4OTI_MPEG1Visual		=0x6A, /* MPEG-1 visual visual stream */
    MP4OTI_MPEG1Audio		=0x6B, /* MPEG-1 audio stream part 3 ("MP3") */
    MP4OTI_JPEG			=0x6C, /* JPEG visual stream */
    MP4OTI_13kVoice		=0xE1 /* 3GPP2 */
};

typedef uint32_t uint24_t;

/* esds_t */
typedef struct {
  uint8_t  version;
  uint24_t flags;

  /* 0x03 ESDescrTag */
  uint16_t ESId;
  uint8_t  streamPriority;

  /* 0x04 DecConfigDescrTag */
  uint8_t  objectTypeId;
  uint8_t  streamType;
  /* XXX: really streamType is
   * only 6bit, followed by:
   * 1bit  upStream
   * 1bit  reserved
   */
  uint24_t bufferSizeDB;
  uint32_t maxBitrate;
  uint32_t avgBitrate;

  /* 0x05 DecSpecificDescrTag */
  uint16_t  decoderConfigLen;
  uint8_t *decoderConfig;

  /* 0x06 SLConfigDescrTag */
  uint8_t  SLConfigLen;
  uint8_t *SLConfig;

  /* TODO: add the missing tags,
   * I currently have no specs
   * for them and doubt they
   * are currently needed ::atmos
   */
} esds_t;

int mp4_parse_esds(unsigned char *data, int datalen, esds_t *esds);
void mp4_free_esds(esds_t *esds);

#endif /* !__PARSE_MP4_H */

