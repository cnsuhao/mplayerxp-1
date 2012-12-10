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
    typedef struct {
	char *url;
	char *protocol;
	char *hostname;
	char *file;
	unsigned int port;
	char *username;
	char *password;
    } URL_t;

    URL_t* url_new(const std::string& url);
    void   url_free(URL_t* url);

    URL_t *url_redirect(URL_t **url, const char *redir);
    void url2string(char *outbuf, const std::string& inbuf);
    void string2url(char *outbuf, const std::string& inbuf);

#ifdef __URL_DEBUG
    void url_debug(URL_t* url);
#endif // __URL_DEBUG
} // namespace
#endif // __URL_H
