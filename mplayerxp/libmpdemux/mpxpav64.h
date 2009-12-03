/* MPLayerXP's AudioVideo64 file format */
#ifndef MPXPAV64_INCLUDED
#define MPXPAV64_INCLUDED 1

#define MPXPAV64_FP_FCNT_UTF7		0x0000000000000000ULL
#define MPXPAV64_FP_FCNT_UTF8		0x0000000000000001ULL
#define MPXPAV64_FP_FCNT_UTF16		0x0000000000000002ULL
#define MPXPAV64_FP_FCNT_UTF32		0x0000000000000003ULL

typedef struct __attribute__((__packed__)) mpxpav64FileProperties_s /* FPRP */
{
    uint64_t num_packets;  //Number of 'DATx' packets
    uint64_t num_bytes;    //Number of 'DATx' bytes
    uint64_t flags;        //Flags
    uint64_t PlayDuration; //Timestamp of the end position
    uint32_t Preroll;      //Timestamp of the first position
    uint32_t MaxBitrate;   //Maximum bitrate of the media (sum of all the stream)
    uint32_t AveBitrate;   //Average bitrate of the media (sum of all the stream)
    uint16_t StreamCount;  //Number of StreamProp Objects
}mpxpav64FileProperties_t;
#define le2me_mpxpav64FileProperties(h) {				\
    (h)->num_packets = le2me_64((h)->num_packets);			\
    (h)->num_bytes = le2me_64((h)->num_bytes);				\
    (h)->flags = le2me_64((h)->flags);					\
    (h)->PlayDuration = le2me_64((h)->PlayDuration);			\
    (h)->Preroll = le2me_32((h)->Preroll);				\
    (h)->MaxBitrate = le2me_32((h)->MaxBitrate);			\
    (h)->AveBitrate = le2me_32((h)->AveBitrate);			\
    (h)->StreamCount = le2me_16((h)->StreamCount);			\
}

typedef struct __attribute__((__packed__)) mpxpav64StreamProperties_s
{
    uint64_t num_packets;  //Number of 'DATx' packets of this stream type
    uint64_t num_bytes;    //Number of 'DATx' bytes of this stream type
    uint64_t flags;        //Flags
    uint64_t PlayDuration; //Timestamp of the end position
    uint32_t Preroll;      //Timestamp of the first position
    uint32_t MaxPacketSize;//Maximum packet size
    uint32_t AvePacketSize;//Average packet size
    uint32_t MinPacketSize;//Average packet size
    uint32_t MaxFrameDuration;//Maximum packet size
    uint32_t AveFrameDuration;//Average packet size
    uint32_t MinFrameDuration;//Average packet size
    uint32_t MaxBitrate;   //Maximum bitrate of the media
    uint32_t AveBitrate;   //Average bitrate of the media
    uint32_t MinBitrate;   //Average bitrate of the media
    uint64_t pts_rate;     //Denominator of PTS fields to get time in seconds (default: 1000)
    uint64_t size_scaler;  //Numerator of SIZE fields to get size in bytes (default: 1)
    uint8_t  mimetype_len; //length of mime-type
    uint8_t  ascii[0];//mime-type: video/x-video audio/x-audio text/x-text
}mpxpav64StreamProperties_t;
#define le2me_mpxpav64StreamProperties(h) {				\
    (h)->num_packets = le2me_64((h)->num_packets);			\
    (h)->num_bytes = le2me_64((h)->num_bytes);				\
    (h)->flags = le2me_64((h)->flags);					\
    (h)->PlayDuration = le2me_64((h)->PlayDuration);			\
    (h)->Preroll = le2me_32((h)->Preroll);				\
    (h)->MaxPacketSize = le2me_32((h)->MaxPacketSize);			\
    (h)->AvePacketSize = le2me_32((h)->AvePacketSize);			\
    (h)->MinPacketSize = le2me_32((h)->MinPacketSize);			\
    (h)->MaxFrameDuration = le2me_32((h)->MaxFrameDuration);		\
    (h)->AveFrameDuration = le2me_32((h)->AveFrameDuration);		\
    (h)->MinFrameDuration = le2me_32((h)->MinFrameDuration);		\
    (h)->MaxBitrate = le2me_32((h)->MaxBitrate);			\
    (h)->AveBitrate = le2me_32((h)->AveBitrate);			\
    (h)->MinBitrate = le2me_32((h)->MinBitrate);			\
    (h)->pts_rate = le2me_64((h)->pts_rate);				\
    (h)->size_scaler = le2me_64((h)->size_scaler);			\
}

#endif
