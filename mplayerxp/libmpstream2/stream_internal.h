#ifndef __STREAM_INTERNAL_H_INCLUDED
#define __STREAM_INTERNAL_H_INCLUDED 1
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <string>

namespace	usr {
    struct libinput_t;

    /** Stream-driver interface */
    class Stream_Interface : public Opaque {
	public:
	    Stream_Interface(libinput_t&) {}
	    virtual ~Stream_Interface() {}
		/** Opens stream with given name
		  * @param libinput	points libinput2
		  * @param _this	points structure to be filled by driver
		  * @param filename	points MRL of stream (vcdnav://, file://, http://, ...)
		  * @param flags	currently unused and filled as 0
		**/
	    virtual MPXP_Rc	open(const std::string& filename,unsigned flags) = 0;

		/** Reads next packet from stream
		  * @param _this	points structure which identifies stream
		  * @param sp		points to packet where stream data should be stored
		  * @return		length of readed information
		**/
	    virtual int		read(stream_packet_t * sp) = 0;

		/** Seeks on new stream position
		  * @param _this	points structure which identifies stream
		  * @param off		SOF offset from begin of stream
		  * @return		real offset after seeking
		**/
	    virtual off_t	seek(off_t off) = 0;

		/** Tells stream position
		  * @param _this	points structure which identifies stream
		  * @return		current offset from begin of stream
		**/
	    virtual off_t	tell() const = 0;

		/** Closes stream
		  * @param _this	points structure which identifies stream
		**/
	    virtual void	close() = 0;

		/** Pass to driver player's commands (like ioctl)
		  * @param _this	points structure which identifies stream
		  * @param cmd		contains the command (for detail see SCTRL_* definitions)
		  * @return		result of command processing
		**/
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param) = 0;
		/** Return type of stream */
	    virtual Stream::type_e	type() const = 0;
		/** Return length of stream or -1 - if unknown */
	    virtual off_t		start_pos() const { return 0; }
		/** Return length of stream or -1 - if unknown */
	    virtual off_t		size() const = 0;
		/** Return size of sector of stream */
	    virtual off_t		sector_size() const = 0;
	    virtual float		stream_pts() const { return 0; }
	    virtual std::string		mime_type() const = 0;
    };

    /** Stream-driver interface */
    struct stream_interface_info_t {
	const char*		mrl;	/**< MRL of stream driver */
	const char*		descr;	/**< description of the driver */
	Stream_Interface*	(*query_interface)(libinput_t&);
    };
} // namespace	usr
#endif
