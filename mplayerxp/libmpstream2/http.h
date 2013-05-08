/*
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __HTTP_H_INCLUDED
#define __HTTP_H_INCLUDED 1
#include "mpxp_config.h"

#include <string>
#include <vector>
#include <stdint.h>

#include "url.h"

namespace	usr {
    class HTTP_Header : public Opaque {
	public:
	    HTTP_Header();
	    virtual ~HTTP_Header();

	    virtual int		response_append(const uint8_t* data,size_t length);
	    virtual int		response_parse();
	    virtual int		is_header_entire() const;
	    virtual const char*	build_request();
	    virtual const char*	get_field(const std::string& field_name );
	    virtual const char*	get_next_field();
	    virtual void	set_field(const std::string& field_name );
	    virtual void	set_method(const std::string& method );
	    virtual void	set_uri(const std::string& uri );
	    virtual int		add_basic_authentication(const std::string& username, const std::string& password );
	    virtual int		authenticate(URL& url, int *auth_retry);

	    virtual void	debug_hdr();
	    virtual void	cookies_set(const std::string& hostname, const std::string& url);

	    const char*		get_reason_phrase() const { return reason_phrase.c_str(); }
	    const char*		get_protocol() const { return protocol.c_str(); }
	    unsigned		get_status() const { return status_code; }
	    const char*		get_body() const { return body.c_str(); }
	    size_t		get_body_size() const { return body.length(); }
	    virtual void	erase_body();
	    const uint8_t*	get_buffer() const { return buffer; }
	    size_t		get_buffer_size() const { return buffer_size; }
	private:
	    std::string		protocol;
	    std::string		method;
	    std::string		uri;
	    std::string		reason_phrase;
	    unsigned int	http_minor_version;
	    // Field variables
	    std::vector<std::string> fields;
	    std::string		field_search;
	    std::vector<std::string>::size_type search_pos;
	    // Body variables
	    unsigned int	status_code;
	    std::string		body;
	    unsigned int	is_parsed;
	    uint8_t*		buffer;
	    unsigned		buffer_size;
    };

    extern int		base64_encode(const any_t*enc, int encLen, char *out, int outMax);
} // namespace	usr
#endif // __HTTP_H
