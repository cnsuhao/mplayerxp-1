#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 *
 */
#include <algorithm>
#include <limits>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

#include "network.h"
#include "url.h"
#include "mpxp_help.h"
#include "stream_msg.h"
#include "mplayerxp.h"

namespace mpxp {
URL::URL(const std::string& __url):_url(__url) { _build(); }
URL::~URL() {}
MPXP_Rc URL::redirect(const std::string& _redir) { _url=_redir; return _build(); }
MPXP_Rc URL::_build() {
    size_t pos1, pos2, pos3, pos4;
    int v6addr = 0;
    char *escfilename=NULL;
    int jumpSize = 3;

    if( _url.empty()) {
    err_out:
	if (escfilename) delete escfilename;
	_protocol.clear();
	_host.clear();
	_file.clear();
	_port=0;
	_user.clear();
	_password.clear();
	return MPXP_False;
    }
    escfilename=new char [_url.length()*3+1];
    string2url(escfilename,_url);
    // Copy the url in the URL container
    _url = escfilename;
    mpxp_v<<"Filename for url is now "<<escfilename<<std::endl;

    // extract the protocol
    pos1 = _url.find("://");
    if( pos1==std::string::npos ) {
	// Check for a special case: "sip:" (without "//"):
	if (_url.substr(0,4)=="sip:") {
	    pos1 = 3; // points to ':'
	    jumpSize = 1;
	} else {
	    mpxp_v<<"Not an URL!"<<std::endl;
	    goto err_out;
	}
    }
    _protocol=_url.substr(0,pos1);
    // jump the "://"
    pos1 += jumpSize;
    // check if a username:password is given
    pos2 = _url.find("@",pos1);
    pos3 = _url.find("/",pos1);
    if( pos3!=std::string::npos && pos3<pos2 ) {
	// it isn't really a username but rather a part of the path
	pos2 = std::string::npos;
    }
    if( pos2!=std::string::npos ) {
	// We got something, at least a username...
	size_t len = pos2-pos1;
	_user=_url.substr(pos1, len);

	pos3 = _url.find(":",pos1);
	if( pos3!=std::string::npos && pos3<pos2 ) {
	    // We also have a password
	    size_t len2 = pos2-pos3-1;
	    _password=_url.substr(pos3+1,len2);
	}
	pos1 = pos2+1;
    }
    // before looking for a port number check if we have an IPv6 type numeric address
    // in IPv6 URL the numeric address should be inside square braces.
    pos2 = _url.find("[",pos1);
    pos3 = _url.find("]",pos1);
    pos4 = _url.find("/",pos1);
    if( pos2!=std::string::npos && pos3!=std::string::npos && pos2 < pos3 && pos4 > pos3) {
	// we have an IPv6 numeric address
	pos1++;
	pos2 = pos3;
	v6addr = 1;
    } else pos2 = pos1;
    // look if the port is given
    pos2 = _url.find(":",pos2);
    // If the : is after the first / it isn't the port
    pos3 = _url.find("/",pos1);
    if(pos3!=std::string::npos && int(pos3)-int(pos2) < 0) pos2 = std::string::npos;
    if( pos2==std::string::npos ) {
	// No port is given
	// Look if a path is given
	if( pos3==std::string::npos ) {
	    // No path/filename
	    // So we have an URL like http://www.hostname.com
	    pos2 = _url.length();
	} else {
	    // We have an URL like http://www.hostname.com/file.txt
	    pos2 = pos3;
	}
    } else {
	    // We have an URL beginning like http://www.hostname.com:1212
	    // Get the port number
	    _port = ::atoi(_url.substr(pos2+1).c_str());
    }
    if( v6addr ) pos2--;
    // copy the hostname in the URL container
    _host = _url.substr(pos1, pos2-pos1);
    // Look if a path is given
    pos2 = _url.find("/",pos1);
    if( pos2!=std::string::npos ) {
	// A path/filename is given
	// check if it's not a trailing '/'
	if( _url.length()>pos2+1 ) {
	    // copy the path/filename in the URL container
	    _file = _url.substr(pos2);
	}
    }
    // Check if a filename was given or set, else set it with '/'
    if( _file.empty()) _file="/";
    delete escfilename;
    return MPXP_Ok;
}

MPXP_Rc URL::clear_login() { _user.clear(); _password.clear(); return MPXP_Ok; }
MPXP_Rc URL::set_login(const std::string& usr,const std::string& passwd) {
    _user=usr;
    _password=passwd;
    return MPXP_Ok;
}
MPXP_Rc URL::set_port(unsigned p) { _port=p; return MPXP_Ok; }
MPXP_Rc URL::assign_port(unsigned p) { if(!_port) { _port=p; return MPXP_Ok; } return MPXP_False; }

const std::string& URL::url() const { return _url; }
const std::string& URL::protocol() const { return _protocol; }
const std::string& URL::host() const { return _host; }
const std::string& URL::file() const { return _file; }
unsigned URL::port() const { return _port; }
std::string URL::port2str() const {
    char tmp[100];
    sprintf(tmp,"%d",_port);
    return std::string(tmp);
}
const std::string& URL::user() const { return _user; }
const std::string& URL::password() const { return _password; }

std::string URL::protocol2lower() const {
    std::string p=_protocol;
    std::transform(p.begin(),p.end(),p.begin(), ::tolower);
    return p;
}

MPXP_Rc URL::check4proxies() {
    if( protocol2lower()=="http_proxy") {
	mpxp_v<<"Using HTTP proxy: http://"<<_host<<":"<<_port<<std::endl;
	return MPXP_Ok;
    }
    // Check if the http_proxy environment variable is set.
    if( protocol2lower()=="http") {
	const char *proxy;
	proxy = getenv("http_proxy");
	if( proxy!=NULL ) {
	    std::string new_url=proxy;
	    URL proxy_url(new_url);
#ifdef HAVE_AF_INET6
	    if (net_conf.ipv4_only_proxy && (::gethostbyname(_host.c_str())==NULL)) {
		mpxp_warn<<"Could not find resolve remote hostname for AF_INET. Trying without proxy"<<std::endl;
		return MPXP_Ok;
	    }
#endif
	    mpxp_v<<"Using HTTP proxy: "<<new_url<<std::endl;
	    new_url=std::string("http_proxy://")+proxy_url.host()+":"+proxy_url.port2str()+"/"+_url;
	    if(proxy_url.redirect(new_url)!=MPXP_Ok) {
		mpxp_warn<<"Invalid proxy setting... Trying without proxy"<<std::endl;
		return MPXP_Ok;
	    }
	    *this=proxy_url;
	}
    }
    return MPXP_Ok;
}


/* Replace escape sequences in an URL (or a part of an URL) */
/* works like strcpy(), but without return argument */
void
url2string(char *outbuf, const std::string& inbuf)
{
	unsigned char c,c1,c2;
	int i,len=inbuf.length();
	for (i=0;i<len;i++){
		c = inbuf[i];
		if (c == '%' && i<len-2) { //must have 2 more chars
			c1 = toupper(inbuf[i+1]); // we need uppercase characters
			c2 = toupper(inbuf[i+2]);
			if (	((c1>='0' && c1<='9') || (c1>='A' && c1<='F')) &&
				((c2>='0' && c2<='9') || (c2>='A' && c2<='F')) ) {
				if (c1>='0' && c1<='9') c1-='0';
				else c1-='A'-10;
				if (c2>='0' && c2<='9') c2-='0';
				else c2-='A'-10;
				c = (c1<<4) + c2;
				i=i+2; //only skip next 2 chars if valid esc
			}
		}
		*outbuf++ = c;
	}
	*outbuf++='\0'; //add nullterm to string
}

static void
url_escape_string_part(char *outbuf, const char *inbuf) {
	unsigned char c,c1,c2;
	int i,len=strlen(inbuf);

	for  (i=0;i<len;i++) {
		c = inbuf[i];
		if ((c=='%') && i<len-2 ) { //need 2 more characters
		    c1=toupper(inbuf[i+1]); c2=toupper(inbuf[i+2]); // need uppercase chars
		   } else {
		    c1=129; c2=129; //not escape chars
		   }

		if(	(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			(c >= 0x7f)) {
			*outbuf++ = c;
		} else if ( c=='%' && ((c1 >= '0' && c1 <= '9') || (c1 >= 'A' && c1 <= 'F')) &&
			   ((c2 >= '0' && c2 <= '9') || (c2 >= 'A' && c2 <= 'F'))) {
							      // check if part of an escape sequence
			    *outbuf++=c;                      // already

							      // dont escape again
			    mpxp_err<<"URL String already escaped: "<<c<<" "<<c1<<" "<<c2<<std::endl;
							      // error as this should not happen against RFC 2396
							      // to escape a string twice
		} else {
			/* all others will be escaped */
			c1 = ((c & 0xf0) >> 4);
			c2 = (c & 0x0f);
			if (c1 < 10) c1+='0';
			else c1+='A'-10;
			if (c2 < 10) c2+='0';
			else c2+='A'-10;
			*outbuf++ = '%';
			*outbuf++ = c1;
			*outbuf++ = c2;
		}
	}
	*outbuf++='\0';
}

/* Replace specific characters in the URL string by an escape sequence */
/* works like strcpy(), but without return argument */
void
string2url(char *outbuf, const std::string& _inbuf) {
    char* inbuf=mp_strdup(_inbuf.c_str());
    int i = 0,j,len = strlen(inbuf);
    char* tmp,*in;
    char *unesc = NULL;

	// Look if we have an ip6 address, if so skip it there is
	// no need to escape anything in there.
	tmp = strstr(inbuf,"://[");
	if(tmp) {
		tmp = strchr(tmp+4,']');
		if(tmp && (tmp[1] == '/' || tmp[1] == ':' ||
			   tmp[1] == '\0')) {
			i = tmp+1-inbuf;
			strncpy(outbuf,inbuf,i);
			outbuf += i;
			tmp = NULL;
		}
	}

	while(i < len) {
		unsigned char c='\0';
		// look for the next char that must be kept
		for  (j=i;j<len;j++) {
			c = inbuf[j];
			if(c=='-' || c=='_' || c=='.' || c=='!' || c=='~' ||	/* mark characters */
			   c=='*' || c=='\'' || c=='(' || c==')' || 	 	/* do not touch escape character */
			   c==';' || c=='/' || c=='?' || c==':' || c=='@' || 	/* reserved characters */
			   c=='&' || c=='=' || c=='+' || c=='$' || c==',') 	/* see RFC 2396 */
				break;
		}
		// we are on a reserved char, write it out
		if(j == i) {
			*outbuf++ = c;
			i++;
			continue;
		}
		// we found one, take that part of the string
		if(j < len) {
			if(!tmp) tmp = new char [len+1];
			strncpy(tmp,inbuf+i,j-i);
			tmp[j-i] = '\0';
			in = tmp;
		} else // take the rest of the string
			in = (char*)inbuf+i;

		if(!unesc) unesc = new char [len+1];
		// unescape first to avoid escaping escape
		url2string(unesc,in);
		// then escape, including mark and other reserved chars
		// that can come from escape sequences
		url_escape_string_part(outbuf,unesc);
		outbuf += strlen(outbuf);
		i += strlen(in);
	}
    *outbuf = '\0';
    if(tmp) delete tmp;
    if(unesc) delete unesc;
    delete inbuf;
}

void URL::debug() const {
    mpxp_v<<"url="<<_url<<std::endl;
    mpxp_v<<"protocol="<<_protocol<<std::endl;
    mpxp_v<<"hostname="<<_host<<std::endl;
    mpxp_v<<"port="<<_port<<std::endl;
    mpxp_v<<"file="<<_file<<std::endl;
    mpxp_v<<"username="<<_user<<std::endl;
    mpxp_v<<"password="<<_password<<std::endl;
}
} // namespace mpxp
