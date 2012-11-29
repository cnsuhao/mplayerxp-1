#ifndef __STEAM_INTERNAL_H_INCLUDED
#define __STEAM_INTERNAL_H_INCLUDED 1

/** Stream-driver interface */
typedef struct stream_driver_s
{
    const char		*mrl;	/**< MRL of stream driver */
    const char		*descr;	/**< description of the driver */
		/** Opens stream with given name
		  * @param libinput	points libinput2
		  * @param _this	points structure to be filled by driver
		  * @param filename	points MRL of stream (vcdnav://, file://, http://, ...)
		  * @param flags	currently unused and filled as 0
		**/
    MPXP_Rc	(* __FASTCALL__ open)(any_t* libinput,stream_t *_this,const char *filename,unsigned flags);

		/** Reads next packet from stream
		  * @param _this	points structure which identifies stream
		  * @param sp		points to packet where stream data should be stored
		  * @return		length of readed information
		**/
    int		(* __FASTCALL__ read)(stream_t *_this,stream_packet_t * sp);

		/** Seeks on new stream position
		  * @param _this	points structure which identifies stream
		  * @param off		SOF offset from begin of stream
		  * @return		real offset after seeking
		**/
    off_t	(* __FASTCALL__ seek)(stream_t *_this,off_t off);

		/** Tells stream position
		  * @param _this	points structure which identifies stream
		  * @return		current offset from begin of stream
		**/
    off_t	(* __FASTCALL__ tell)(const stream_t *_this);

		/** Closes stream
		  * @param _this	points structure which identifies stream
		**/
    void	(* __FASTCALL__ close)(stream_t *_this);

		/** Pass to driver player's commands (like ioctl)
		  * @param _this	points structure which identifies stream
		  * @param cmd		contains the command (for detail see SCTRL_* definitions)
		  * @return		result of command processing
		**/
    MPXP_Rc	(* __FASTCALL__ control)(const stream_t *_this,unsigned cmd,any_t*param);
}stream_driver_t;

struct stream_info_t {
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  /// mode isn't used atm (ie always READ) but it shouldn't be ignored
  /// opts is at least in it's defaults settings and may have been
  /// altered by url parsing if enabled and the options string parsing.
  int (*open)(struct stream_s* st, int mode, any_t* opts, int* file_format);
  char* protocols[MAX_STREAM_PROTOCOLS];
  any_t* opts;
  int opts_url; /* If this is 1 we will parse the url as an option string
		 * too. Otherwise options are only parsed from the
		 * options string given to open_stream_plugin */
};
#endif
