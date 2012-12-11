/*
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __HTTP_H_INCLUDED
#define __HTTP_H_INCLUDED 1
#include "mp_config.h"

#include <string>
#include <vector>

namespace mpxp {
    class HTTP_Header : public Opaque {
	public:
	    HTTP_Header();
	    virtual ~HTTP_Header();

	    virtual int		response_append(const char *data, int length );
	    virtual int		response_parse();
	    virtual int		is_header_entire() const;
	    virtual const char*	build_request();
	    virtual const char*	get_field(const char *field_name );
	    virtual const char*	get_next_field();
	    virtual void	set_field(const char *field_name );
	    virtual void	set_method(const char *method );
	    virtual void	set_uri(const char *uri );
	    virtual int		add_basic_authentication(const char *username, const char *password );

	    virtual void	debug_hdr();
	    virtual void	cookies_set(const char *hostname, const char *url);

	    const char*		get_reason_phrase() const { return reason_phrase; }
	    const char*		get_protocol() const { return protocol.c_str(); }
	    unsigned		get_status() const { return status_code; }
	    const char*		get_body() const { return body; }
	    size_t		get_body_size() const { return body_size; }
	    virtual void	erase_body();
	    const char*		get_buffer() const { return buffer; }
	    size_t		get_buffer_size() const { return buffer_size; }
	private:
	    std::string		protocol;
	    std::string		method;
	    std::string		uri;
	    char*		reason_phrase;
	    unsigned int	http_minor_version;
	    // Field variables
	    std::vector<std::string> fields;
	    std::string		field_search;
	    std::vector<std::string>::size_type search_pos;
	    // Body variables
	    unsigned int	status_code;
	    char*		body;
	    size_t		body_size;
	    unsigned int	is_parsed;
	    char*		buffer;
	    size_t		buffer_size;
    };

    extern int		base64_encode(const any_t*enc, int encLen, char *out, int outMax);
} // namespace mpxp
#endif // __HTTP_H
