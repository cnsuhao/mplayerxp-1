#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * This file was ported to MPlayer from xine CVS rtsp.c,v 1.9 2003/04/10 02:30:48
 */

/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a mp_free video player.
 *
 * xine is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * a minimalistic implementation of rtsp protocol,
 * *not* RFC 2326 compilant yet.
 *
 *    2006, Benjamin Zores and Vincent Mussard
 *      fixed a lot of RFC compliance issues.
 */
#include <stdexcept>

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "mplayerxp.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>

#include "tcp.h"
#include "rtsp.h"
#include "rtsp_session.h"
#include "osdep/timer.h"
#include "stream_msg.h"

namespace	usr {
/*
#define LOG
*/

enum {
   BUF_SIZE=4096,
   HEADER_SIZE=1024
};

/*
 * constants
 */

#define RTSP_PROTOCOL_VERSION "RTSP/1.0"

/* server states */
enum {
    RTSP_CONNECTED=1,
    RTSP_INIT=2,
    RTSP_READY=4,
    RTSP_PLAYING=8,
    RTSP_RECORDING=16
};

/* server capabilities */
enum {
    RTSP_OPTIONS=0x001,
    RTSP_DESCRIBE=0x002,
    RTSP_ANNOUNCE=0x004,
    RTSP_SETUP=0x008,
    RTSP_GET_PARAMETER=0x010,
    RTSP_SET_PARAMETER=0x020,
    RTSP_TEARDOWN=0x040,
    RTSP_PLAY=0x080,
    RTSP_RECORD=0x100
};

Rtsp::Rtsp(Tcp& _tcp):tcp(_tcp) {}
Rtsp::~Rtsp() {
  if (path) delete path;
  if (host) delete host;
  if (mrl) delete mrl;
  if (session) delete session;
  if (user_agent) delete user_agent;
  free_answers();
  unschedule_all();
}

/*
 * network utilities
 */

int Rtsp::write_stream(const char *buf, int len) const {
  int total, timeout;

  total = 0; timeout = 30;
  while (total < len){
    int n;

    n = tcp.write((uint8_t*)(&buf[total]), len - total);

    if (n > 0)
      total += n;
    else if (n < 0) {
#ifndef HAVE_WINSOCK2
      if ((timeout>0) && ((errno == EAGAIN) || (errno == EINPROGRESS))) {
#else
      if ((timeout>0) && ((errno == EAGAIN) || (WSAGetLastError() == WSAEINPROGRESS))) {
#endif
	usec_sleep (1000000); timeout--;
      } else
	return -1;
    }
  }

  return total;
}

ssize_t Rtsp::read_stream(any_t*buf, size_t count) const {

  ssize_t ret, total;

  total = 0;

  while (total < ssize_t(count)) {

    ret=tcp.read(((uint8_t*)buf)+total, count-total);

    if (ret<0) {
      if(errno == EAGAIN) if(!tcp.has_data(0)) return -1;
      continue;
    } else total += ret;

    /* end of stream */
    if (!ret) break;
  }

  return total;
}

/*
 * rtsp_get gets a line from stream
 * and returns a null terminated string.
 */

char* Rtsp::get() const {
    int n=1;
    char *buffer = new char [BUF_SIZE];
    char *string = NULL;

    read_stream(buffer, 1);
    while (n<BUF_SIZE) {
	read_stream(&(buffer[n]), 1);
	if ((buffer[n-1]==0x0d)&&(buffer[n]==0x0a)) break;
	n++;
    }

    if (n>=BUF_SIZE) throw std::runtime_error("librtsp: buffer overflow in rtsp_get");

    string=new char [n];
    memcpy(string,buffer,n-1);
    string[n-1]=0;

#ifdef LOG
    mpxp_info<<"librtsp: << "<<string<<std::endl;
#endif

    delete buffer;
    return string;
}

/*
 * rtsp_put puts a line on stream
 */

void Rtsp::put(const char *string) const {
    int len=strlen(string);
    char *buf=new char [len+2];

#ifdef LOG
    mpxp_info<<"librtsp: >> "<<string<<std::endl;
#endif

    memcpy(buf,string,len);
    buf[len]=0x0d;
    buf[len+1]=0x0a;

    write_stream(buf, len+2);

#ifdef LOG
    mpxp_info>>" done"<<std::endl;
#endif
    delete buf;
}

/*
 * extract server status code
 */

int Rtsp::get_code(const char *string) const {
    char buf[4];
    int code=0;

    if (!strncmp(string, RTSP_PROTOCOL_VERSION, strlen(RTSP_PROTOCOL_VERSION))) {
	memcpy(buf, string+strlen(RTSP_PROTOCOL_VERSION)+1, 3);
	buf[3]=0;
	code=atoi(buf);
    } else if (!strncmp(string, RTSP_METHOD_SET_PARAMETER,8)) {
	return RTSP_STATUS_SET_PARAMETER;
    }
    if(code != RTSP_STATUS_OK) mpxp_info<<"librtsp: server responds: "<<string<<std::endl;
    return code;
}

/*
 * send a request
 */

void Rtsp::send_request(const char *type, const char *what) {
    char *const *payload=scheduled;
    char *buf;

    buf = new char [strlen(type)+strlen(what)+strlen(RTSP_PROTOCOL_VERSION)+3];

    sprintf(buf,"%s %s %s",type, what, RTSP_PROTOCOL_VERSION);
    put(buf);
    delete buf;
    if (payload)
	while (*payload) {
	    put(*payload);
	    payload++;
	}
    put("");
    unschedule_all();
}

/*
 * schedule standard fields
 */

void Rtsp::schedule_standard() {
    char tmp[17];

    snprintf(tmp, 17, "CSeq: %u", cseq);
    schedule_field(tmp);

    if (session) {
	char *buf;
	buf = new char [strlen(session)+15];
	sprintf(buf, "Session: %s", session);
	schedule_field(buf);
	delete buf;
    }
}
/*
 * get the answers, if server responses with something != 200, return NULL
 */

int Rtsp::get_answers() {
    char *answer=NULL;
    unsigned int answer_seq;
    char **answer_ptr=answers;
    int code;
    int ans_count = 0;

    answer=get();
    if (!answer) return 0;
    code=get_code(answer);
    delete answer;

    free_answers();

    do { /* while we get answer lines */
	answer=get();
	if (!answer) return 0;

	if (!strncasecmp(answer,"CSeq:",5)) {
	    sscanf(answer,"%*s %u",&answer_seq);
	    if (cseq != answer_seq) {
#ifdef LOG
		mpxp_warn<<"librtsp: warning: CSeq mismatch. got "<<answer_seq<<", assumed "<<s->cseq<<std::endl;
#endif
	    cseq=answer_seq;
	    }
	}
	if (!strncasecmp(answer,"Server:",7)) {
	    char *buf = new char [strlen(answer)];
	    sscanf(answer,"%*s %s",buf);
	    if (server) delete server;
	    server=mp_strdup(buf);
	    delete buf;
	}
	if (!strncasecmp(answer,"Session:",8)) {
	    char *buf = new(zeromem) char [strlen(answer)];
	    sscanf(answer,"%*s %s",buf);
	    if (session) {
		if (strcmp(buf, session)) {
		    mpxp_warn<<"rtsp: warning: setting NEW session: "<<buf<<std::endl;
		    delete session;
		    session=mp_strdup(buf);
		}
	    } else {
#ifdef LOG
		mpxp_info<<"rtsp: setting session id to: "<<buf<<std::endl;
#endif
		session=mp_strdup(buf);
	    }
	    delete buf;
	}
	*answer_ptr=answer;
	answer_ptr++;
    } while ((strlen(answer)!=0) && (++ans_count < RTSP_MAX_FIELDS));
    cseq++;
    *answer_ptr=NULL;
    schedule_standard();
    return code;
}

/*
 * send an ok message
 */

int Rtsp::send_ok() const {
    char _cseq[16];

    put("RTSP/1.0 200 OK");
    sprintf(_cseq,"CSeq: %u", cseq);
    put(_cseq);
    put("");
    return 0;
}

/*
 * implementation of must-have rtsp requests; functions return
 * server status code.
 */

int Rtsp::request_options(const char *what) {
    char *buf;

    if (what) buf=mp_strdup(what);
    else {
	buf=new char [strlen(host)+16];
	sprintf(buf,"rtsp://%s:%i", host, port);
    }
    send_request(RTSP_METHOD_OPTIONS,buf);
    delete buf;
    return get_answers();
}

int Rtsp::request_describe(const char *what) {
    char *buf;

    if (what) buf=mp_strdup(what);
    else {
	buf=new char [strlen(host)+strlen(path)+16];
	sprintf(buf,"rtsp://%s:%i/%s", host, port, path);
    }
    send_request(RTSP_METHOD_DESCRIBE,buf);
    delete buf;
    return get_answers();
}

int Rtsp::request_setup(const char *what, char *control) {
    char *buf = NULL;

    if (what) buf = mp_strdup (what);
    else {
	int len = strlen (host) + strlen (path) + 16;
	if (control) len += strlen (control) + 1;
	buf = new char [len];
	sprintf (buf, "rtsp://%s:%i/%s%s%s", host, port, path,
	     control ? "/" : "", control ? control : "");
    }
    send_request (RTSP_METHOD_SETUP, buf);
    delete buf;
    return get_answers ();
}

int Rtsp::request_setparameter(const char *what) {
    char *buf;

    if (what) buf=mp_strdup(what);
    else {
	buf=new char [strlen(host)+strlen(path)+16];
	sprintf(buf,"rtsp://%s:%i/%s", host, port, path);
    }
    send_request(RTSP_METHOD_SET_PARAMETER,buf);
    delete buf;
    return get_answers();
}

int Rtsp::request_play(const char *what) {
    char *buf;
    int ret;

    if (what) buf=mp_strdup(what);
    else {
	buf=new char [strlen(host)+strlen(path)+16];
	sprintf(buf,"rtsp://%s:%i/%s", host, port, path);
    }
    send_request(RTSP_METHOD_PLAY,buf);
    delete buf;

    ret = get_answers ();
    if (ret == RTSP_STATUS_OK) server_state = RTSP_PLAYING;
    return ret;
}

int Rtsp::request_teardown(const char *what) {
    char *buf;

    if (what) buf = mp_strdup (what);
    else {
	buf = new char [strlen (host) + strlen (path) + 16];
	sprintf (buf, "rtsp://%s:%i/%s", host, port, path);
    }
    send_request (RTSP_METHOD_TEARDOWN, buf);
    delete buf;

    /* after teardown we're done with RTSP streaming, no need to get answer as
	reading more will only result to garbage and buffer overflow */
    return RTSP_STATUS_OK;
}

/*
 * read opaque data from stream
 */

int Rtsp::read_data(char *buffer, unsigned int size) const {
    int i,seq;

    if (size>=4) {
	i=read_stream(buffer, 4);
	if (i<4) return i;
	if (((buffer[0]=='S')&&(buffer[1]=='E')&&(buffer[2]=='T')&&(buffer[3]=='_')) ||
	    ((buffer[0]=='O')&&(buffer[1]=='P')&&(buffer[2]=='T')&&(buffer[3]=='I'))) { // OPTIONS
	    char *rest=get();
	    if (!rest) return -1;
	    seq=-1;
	    do {
		delete rest;
		rest=get();
		if (!rest) return -1;
		if (!strncasecmp(rest,"CSeq:",5))
		sscanf(rest,"%*s %u",&seq);
	    } while (strlen(rest)!=0);
	    delete rest;
	    if (seq<0) {
#ifdef LOG
		mpxp_warn<<"rtsp: warning: CSeq not recognized!"<<std::endl;
#endif
		seq=1;
	    }
	    /* let's make the server happy */
	    put("RTSP/1.0 451 Parameter Not Understood");
	    rest=new char [17];
	    sprintf(rest,"CSeq: %u", seq);
	    put(rest);
	    delete rest;
	    put("");
	    i=read_stream(buffer, size);
	} else {
	    i=read_stream(buffer+4, size-4);
	    i+=4;
	}
    } else i=read_stream(buffer, size);
#ifdef LOG
    mpxp_info<<"librtsp: << "<<i<<" of "<<size<<" bytes"<<std::endl;
#endif
    return i;
}

/*
 * connect to a rtsp server
 */

//rtsp_t *rtsp_connect(const char *mrl, const char *user_agent) {
Rtsp* Rtsp::connect(Tcp& tcp, char* mrl,const char *path,const  char *host, int port,const char *user_agent) {

  Rtsp& s=*new(zeromem) Rtsp(tcp);
  int i;

  for (i=0; i<RTSP_MAX_FIELDS; i++) {
    s.answers[i]=NULL;
    s.scheduled[i]=NULL;
  }

  s.server=NULL;
  s.server_state=0;
  s.server_caps=0;

  s.cseq=0;
  s.session=NULL;

  if (user_agent)
    s.user_agent=mp_strdup(user_agent);
  else
    s.user_agent=mp_strdup("User-Agent: RealMedia Player Version 6.0.9.1235 (linux-2.0-libc6-i386-gcc2.95)");

  s.mrl = mp_strdup(mrl);
  s.host = mp_strdup(host);
  s.port = port;
  s.path = mp_strdup(path);
  while (*path == '/')
    path++;
  if ((s.param = strchr(s.path, '?')) != NULL)
    s.param++;

  if (!tcp.established()) {
    mpxp_err<<"rtsp: failed to connect to "<<s.host<<std::endl;
    s.close();
    delete &s;
    return NULL;
  }

  s.server_state=RTSP_CONNECTED;

  /* now let's send an options request. */
  s.schedule_field("CSeq: 1");
  s.schedule_field(s.user_agent);
  s.schedule_field("ClientChallenge: 9e26d33f2984236010ef6253fb1887f7");
  s.schedule_field("PlayerStarttime: [28/03/2003:22:50:23 00:00]");
  s.schedule_field("CompanyID: KnKV4M4I/B2FjJ1TToLycw==");
  s.schedule_field("GUID: 00000000-0000-0000-0000-000000000000");
  s.schedule_field("RegionData: 0");
  s.schedule_field("ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
  /*rtsp_schedule_field(s, "Pragma: initiate-session");*/
  s.request_options(NULL);

  return &s;
}


/*
 * closes an rtsp connection
 */

void Rtsp::close() {
    if (server_state) {
	if (server_state == RTSP_PLAYING)
	    request_teardown (NULL);
	tcp.close();
    }
}

/*
 * search in answers for tags. returns a pointer to the content
 * after the first matched tag. returns NULL if no match found.
 */

char* Rtsp::search_answers(const char *tag) const {
    char *const *answer;
    char *ptr;

    if (!answers) return NULL;
    answer=answers;

    while (*answer) {
	if (!strncasecmp(*answer,tag,strlen(tag))) {
	    ptr=strchr(*answer,':');
	    if (!ptr) return NULL;
	    ptr++;
	    while(*ptr==' ') ptr++;
	    return ptr;
	}
	answer++;
    }
    return NULL;
}

/*
 * session id management
 */

void Rtsp::set_session(const char *id) {
    if (session) delete session;
    session=mp_strdup(id);
}

const char* Rtsp::get_session() const { return session; }
char* Rtsp::get_mrl() const { return mrl; }

char* Rtsp::get_param(const char *p) const {
    int len;
    const char *_param;
    if (!param) return NULL;
    if (!p) return mp_strdup(param);
    len = strlen(p);
    _param = param;
    while (_param && *_param) {
	const char *nparam = strchr(_param, '&');
	if (strncmp(_param, p, len) == 0 && _param[len] == '=') {
	    _param += len + 1;
	    len = nparam ? nparam - _param : strlen(_param);
	    char* _nparam = new char [len + 1];
	    memcpy(_nparam, _param, len);
	    _nparam[len] = 0;
	    return _nparam;
	}
	_param = nparam ? nparam + 1 : NULL;
    }
    return NULL;
}

/*
 * schedules a field for transmission
 */

void Rtsp::schedule_field(const char *string) {
    int i=0;
    if (!string) return;
    while(scheduled[i]) i++;
    scheduled[i]=mp_strdup(string);
}

/*
 * removes the first scheduled field which prefix matches string.
 */

void Rtsp::unschedule_field(const char *string) {
    char **ptr=scheduled;
    if (!string) return;
    while(*ptr) {
	if (!strncmp(*ptr, string, strlen(string))) break;
	else ptr++;
    }
    if (*ptr) delete *ptr;
    ptr++;
    do {
	*(ptr-1)=*ptr;
    } while(*ptr);
}

/*
 * unschedule all fields
 */

void Rtsp::unschedule_all() {
    char **ptr;

    if (!scheduled) return;
    ptr=scheduled;

    while (*ptr) {
	delete *ptr;
	*ptr=NULL;
	ptr++;
    }
}
/*
 * mp_free answers
 */

void Rtsp::free_answers() {
    char **answer;

    if (!answers) return;
    answer=answers;
    while (*answer) {
	delete *answer;
	*answer=NULL;
	answer++;
    }
}
} // namespace	usr
