#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#ifdef HAVE_LIRC

#include <lirc/lirc_client.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>

#include "mpxp_help.h"
#include "input.h"
#include "in_msg.h"

namespace	usr {
static struct lirc_config *lirc_config;
char *lirc_configfile;

typedef struct priv_s {
    int		lirc_sock;
    char*	cmd_buf;
}priv_t;

any_t* mp_input_lirc_open(void) {
    priv_t* priv=new(zeromem) priv_t;

    mpxp_info<<MSGTR_SettingUpLIRC<<std::endl;
    if((priv->lirc_sock=lirc_init(const_cast<char*>("mplayer"),1))==-1){
	mpxp_err<<MSGTR_LIRCopenfailed<<std::endl<<MSGTR_LIRCdisabled<<std::endl;
	delete priv;
	return NULL;
    }

    if(lirc_readconfig( lirc_configfile,&lirc_config,NULL )!=0 ){
	mpxp_err<<MSGTR_LIRCcfgerr<<": "<<(lirc_configfile==NULL?"~/.lircrc":lirc_configfile)<<std::endl;
	mpxp_err<<MSGTR_LIRCdisabled<<std::endl;
	lirc_deinit();
	delete priv;
	return NULL;
    }
    return priv;
}

int mp_input_lirc_read_cmd(any_t* ctx,char* dest, int s) {
    priv_t& priv = *reinterpret_cast<priv_t*>(ctx);
    fd_set fds;
    struct timeval tv;
    int r,cl = 0;
    char *code = NULL,*c = NULL;

    // We have something in the buffer return it
    if(priv.cmd_buf != NULL) {
	int l = strlen(priv.cmd_buf), w = l > s ? s : l;
	memcpy(dest,priv.cmd_buf,w);
	l -= w;
	if(l > 0) memmove(priv.cmd_buf,&priv.cmd_buf[w],l+1);
	else {
	    delete priv.cmd_buf;
	    priv.cmd_buf = NULL;
	}
	return w;
    }
    // Nothing in the buffer, pool the lirc fd
    FD_ZERO(&fds);
    FD_SET(priv.lirc_sock,&fds);
    memset(&tv,0,sizeof(tv));
    while((r = select(1,&fds,NULL,NULL,&tv)) <= 0) {
	if(r < 0) {
	    if(errno == EINTR) continue;
	    mpxp_err<<"Select error:"<<strerror(errno)<<std::endl;
	    return MP_INPUT_ERROR;
	} else
	    return MP_INPUT_NOTHING;
    }
    // There's something to read
    if(lirc_nextcode(&code) != 0) {
	mpxp_err<<"Lirc error :("<<std::endl;
	return MP_INPUT_DEAD;
    }

    if(!code) return MP_INPUT_NOTHING;

    // We put all cmds in a single buffer separated by \n
    while((r = lirc_code2char(lirc_config,code,&c))==0 && c!=NULL) {
	int l = strlen(c);
	if(l <= 0) continue;
	priv.cmd_buf = (char *)mp_realloc(priv.cmd_buf,cl+l+2);
	memcpy(&priv.cmd_buf[cl],c,l);
	cl += l+1;
	priv.cmd_buf[cl-1] = '\n';
	priv.cmd_buf[cl] = '\0';
    }
    delete code;

    if(r < 0) return MP_INPUT_DEAD;
    else if(priv.cmd_buf) // return the first command in the buffer
	return mp_input_lirc_read_cmd(&priv,dest,s);
    else
	return MP_INPUT_NOTHING;
}

void mp_input_lirc_close(any_t* ctx) {
    priv_t* priv = reinterpret_cast<priv_t*>(ctx);
    if(priv->cmd_buf) {
	delete priv->cmd_buf;
	priv->cmd_buf = NULL;
    }
    lirc_freeconfig(lirc_config);
    lirc_deinit();
    delete priv;
}
}// namespace	usr
#endif
