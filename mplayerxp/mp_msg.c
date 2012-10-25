#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include "mp_config.h"
#include "nls/nls.h"
#include "mp_msg.h"

#define _bg(x) ((x) >> 4)
#define _fg(x) ((x) & 0x0f)
static int _color[8] = {0,4,2,6,1,5,3,7};
char vtmp[100];
uint32_t mp_msg_filter=0xFFFFFFFF;
static char scol[9][20];
const char hl[9] = { 0xC, 0x4, 0xE, 0xA, 0xB, 0x7, 0x9, 0x3, 0x7 };
pthread_mutex_t mp_msg_mutex=PTHREAD_MUTEX_INITIALIZER;

static char *_2ansi(unsigned char attr)
{
    int bg = _bg(attr);
    int bc = _color[bg & 7];

    sprintf(vtmp,
	"\033[%d;3%d;4%d%sm",
	_fg(attr) > 7,
	_color[_fg(attr) & 7],
	bc,
	bg > 7 ? ";5" : ""
    );
    return vtmp;
}

void mp_msg_init(int verbose)
{
    unsigned i;
    for(i=0;i<sizeof(hl)/sizeof(char);i++) memcpy(scol[i],_2ansi(hl[i]),sizeof(scol[0]));
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

void mp_msg_c( unsigned x, const char *srcfile,unsigned linenum,const char *format, ... ){
/* TODO: more useful usage of module_id */
    va_list va;
    char sbuf[0xFFFFF];
    unsigned ssize;
    unsigned level=(x>>28)&0xF;
    unsigned mod=x&0x0FFFFFFF;
    static int was_eol=1;
    if(level>mp_conf.verbose+MSGL_V-1) return; /* do not display */
    if((mod&mp_msg_filter)==0) return; /* do not display */
    pthread_mutex_lock(&mp_msg_mutex);
    if(isatty(fileno(stderr)))
	fprintf(stderr,scol[level<9?level:8]);
    if(mp_conf.verbose>1 && was_eol)
    {
	unsigned mod_name;
	const char *smod=NULL;
	mod_name = 0;
	while((mod&0x1)==0) { mod_name++; mod>>=1; }
	if(mod_name < sizeof(msg_prefix)/sizeof(msg_prefix[0]))
		smod = msg_prefix[mod_name];
	fprintf(stderr,"%s.%s(%u): ",smod?smod:"UNKNOWN",srcfile,linenum);
    }
    va_start(va, format);
    ssize=vsprintf(sbuf,format, va);
    va_end(va);
    if(strcmp(nls_get_screen_cp(),"UTF-8")!=0) {
	char *obuf;
	obuf=nls_recode2screen_cp("UTF-8",sbuf,ssize);
	fputs(obuf,stderr);
	free(obuf);
    }
    else fputs(sbuf,stderr);
    if(format[strlen(format)-1]=='\n') was_eol=1;
    else was_eol=0;
    fflush(stderr);
    pthread_mutex_unlock(&mp_msg_mutex);
}

void mp_msg_flush(void) { fflush(stderr); }

void mp_msg_uninit(void)
{
    if(isatty(fileno(stderr))) fprintf(stderr,scol[8]);
    mp_msg_flush();
}
