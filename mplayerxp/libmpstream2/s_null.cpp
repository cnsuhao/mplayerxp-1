#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
    s_null - not a driver.
*/
#include "stream.h"
#include "stream_internal.h"

namespace	usr {
    class Null_Stream_Interface : public Stream_Interface {
	public:
	    Null_Stream_Interface(libinput_t& libinput);
	    virtual ~Null_Stream_Interface();

	    virtual MPXP_Rc	open(const std::string& filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual Stream::type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	    virtual std::string mime_type() const;
    };

Null_Stream_Interface::Null_Stream_Interface(libinput_t&libinput):Stream_Interface(libinput) {}
Null_Stream_Interface::~Null_Stream_Interface() {}

MPXP_Rc Null_Stream_Interface::open(const std::string& filename,unsigned flags) {
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
Stream::type_e Null_Stream_Interface::type() const { return Stream::Type_Stream; }
off_t	Null_Stream_Interface::size() const { return 0; }
off_t	Null_Stream_Interface::sector_size() const { return 0; }
std::string Null_Stream_Interface::mime_type() const { return "application/octet-stream"; }

static Stream_Interface* query_interface(libinput_t& libinput) { return new(zeromem) Null_Stream_Interface(libinput); }

extern const stream_interface_info_t null_stream =
{
    "null://",
    "not a driver",
    query_interface,
};
} // namespace	usr