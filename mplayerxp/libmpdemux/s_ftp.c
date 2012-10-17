#include "../mp_config.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#define closesocket close
#else
#include <winsock2.h>
#endif

#include "stream.h"
#include "help_mp.h"
#include "tcp.h"
#include "url.h"
#include "demux_msg.h"


struct stream_priv_s {
  char* user;
  char* pass;
  char* host;
  int port;
  char* filename;
  URL_t *url;

  char *cput,*cget;
  int handle;
  int cavail,cleft;
  char *buf;
  off_t spos;
};

#define BUFSIZE 8192

#define TELNET_IAC      255             /* interpret as command: */
#define TELNET_IP       244             /* interrupt process--permanently */
#define TELNET_SYNCH    242             /* for telfunc calls */

// Check if there is something to read on a fd. This avoid hanging
// forever if the network stop responding.
static int __FASTCALL__ fd_can_read(int fd,int timeout) {
  fd_set fds;
  struct timeval tv;

  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  
  return (select(fd+1, &fds, NULL, NULL, &tv) > 0);
}

/*
 * read a line of text
 *
 * return -1 on error or bytecount
 */
static int __FASTCALL__ readline(char *buf,int max,struct stream_priv_s *ctl)
{
    int x,retval = 0;
    char *end,*bp=buf;
    int eof = 0;
 
    do {
      if (ctl->cavail > 0) {
	x = (max >= ctl->cavail) ? ctl->cavail : max-1;
	end = memccpy(bp,ctl->cget,'\n',x);
	if (end != NULL)
	  x = end - bp;
	retval += x;
	bp += x;
	*bp = '\0';
	max -= x;
	ctl->cget += x;
	ctl->cavail -= x;
	if (end != NULL) {
	  bp -= 2;
	  if (strcmp(bp,"\r\n") == 0) {
	    *bp++ = '\n';
	    *bp++ = '\0';
	    --retval;
	  }
	  break;
	}
      }
      if (max == 1) {
	*buf = '\0';
	break;
      }
      if (ctl->cput == ctl->cget) {
	ctl->cput = ctl->cget = ctl->buf;
	ctl->cavail = 0;
	ctl->cleft = BUFSIZE;
      }
      if(eof) {
	if (retval == 0)
	  retval = -1;
	break;
      }

      if(!fd_can_read(ctl->handle, 15)) {
        MSG_ERR("[ftp] read timed out\n");
        retval = -1;
        break;
      }

      if ((x = recv(ctl->handle,ctl->cput,ctl->cleft,0)) == -1) {
	MSG_ERR("[ftp] read error: %s\n",strerror(errno));
	retval = -1;
	break;
      }
      if (x == 0)
	eof = 1;
      ctl->cleft -= x;
      ctl->cavail += x;
      ctl->cput += x;
    } while (1);
    
    return retval;
}

/*
 * read a response from the server
 *
 * return 0 if first char doesn't match
 * return 1 if first char matches
 */
static int __FASTCALL__ readresp(struct stream_priv_s* ctl,char* rsp)
{
    static char response[256];
    char match[5];
    int r;

    if (readline(response,256,ctl) == -1)
      return 0;
 
    r = atoi(response)/100;
    if(rsp) strcpy(rsp,response);

    MSG_V("[ftp] < %s",response);

    if (response[3] == '-') {
      strncpy(match,response,3);
      match[3] = ' ';
      match[4] = '\0';
      do {
	if (readline(response,256,ctl) == -1) {
	  MSG_ERR("[ftp] Control socket read failed\n");
	  return 0;
	}
	MSG_V("[ftp] < %s",response);
      }	while (strncmp(response,match,4));
    }
    return r;
}


static int __FASTCALL__ FtpSendCmd(const char *cmd, struct stream_priv_s *nControl,char* rsp)
{
  int l = strlen(cmd);
  int hascrlf = cmd[l - 2] == '\r' && cmd[l - 1] == '\n';

  if(hascrlf && l == 2) MSG_V("\n");
  else MSG_V("[ftp] > %s",cmd);
  while(l > 0) {
    int s = send(nControl->handle,cmd,l,0);

    if(s <= 0) {
      MSG_ERR("[ftp] write error: %s\n",strerror(errno));
      return 0;
    }
    
    cmd += s;
    l -= s;
  }
    
  if (hascrlf)  
    return readresp(nControl,rsp);
  else
    return FtpSendCmd("\r\n", nControl, rsp);
}

static int __FASTCALL__ FtpOpenPort(struct stream_priv_s* p) {
  int resp,fd;
  char rsp_txt[256];
  char* par,str[128];
  int num[6];

  resp = FtpSendCmd("PASV",p,rsp_txt);
  if(resp != 2) {
    MSG_WARN("[ftp] command 'PASV' failed: %s\n",rsp_txt);
    return 0;
  }
  
  par = strchr(rsp_txt,'(');
  
  if(!par || !par[0] || !par[1]) {
    MSG_ERR("[ftp] invalid server response: %s ??\n",rsp_txt);
    return 0;
  }

  sscanf(par+1,"%u,%u,%u,%u,%u,%u",&num[0],&num[1],&num[2],
	 &num[3],&num[4],&num[5]);
  snprintf(str,127,"%d.%d.%d.%d",num[0],num[1],num[2],num[3]);
  fd = tcp_connect2Server(str,(num[4]<<8)+num[5],0);

  if(fd < 0)
    MSG_ERR("[ftp] failed to create data connection\n");

  return fd;
}

static int __FASTCALL__ FtpOpenData(stream_t* s,size_t newpos) {
  struct stream_priv_s* p = s->priv;
  int resp;
  char str[256],rsp_txt[256];

  // Open a new connection
  s->fd = FtpOpenPort(p);

  if(s->fd < 0) return 0;

  if(newpos > 0) {
    snprintf(str,255,"REST %"PRId64, (int64_t)newpos);

    resp = FtpSendCmd(str,p,rsp_txt);
    if(resp != 3) {
      MSG_WARN("[ftp] command '%s' failed: %s\n",str,rsp_txt);
      newpos = 0;
    }
  }

  // Get the file
  snprintf(str,255,"RETR %s",p->filename);
  resp = FtpSendCmd(str,p,rsp_txt);

  if(resp != 1) {
    MSG_ERR("[ftp] command '%s' failed: %s\n",str,rsp_txt);
    return 0;
  }

  p->spos = s->pos = newpos;
  return 1;
}

static int __FASTCALL__ ftp_read(stream_t *s,stream_packet_t*sp){
  int r;

  if(s->fd < 0 && !FtpOpenData(s,s->pos))
    return -1;
  
  if(!fd_can_read(s->fd, 15)) {
    MSG_ERR("[ftp] read timed out\n");
    return -1;
  }
  MSG_V("ftp read: %u bytes\n",sp->len);
  r = recv(s->fd,sp->buf,sp->len,0);
  ((struct stream_priv_s *)s->priv)->spos+=r;
  return (r <= 0) ? -1 : r;
}

static off_t __FASTCALL__ ftp_seek(stream_t *s,off_t newpos) {
  struct stream_priv_s* p = s->priv;
  int resp;
  char rsp_txt[256];

  if(p->spos==newpos) return p->spos;
  MSG_V("ftp seek: %llu bytes\n",newpos);
  if(s->pos > s->end_pos) {
    s->_Errno=errno;
    return 0;
  }


  // Check to see if the server did not already terminate the transfer
  if(fd_can_read(p->handle, 0)) {
    if(readresp(p,rsp_txt) != 2)
      MSG_WARN("[ftp] Warning the server didn't finished the transfer correctly: %s\n",rsp_txt);
    closesocket(s->fd);
    s->fd = -1;
  }

  // Close current download
  if(s->fd >= 0) {
    static const char pre_cmd[]={TELNET_IAC,TELNET_IP,TELNET_IAC,TELNET_SYNCH};
    //int fl;
    

    // First close the fd
    closesocket(s->fd);
    s->fd = 0;
    
    // Send send the telnet sequence needed to make the server react
    
    // Dunno if this is really needed, lftp have it. I let
    // it here in case it turn out to be needed on some other OS
    //fl=fcntl(p->handle,F_GETFL);
    //fcntl(p->handle,F_SETFL,fl&~O_NONBLOCK);

    // send only first byte as OOB due to OOB braindamage in many unices
    send(p->handle,pre_cmd,1,MSG_OOB);
    send(p->handle,pre_cmd+1,sizeof(pre_cmd)-1,0);
    
    //fcntl(p->handle,F_SETFL,fl);

    // Get the 426 Transfer aborted
    // Or the 226 Transfer complete
    resp = readresp(p,rsp_txt);
    if(resp != 4 && resp != 2) {
      MSG_ERR("[ftp] Server didn't abort correctly: %s\n",rsp_txt);
      s->eof = 1;
      return 0;
    }
    // Send the ABOR command
    // Ignore the return code as sometimes it fail with "nothing to abort"
    FtpSendCmd("ABOR",p,rsp_txt);
  }
  if(FtpOpenData(s,newpos)) p->spos=newpos;
  return p->spos;
}

static off_t __FASTCALL__ ftp_tell(stream_t*stream)
{
    struct stream_priv_s*p=stream->priv;
    return p->spos;
}


static void __FASTCALL__ ftp_close(stream_t *s) {
  struct stream_priv_s* p = s->priv;

  if(!p) return;

  if(s->fd > 0) {
    closesocket(s->fd);
    s->fd = 0;
  }

  FtpSendCmd("QUIT",p,NULL);

  if(p->handle) closesocket(p->handle);
  if(p->buf) free(p->buf);

  free(p);
}

static int __FASTCALL__ ftp_open(stream_t *stream,const char *filename,unsigned flags)
{
  int len = 0,resp;
  struct stream_priv_s* p;
  URL_t* url;
  char str[256],rsp_txt[256];
  char *uname;

  UNUSED(flags);
  if(!(uname=malloc(strlen(filename)+7))) return 0;
  strcpy(uname,"ftp://");
  strcat(uname,filename);
  if(!(url=url_new(uname))) goto bad_url;
  free(uname);
//  url = check4proxies (rurl);
  if(!(url->hostname && url->file)) {
    bad_url:
    MSG_ERR("[ftp] Bad url\n");
    return 0;
  }
  p=stream->priv=malloc(sizeof(struct stream_priv_s));
  memset(p,0,sizeof(struct stream_priv_s));
  p->user=url->username?url->username:"anonymous";
  p->pass=url->password?url->password:"no@spam";
  p->host=url->hostname;
  p->port=url->port?url->port:21;
  p->filename=url->file;
  MSG_V("FTP: Opening ~%s :%s @%s :%i %s\n",p->user,p->pass,p->host,p->port,p->filename);

  // Open the control connection
  p->handle = tcp_connect2Server(p->host,p->port,1);

  if(p->handle < 0) {
    url_free(url);
    free(stream->priv);
    return 0;
  }

  // We got a connection, let's start serious things
  stream->fd = -1;
  p->buf = malloc(BUFSIZE);

  if (readresp(p, NULL) == 0) {
    ftp_close(stream);
    url_free(url);
    return 0;
  }
  // Login
  snprintf(str,255,"USER %s",p->user);
  resp = FtpSendCmd(str,p,rsp_txt);

  // password needed
  if(resp == 3) {
    snprintf(str,255,"PASS %s",p->pass);
    resp = FtpSendCmd(str,p,rsp_txt);
    if(resp != 2) {
      MSG_ERR("[ftp] command '%s' failed: %s\n",str,rsp_txt);
      ftp_close(stream);
      url_free(url);
      return 0;
    }
  } else if(resp != 2) {
    MSG_ERR("[ftp] command '%s' failed: %s\n",str,rsp_txt);
    ftp_close(stream);
    url_free(url);
    return 0;
  }


  // Set the transfer type
  resp = FtpSendCmd("TYPE I",p,rsp_txt);
  if(resp != 2) {
    MSG_ERR("[ftp] command 'TYPE I' failed: %s\n",rsp_txt);
    ftp_close(stream);
    url_free(url);
    return 0;
  }

  // Get System of FTP
  resp = FtpSendCmd("SYST",p,rsp_txt);
  if(resp != 2) {
    MSG_ERR("[ftp] command 'SYST' failed: %s\n",rsp_txt);
    ftp_close(stream);
    url_free(url);
    return 0;
  }
  MSG_INFO("[ftp] System: %s\n",rsp_txt);
  resp = FtpSendCmd("STAT",p,rsp_txt);
  if(resp != 2) {
    MSG_ERR("[ftp] command 'STAT' failed: %s\n",rsp_txt);
    ftp_close(stream);
    url_free(url);
    return 0;
  }

  // Get the filesize
  snprintf(str,255,"SIZE %s",p->filename);
  resp = FtpSendCmd(str,p,rsp_txt);
  if(resp != 2) {
    MSG_WARN("[ftp] command '%s' failed: %s\n",str,rsp_txt);
  } else {
    int dummy;
    sscanf(rsp_txt,"%d %d",&dummy,&len);
  }

  stream->sector_size=BUFSIZE;
  if(len) {
    stream->type = STREAMTYPE_SEEKABLE;
    stream->end_pos = len;
  }
  else {
    stream->type = STREAMTYPE_STREAM;
  }
  p->spos=0;
  // The data connection is really opened only at the first
  // read/seek. This must be done when the cache is used
  // because the connection would stay open in the main process,
  // preventing correct abort with many servers.
  stream->fd = -1;

  url_free(url);
  return 1;
}

static int __FASTCALL__ ftp_ctrl(stream_t *s,unsigned cmd,any_t*args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return SCTRL_UNKNOWN;
}

/* "reuse a bit of code from ftplib written by Thomas Pfau", */
const stream_driver_t ftp_stream =
{
    "ftp://",
    "reads multimedia stream from File Transfer Protocol (FTP)",
    ftp_open,
    ftp_read,
    ftp_seek,
    ftp_tell,
    ftp_close,
    ftp_ctrl
};
