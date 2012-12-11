/*
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __URL_H
#define __URL_H
#include <string>
//#define __URL_DEBUG

namespace mpxp {
    struct URL : public Opaque {
	public:
	    URL();
	    virtual ~URL();

	    const char*	url;
	    char*	protocol;
	    char*	hostname;
	    char*	file;
	    unsigned int port;
	    char*	username;
	    char*	password;
    };

    URL* url_new(const std::string& url);

    URL *url_redirect(URL **url, const std::string& redir);
    void url2string(char *outbuf, const std::string& inbuf);
    void string2url(char *outbuf, const std::string& inbuf);

#ifdef __URL_DEBUG
    void url_debug(URL* url);
#endif // __URL_DEBUG
} // namespace
#endif // __URL_H
