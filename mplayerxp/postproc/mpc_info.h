#ifndef MP_CODEC_INFO_T
#define MP_CODEC_INFO_T

typedef struct mp_codec_info_s
{
	/* codec long name ("Autodesk FLI/FLC Animation decoder" */
	const char *name;
	/* short name (same as driver name in codecs.conf) ("dshow") */
	const char *short_name;
	/* interface author/maintainer */
	const char *maintainer;
	/* codec author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
	const char *author;
	/* any additional comments */
	const char *comment;
} mp_codec_info_t;

#endif
