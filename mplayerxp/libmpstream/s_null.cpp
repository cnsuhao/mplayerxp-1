#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_null - not a driver.
*/
#include "stream.h"
#include "stream_internal.h"

namespace mpxp {
    class Null_Stream_Interface : public Stream_Interface {
	public:
	    Null_Stream_Interface();
	    virtual ~Null_Stream_Interface();

	    virtual MPXP_Rc	open(libinput_t* libinput,const char *filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual stream_type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
    };

Null_Stream_Interface::Null_Stream_Interface() {}
Null_Stream_Interface::~Null_Stream_Interface() {}

MPXP_Rc Null_Stream_Interface::open(libinput_t*libinput,const char *filename,unsigned flags) {
    UNUSED(libinput);
    UNUSED(filename);
    UNUSED(flags);
    return MPXP_False;
}

int	Null_Stream_Interface::read(stream_packet_t*sp) {  return 0; }
off_t	Null_Stream_Interface::seek(off_t pos) { return pos; }
off_t	Null_Stream_Interface::tell() const { return 0; }
void	Null_Stream_Interface::close() {}
MPXP_Rc Null_Stream_Interface::ctrl(unsigned cmd,any_t*args) {
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}
stream_type_e Null_Stream_Interface::type() const { return STREAMTYPE_STREAM; }
off_t	Null_Stream_Interface::size() const { return 0; }
off_t	Null_Stream_Interface::sector_size() const { return 0; }

static Stream_Interface* query_interface() { return new(zeromem) Null_Stream_Interface; }

extern const stream_interface_info_t null_stream =
{
    "null://",
    "not a driver",
    query_interface,
};
} // namespace mpxp