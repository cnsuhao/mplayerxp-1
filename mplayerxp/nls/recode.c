#include "../mp_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifdef USE_ICONV
#ifdef HAVE_GICONV
#include <giconv.h>
#else
#include <iconv.h>
#endif
#endif
#include "nls_msg.h"
#include "./nls.h"

/*
 *We have to proceed with the POSIX methods of looking to `LANG'.
  On some systems this can be done by the `setlocale' function itself.
 *Since glibc-2.2, the `LANGUAGE' environment variable is ignored by
  locale program. (This is a GNU extension)
 *`LINGUAS' contains a space separated list of two-letter codes.
 set | more:
 LANG=ru_RU.KOI8-R
 LANGUAGE=ru_RU.KOI8-R
 LINGUAS=ru
 Output of locale program contains `LANG' variable only!
*/
static const char * langs[] = { "LANG", "LANGUAGE", "LINGUAS" };

char *nls_get_screen_cp(void)
{
    unsigned i;
    char *nls;
    static char to_cp[256];
    strcpy(to_cp,"UTF-8");
    for(i=0;i<sizeof(langs)/sizeof(char *);i++)
    {
	if((nls=getenv(langs[i]))!=NULL)
	{
		nls=strchr(nls,'.');
		if(nls) strcpy(to_cp,nls+1);
		break;
	}
    }
    return to_cp;
}

char *nls_recode2screen_cp(const char *src_cp,const char *param,unsigned len)
{
    char *obuff;
#ifdef USE_ICONV
    if(src_cp)
    {
	iconv_t ic;
	const char *ibuff,*ib;
	char *ob,*to_cp;
	size_t inb,outb;
	to_cp=nls_get_screen_cp();
	errno=0;
	ic=iconv_open(to_cp,src_cp);
	if(errno)
	{
	    fprintf(stderr,"ICONV(%s,%s): Open with error: %s\n",to_cp,src_cp,strerror(errno));
	    errno=0;
	    ic=iconv_open("UTF8",src_cp);
	    if(errno) fprintf(stderr,"ICONV(%s,%s)[%s]: Open with error: %s\n",to_cp,src_cp,"UTF8",strerror(errno));
	}
	if(errno) goto do_def;
	inb=len;
	outb=(len+1)*4;
	obuff=malloc(outb);
	ibuff=param;
	ob=obuff;
	ib=ibuff;
	if(iconv(ic,(char **)&ib,&inb,&ob,&outb) != (size_t)(-1))
	{
	    iconv_close(ic);
	    *ob='\0';
	}
	else
	{
	    free(obuff);
	    fprintf(stderr,"ICONV: Can't recode from %s to %s (%s)\n",src_cp,to_cp,strerror(errno));
	    iconv_close(ic);
	    goto do_def;
	}
    }
    else
    {
	do_def:
	obuff=strdup(param);
    }
#else
    obuff=strdup(param);
#endif
    return obuff;
}

static unsigned __FASTCALL__ size_of_utf8char(char ch)
{
    if((ch & 0xFF) == 0xFF) return 8;
    else if((ch & 0xFE) == 0xFE) return 7;
    else if((ch & 0xFC) == 0xFC) return 6;
    else if((ch & 0xF8) == 0xF8) return 5;
    else if((ch & 0xF0) == 0xF0) return 4;
    else if((ch & 0xE0) == 0xE0) return 3;
    else if((ch & 0xC0) == 0xC0) return 2;
    else return 1;
}

unsigned utf8_get_char(const char **str) {
  char *obuff;
  const char *strp = (const char *)*str;
  unsigned c,siz;
  siz=size_of_utf8char(*strp);
  obuff=nls_recode2screen_cp("UTF8",strp,siz);
  *str = (const char *)strp+siz;
  c=*obuff;
  free(obuff);
  return c;
}

char *nls_recode_from_screen_cp(const char *to_cp,const char *param,size_t *outb)
{
    char *obuff;
#ifdef USE_ICONV
    if(to_cp)
    {
	iconv_t ic;
	const char *ibuff,*ib;
	char *ob,*src_cp;
	size_t inb;
	src_cp=nls_get_screen_cp();
	errno=0;
	ic=iconv_open(to_cp,src_cp);
	if(errno)
	{
	    fprintf(stderr,"ICONV(%s,%s): Open with error: %s\n",to_cp,src_cp,strerror(errno));
	    errno=0;
	    ic=iconv_open(to_cp,"UTF8");
	    if(errno) fprintf(stderr,"ICONV(%s,%s)[%s]: Open with error: %s\n",to_cp,src_cp,"UTF8",strerror(errno));
	}
	if(errno) goto do_def;
	inb=strlen(param)+1;
	*outb=(strlen(param)+1)*4;
	obuff=malloc(*outb);
	ibuff=param;
	ob=obuff;
	ib=ibuff;
	if(iconv(ic,(char **)&ib,&inb,&ob,outb) != (size_t)(-1))
	{
	    iconv_close(ic);
	    *ob='\0';
	}
	else
	{
	    free(obuff);
	    fprintf(stderr,"ICONV: Can't recode from %s to %s (%s)\n",src_cp,to_cp,strerror(errno));
	    iconv_close(ic);
	    goto do_def;
	}
    }
    else
    {
	do_def:
	obuff=strdup(param);
	*outb=strlen(param);
    }
#else
    obuff=strdup(param);
    *outb=strlen(param);
#endif
    return obuff;
}
