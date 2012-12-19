#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "nls/nls.h"
#include "mpxp_msg.h"

namespace mpxp {
mpxp_streambuf::mpxp_streambuf(mpxp_ostream& _parent,const std::string& _data)
		:data(_data)
		,parent(_parent)
{
    setp(buf, buf + BUF_SIZE);
}

mpxp_streambuf::~mpxp_streambuf() {}

int mpxp_streambuf::overflow(int c) {
    // Handle output
    put_chars(pbase(), pptr());
    if (c != Traits::eof()) {
	char c2 = c;
	// Handle the one character that didn't fit to buffer
	put_chars(&c2, &c2 + 1);
    }
    // This tells that buffer is empty again
    setp(buf, buf + BUF_SIZE);
    // I'm not sure about this return value!
    return 0;
}

int mpxp_streambuf::sync() {
    // Handle output
    put_chars(pbase(), pptr());
    // This tells that buffer is empty again
    setp(buf, buf + BUF_SIZE);
    return 0;
}

void mpxp_streambuf::put_chars(char const* begin, char const* end) const {
    if(!(parent._type&mp_conf.msg_filter)) { parent.setstate(std::ios_base::badbit); return; }
    if(::isatty(::fileno(::stderr))) std::cerr<<data;
    ::fwrite(begin,end-begin,1,::stderr);
}

static const char* msg_prefix[] = {
    "GLOBAL",
    "PLAYER",
    "LIBVO",
    "LIBAO",
    "DEMUX",
    "PARSER",
    "DECAUD",
    "DECVID",
    "MPSUB",
    "OSDEP",
    "PLAYTR",
    "INPUT",
    "OSD",
    "CPUDTC",
    "CODCFG",
    "SWS",
    "POSTPR",
    "NLS",
    "STREAM",
    "UNKNOWN"
};

mpxp_ostream::mpxp_ostream(const std::string& data,mpxp_msgt_e type)
	    :std::basic_ostream< char, std::char_traits< char > >(&buf)
	    ,_type(type)
	    ,idx(compute_idx(type))
	    ,buf(*this,mp_conf.verbose>1?data+msg_prefix[idx]+": ":data) {}
mpxp_ostream::~mpxp_ostream() {}

unsigned mpxp_ostream::compute_idx(mpxp_msgt_e type) const {
    unsigned mod_idx=0,_idx=type;
    while((_idx&0x1)==0) { mod_idx++; _idx>>=1; }
    return std::min(_idx,unsigned(sizeof(msg_prefix)/sizeof(msg_prefix[0])));
}

/* TODO: replace this block with std::string */
static const char blue[]="\033[0;34;40m";
static const char green[]="\033[0;32;40m";
static const char cyan[]="\033[0;36;40m";
static const char red[]="\033[0;31;40m";
static const char magenta[]="\033[0;35;40m";
static const char brown[]="\033[0;33;40m";
static const char gray[]="\033[0;37;40m";
static const char light_blue[]="\033[1;34;40m";
static const char light_green[]="\033[1;32;40m";
static const char light_cyan[]="\033[1;36;40m";
static const char light_red[]="\033[1;31;40m";
static const char light_magenta[]="\033[1;35;40m";
static const char yellow[]="\033[1;33;40m";
static const char white[]="\033[1;37;40m";

mpxp_ostream_info::mpxp_ostream_info(mpxp_msgt_e type):mpxp_ostream(gray,type){}
mpxp_ostream_info::~mpxp_ostream_info() {}

mpxp_ostream_fatal::mpxp_ostream_fatal(mpxp_msgt_e type):mpxp_ostream(light_red,type){}
mpxp_ostream_fatal::~mpxp_ostream_fatal() {}

mpxp_ostream_err::mpxp_ostream_err(mpxp_msgt_e type):mpxp_ostream(red,type){}
mpxp_ostream_err::~mpxp_ostream_err() {}

mpxp_ostream_warn::mpxp_ostream_warn(mpxp_msgt_e type):mpxp_ostream(yellow,type){}
mpxp_ostream_warn::~mpxp_ostream_warn() {}

mpxp_ostream_ok::mpxp_ostream_ok(mpxp_msgt_e type):mpxp_ostream(light_green,type){}
mpxp_ostream_ok::~mpxp_ostream_ok() {}

mpxp_ostream_hint::mpxp_ostream_hint(mpxp_msgt_e type):mpxp_ostream(light_cyan,type){}
mpxp_ostream_hint::~mpxp_ostream_hint() {}

mpxp_ostream_status::mpxp_ostream_status(mpxp_msgt_e type):mpxp_ostream(light_blue,type){}
mpxp_ostream_status::~mpxp_ostream_status() {}

mpxp_ostream_v::mpxp_ostream_v(mpxp_msgt_e type):mpxp_ostream(cyan,type){ if(mp_conf.verbose<1) setstate(ios_base::badbit); /* do not display */ }
mpxp_ostream_v::~mpxp_ostream_v() {}

mpxp_ostream_dbg2::mpxp_ostream_dbg2(mpxp_msgt_e type):mpxp_ostream(gray,type){ if(mp_conf.verbose<2) setstate(ios_base::badbit); /* do not display */ }
mpxp_ostream_dbg2::~mpxp_ostream_dbg2() {}

mpxp_ostream_dbg3::mpxp_ostream_dbg3(mpxp_msgt_e type):mpxp_ostream(gray,type){ if(mp_conf.verbose<3) setstate(ios_base::badbit); /* do not display */ }
mpxp_ostream_dbg3::~mpxp_ostream_dbg3() {}

mpxp_ostream_dbg4::mpxp_ostream_dbg4(mpxp_msgt_e type):mpxp_ostream(gray,type){ if(mp_conf.verbose<4) setstate(ios_base::badbit); /* do not display */ }
mpxp_ostream_dbg4::~mpxp_ostream_dbg4() {}

/* old stuff: */

inline int _bg(int x) { return x >> 4; }
inline int _fg(int x) { return x & 0x0f; }
struct priv_t {
    int		_color[8];
    char	vtmp[100];
    char	scol[9][20];
    pthread_mutex_t mp_msg_mutex;
};
const char hl[9] = { 0xC, 0x4, 0xE, 0xA, 0xB, 0x7, 0x9, 0x3, 0x7 };

static char *_2ansi(unsigned char attr)
{
    priv_t*priv=reinterpret_cast<priv_t*>(mpxp_context().msg_priv);
    int bg = _bg(attr);
    int bc = priv->_color[bg & 7];

    sprintf(priv->vtmp,
	"\033[%d;3%d;4%d%sm",
	_fg(attr) > 7,
	priv->_color[_fg(attr) & 7],
	bc,
	bg > 7 ? ";5" : ""
    );
    return priv->vtmp;
}

void mpxp_print_init(int verbose)
{
    unsigned i;
    int _color[8]={0,4,2,6,1,5,3,7};
    priv_t*priv=new(zeromem) priv_t;
    mpxp_context().msg_priv=priv;
    memcpy(priv->_color,_color,sizeof(_color));
    pthread_mutex_init(&priv->mp_msg_mutex,NULL);
    for(i=0;i<sizeof(hl)/sizeof(char);i++) memcpy(priv->scol[i],_2ansi(hl[i]),sizeof(priv->scol[0]));
}

void mpxp_print_uninit(void)
{
    priv_t*priv=reinterpret_cast<priv_t*>(mpxp_context().msg_priv);
    if(isatty(fileno(stderr))) fprintf(stderr,priv->scol[8]);
    mpxp_print_flush();
    pthread_mutex_destroy(&priv->mp_msg_mutex);
    delete priv;
}

int mpxp_printf( unsigned x, const std::string& format, ... ){
/* TODO: more useful usage of module_id */
    int rc=0;
    char* sbuf=new char[0xFFFFF];
    unsigned ssize;
    unsigned level=(x>>28)&0xF;
    unsigned mod=x&0x0FFFFFFF;
    static int was_eol=1;
    priv_t*priv=NULL;
    priv=reinterpret_cast<priv_t*>(mpxp_context().msg_priv);
    if(level>mp_conf.verbose+MSGL_V-1) return 0; /* do not display */
    if((mod&mp_conf.msg_filter)==0) return 0; /* do not display */
    if(priv) {
//	pthread_mutex_lock(&priv->mp_msg_mutex);
	if(::isatty(::fileno(::stderr)))
	    ::fprintf(::stderr,priv->scol[level<9?level:8]);
    }
    if(mp_conf.verbose>1 && was_eol)
    {
	unsigned mod_name;
	const char *smod=NULL;
	mod_name = 0;
	while((mod&0x1)==0) { mod_name++; mod>>=1; }
	if(mod_name < sizeof(msg_prefix)/sizeof(msg_prefix[0]))
		smod = msg_prefix[mod_name];
	fprintf(stderr,"%s: ",smod?smod:"UNKNOWN");
    }
    va_list va;
    va_start(va, format);
    ssize=vsprintf(sbuf,format.c_str(), va);
    va_end(va);
    if(strcmp(nls_get_screen_cp(),"UTF-8")!=0) {
	char *obuf;
	obuf=nls_recode2screen_cp("UTF-8",sbuf,ssize);
	rc=fputs(obuf,stderr);
	delete obuf;
    }
    else rc=fputs(sbuf,stderr);
    if(format[format.length()-1]=='\n') was_eol=1;
    else was_eol=0;
    fflush(stderr);
//    if(priv) pthread_mutex_unlock(&priv->mp_msg_mutex);
    delete sbuf;
    return rc;
}

void mpxp_print_flush(void) { fflush(stderr); }
} //namespace mpxp
