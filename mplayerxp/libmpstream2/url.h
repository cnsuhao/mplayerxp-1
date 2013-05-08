/*
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __URL_H
#define __URL_H

#include "xmpcore/xmp_enums.h"
#include <string>
//#define __URL_DEBUG

namespace	usr {
    struct URL : public Opaque {
	public:
	    URL(const std::string& url="");
	    virtual ~URL();

	    virtual MPXP_Rc		redirect(const std::string& newurl);
	    virtual MPXP_Rc		check4proxies();
	    virtual MPXP_Rc		clear_login();
	    virtual MPXP_Rc		set_login(const std::string& user,const std::string& passwd);
	    virtual MPXP_Rc		set_port(unsigned);
	    virtual MPXP_Rc		assign_port(unsigned);

	    virtual void		debug() const;

	    virtual const std::string&	url() const;
	    virtual const std::string&	protocol() const;
	    virtual std::string		protocol2lower() const;
	    virtual const std::string&	host() const;
	    virtual const std::string&	file() const;
	    virtual unsigned		port() const;
	    virtual std::string		port2str() const;
	    virtual const std::string&	user() const;
	    virtual const std::string&	password() const;
	private:
	    MPXP_Rc		_build();
	    std::string		_url;
	    std::string		_protocol;
	    std::string		_host;
	    std::string		_file;
	    unsigned		_port;
	    std::string		_user;
	    std::string		_password;
    };

    void url2string(char *outbuf, const std::string& inbuf);
    void string2url(char *outbuf, const std::string& inbuf);
} // namespace
#endif // __URL_H
