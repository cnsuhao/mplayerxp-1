/*
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __HTTP_H_INCLUDED
#define __HTTP_H_INCLUDED 1

#include "mp_config.h"

namespace mpxp {
    struct HTTP_field_t {
	char *field_name;
	HTTP_field_t *next;
    };

    class HTTP_Header : public Opaque {
	public:
	    HTTP_Header();
	    virtual ~HTTP_Header();

	    virtual int		response_append(const char *data, int length );
	    virtual int		response_parse();
	    virtual int		is_header_entire() const;
	    virtual char*	build_request();
	    virtual char*	get_field(const char *field_name );
	    virtual char*	get_next_field();
	    virtual void	set_field(const char *field_name );
	    virtual void	set_method(const char *method );
	    virtual void	set_uri(const char *uri );
	    virtual int		add_basic_authentication(const char *username, const char *password );

	    virtual void	debug_hdr();
	    virtual void	cookies_set(const char *hostname, const char *url);

	    char*		buffer;
	    size_t		buffer_size;
	    unsigned int	status_code;
	    unsigned char*	body;
	    size_t		body_size;
	    const char*		get_reason_phrase() const { return reason_phrase; }
	    const char*		get_protocol() const { return protocol; }
	private:
	    char*		protocol;
	    char*		method;
	    char*		uri;
	    char*		reason_phrase;
	    unsigned int	http_minor_version;
	    // Field variables
	    HTTP_field_t*	first_field;
	    HTTP_field_t*	last_field;
	    unsigned int	field_nb;
	    char*		field_search;
	    HTTP_field_t*	field_search_pos;
	    // Body variables
	    unsigned int	is_parsed;
    };

    extern int		base64_encode(const any_t*enc, int encLen, char *out, int outMax);
} // namespace mpxp
#endif // __HTTP_H
