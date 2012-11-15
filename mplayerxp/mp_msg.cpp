#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
extern "C" {
#include "mp_config.h"
#include "nls/nls.h"
#include "mp_msg.h"
#include "osdep/mplib.h"
}
#define _bg(x) ((x) >> 4)
#define _fg(x) ((x) & 0x0f)
typedef struct priv_s {
    int		_color[8];
    char	vtmp[100];
    char	scol[9][20];
    pthread_mutex_t mp_msg_mutex;
}priv_t;
const char hl[9] = { 0xC, 0x4, 0xE, 0xA, 0xB, 0x7, 0x9, 0x3, 0x7 };

static char *_2ansi(unsigned char attr)
{
    priv_t*priv=reinterpret_cast<priv_t*>(mp_data->msg_priv);
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

void mp_msg_init(int verbose)
{
    pthread_mutexattr_t attr;
    unsigned i;
    int _color[8]={0,4,2,6,1,5,3,7};
    mp_data->msg_priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=reinterpret_cast<priv_t*>(mp_data->msg_priv);
    memcpy(priv->_color,_color,sizeof(_color));
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&priv->mp_msg_mutex,&attr);
    pthread_mutexattr_destroy(&attr);
    for(i=0;i<sizeof(hl)/sizeof(char);i++)
	memcpy(priv->scol[i],_2ansi(hl[i]),sizeof(priv->scol[0]));
}

void mp_msg_uninit(void)
{
    priv_t*priv=reinterpret_cast<priv_t*>(mp_data->msg_priv);
    if(isatty(fileno(stderr))) fprintf(stderr,priv->scol[8]);
    mp_msg_flush();
    pthread_mutex_destroy(&priv->mp_msg_mutex);
    mp_free(priv);
}

const char * msg_prefix[] =
{
    "GLOBAL",
    "PLAYER",
    "LIBVO",
    "LIBAO",
    "DEMUX",
    "CFGPRS",
    "DECAUD",
    "DECVID",
    "VOBSUB",
    "OSDEP",
    "SPUDEC",
    "PLAYTR",
    "INPUT",
    "OSD",
    "CPUDTC",
    "CODCFG",
    "SWS",
    "FINDSB",
    "SUBRDR",
    "POSTPR"
};

int mp_msg_c( unsigned x, const char *format, ... ){
/* TODO: more useful usage of module_id */
    int rc=0;
    priv_t*priv=NULL;
    va_list va;
    char sbuf[0xFFFFF];
    unsigned ssize;
    unsigned level=(x>>28)&0xF;
    unsigned mod=x&0x0FFFFFFF;
    static int was_eol=1;
    if(mp_data) priv=reinterpret_cast<priv_t*>(mp_data->msg_priv);
    if(level>mp_conf.verbose+MSGL_V-1) return 0; /* do not display */
    if((mod&mp_conf.msg_filter)==0) return 0; /* do not display */
    if(priv) {
	pthread_mutex_lock(&priv->mp_msg_mutex);
	if(isatty(fileno(stderr)))
	    fprintf(stderr,priv->scol[level<9?level:8]);
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
    va_start(va, format);
    ssize=vsprintf(sbuf,format, va);
    va_end(va);
    if(strcmp(nls_get_screen_cp(),"UTF-8")!=0) {
	char *obuf;
	obuf=nls_recode2screen_cp("UTF-8",sbuf,ssize);
	rc=fputs(obuf,stderr);
	mp_free(obuf);
    }
    else rc=fputs(sbuf,stderr);
    if(format[strlen(format)-1]=='\n') was_eol=1;
    else was_eol=0;
    fflush(stderr);
    if(priv) pthread_mutex_unlock(&priv->mp_msg_mutex);
    return rc;
}

void mp_msg_flush(void) { fflush(stderr); }
