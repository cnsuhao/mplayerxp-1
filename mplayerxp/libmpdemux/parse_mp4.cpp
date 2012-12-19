#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/* parse_mp4.c - MP4 file format parser code
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 * Code inspired by libmp4 from http://mpeg4ip.sourceforge.net/.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include "parse_mp4.h"
#include "libmpstream2/stream.h"
#include "demux_msg.h"

int mp4_read_descr_len(Memory_Stream& s) {
  uint8_t b;
  uint8_t numBytes = 0;
  uint32_t length = 0;

  do {
    b = s.read_char();
    numBytes++;
    length = (length << 7) | (b & 0x7F);
  } while ((b & 0x80) && numBytes < 4);

  return length;
}

/* parse the data part of MP4 esds atoms */
int mp4_parse_esds(unsigned char *data, int datalen, esds_t *esds) {
  /* create memory stream from data */
  Memory_Stream& s = *new Memory_Stream(data, datalen);
  uint8_t len;
#ifdef MP4_DUMPATOM
  {int i;
  MSG_DBG2("ESDS Dump (%dbyte):\n", datalen);
  for(i = 0; i < datalen; i++)
    MSG_DBG2("%02X ", data[i]);
  MSG_DBG2("\nESDS Dumped\n");}
#endif
  memset(esds, 0, sizeof(esds_t));

  esds->version = s.read_char();
  esds->flags = s.read_int24();
  MSG_V("ESDS MPEG4 version: %d  flags: 0x%06X\n",
      esds->version, esds->flags);

  /* get and verify ES_DescrTag */
  if (s.read_char() == MP4ESDescrTag) {
    /* read length */
    len = mp4_read_descr_len(s);

    esds->ESId = s.read_word();
    esds->streamPriority = s.read_char();
    MSG_V(
	"ESDS MPEG4 ES Descriptor (%dBytes):\n"
	" -> ESId: %d\n"
	" -> streamPriority: %d\n",
	len, esds->ESId, esds->streamPriority);

    if (len < (5 + 15)) {
      delete &s;
      return 1;
    }
  } else {
    esds->ESId = s.read_word();
    MSG_V(
	"ESDS MPEG4 ES Descriptor (%dBytes):\n"
	" -> ESId: %d\n", 2, esds->ESId);
  }

  /* get and verify DecoderConfigDescrTab */
  if (s.read_char() != MP4DecConfigDescrTag) {
    delete &s;
    return 1;
  }

  /* read length */
  len = mp4_read_descr_len(s);

  esds->objectTypeId = s.read_char();
  esds->streamType = s.read_char();
  esds->bufferSizeDB = s.read_int24();
  esds->maxBitrate = s.read_dword();
  esds->avgBitrate = s.read_dword();
  MSG_V(
      "ESDS MPEG4 Decoder Config Descriptor (%dBytes):\n"
      " -> objectTypeId: %d\n"
      " -> streamType: 0x%02X\n"
      " -> bufferSizeDB: 0x%06X\n"
      " -> maxBitrate: %.3fkbit/s\n"
      " -> avgBitrate: %.3fkbit/s\n",
      len, esds->objectTypeId, esds->streamType,
      esds->bufferSizeDB, esds->maxBitrate/1000.0,
      esds->avgBitrate/1000.0);

  esds->decoderConfigLen=0;

  if (len < 15) {
    delete &s;
    return 0;
  }

  /* get and verify DecSpecificInfoTag */
  if (s.read_char() != MP4DecSpecificDescrTag) {
    delete &s;
    return 0;
  }

  /* read length */
  esds->decoderConfigLen = len = mp4_read_descr_len(s);

  esds->decoderConfig = new uint8_t[esds->decoderConfigLen];
  if (esds->decoderConfig) {
    s.read(esds->decoderConfig, esds->decoderConfigLen);
  } else {
    esds->decoderConfigLen = 0;
  }
  MSG_V("ESDS MPEG4 Decoder Specific Descriptor (%dBytes)\n", len);

  /* get and verify SLConfigDescrTag */
  if(s.read_char() != MP4SLConfigDescrTag) {
    delete &s;
    return 0;
  }

  /* Note: SLConfig is usually constant value 2, size 1Byte */
  esds->SLConfigLen = len = mp4_read_descr_len(s);
  esds->SLConfig = new uint8_t[esds->SLConfigLen];
  if (esds->SLConfig) {
    s.read(esds->SLConfig, esds->SLConfigLen);
  } else {
    esds->SLConfigLen = 0;
  }
  MSG_V("ESDS MPEG4 Sync Layer Config Descriptor (%dBytes)\n"
      " -> predefined: %d\n", len, esds->SLConfig[0]);

  /* will skip the remainder of the atom */
  delete &s;
  return 0;

}

/* cleanup all mem occupied by mp4_parse_esds */
void mp4_free_esds(esds_t *esds) {
  if(esds->decoderConfigLen)
    delete esds->decoderConfig;
  if(esds->SLConfigLen)
    delete esds->SLConfig;
}

#undef freereturn
#undef MP4_DL

