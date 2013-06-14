#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * Subtitle reader with format autodetection
 *
 * Written by laaz
 * Some code cleanup & mp_realloc() by A'rpi/ESP-team
 * dunnowhat sub format by szabi
 */
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "subreader.h"
#include "libvo2/sub.h"

#ifdef USE_ICONV
#ifdef HAVE_GICONV
#include <giconv.h>
#else
#include <iconv.h>
#endif
#endif
#include "mpsub_msg.h"

/* Maximal length of line of a subtitle */
static const int LINE_LEN=1000;
static const subtitle* ERR=(subtitle*)-1;

static float mpsub_position=0;

int sub_uses_time=0;
int sub_errs=0;
int sub_num=0;          // number of subtitle structs
int sub_slacktime=2000; // 20 seconds

/* Use the SUB_* constant defined in the header file */
int sub_format=SUB_INVALID;

static int __FASTCALL__ eol(char p) {
    return (p=='\r' || p=='\n' || p=='\0');
}

/* Remove leading and trailing space */
static void __FASTCALL__ trail_space(char *s) {
	int i = 0;
	while (isspace(s[i])) ++i;
	if (i) strcpy(s, s + i);
	i = strlen(s) - 1;
	while (i > 0 && isspace(s[i])) s[i--] = '\0';
}

static subtitle * __FASTCALL__ sub_read_line_sami(std::ifstream& fd, subtitle *current) {
    static char line[LINE_LEN+1];
    static char *s = NULL, *slacktime_s;
    char text[LINE_LEN+1], *p=NULL, *q;
    int state;

    current->lines = current->start = current->end = 0;
    state = 0;

    /* read the first line */
    if (!s) {
	fd.getline(line,LINE_LEN);
	if (!fd.good()) return 0;
    }
    do {
	switch (state) {

	case 0: /* find "START=" or "Slacktime:" */
	    slacktime_s = strstr (s, "Slacktime:");
	    if (slacktime_s) sub_slacktime = strtol (slacktime_s + 10, NULL, 0) / 10;

	    s = strstr (s, "Start=");
	    if (s) {
		current->start = strtol (s + 6, &s, 0) / 10;
		state = 1; continue;
	    }
	    break;

	case 1: /* find "<P" */
	    if ((s = strstr (s, "<P"))) { s += 2; state = 2; continue; }
	    break;

	case 2: /* find ">" */
	    if ((s = strchr (s, '>'))) { s++; state = 3; p = text; continue; }
	    break;

	case 3: /* get all text until '<' appears */
	    if (*s == '\0') break;
	    else if (!strncasecmp (s, "<br>", 4)) {
		*p = '\0'; p = text; trail_space (text);
		if (text[0] != '\0')
		    current->text[current->lines++] = mp_strdup (text);
		s += 4;
	    }
	    else if (*s == '<') { state = 4; }
	    else if (!strncasecmp (s, "&nbsp;", 6)) { *p++ = ' '; s += 6; }
	    else if (*s == '\t') { *p++ = ' '; s++; }
	    else if (*s == '\r' || *s == '\n') { s++; }
	    else *p++ = *s++;

	    /* skip duplicated space */
	    if (p > text + 2) if (*(p-1) == ' ' && *(p-2) == ' ') p--;

	    continue;

	case 4: /* get current->end or skip <TAG> */
	    q = strstr (s, "Start=");
	    if (q) {
		current->end = strtol (q + 6, &q, 0) / 10 - 1;
		*p = '\0'; trail_space (text);
		if (text[0] != '\0')
		    current->text[current->lines++] = mp_strdup (text);
		if (current->lines > 0) { state = 99; break; }
		state = 0; continue;
	    }
	    s = strchr (s, '>');
	    if (s) { s++; state = 3; continue; }
	    break;
	}

	/* read next line */
	fd.getline(line,LINE_LEN);
	if (state != 99 && !fd.good()) {
	    if (current->start > 0) {
		break; // if it is the last subtitle
	    } else {
		return 0;
	    }
	}

    } while (state != 99);

    // For the last subtitle
    if (current->end <= 0) {
	current->end = current->start + sub_slacktime;
	*p = '\0'; trail_space (text);
	if (text[0] != '\0')
	    current->text[current->lines++] = mp_strdup (text);
    }

    return current;
}


static const char * __FASTCALL__ sub_readtext(const char *source, char **dest) {
    int len=0;
    const char *p=source;

    while ( !eol(*p) && *p!= '|' ) {
	p++,len++;
    }

    *dest= new char [len+1];
    if (!dest) {return (const char*)ERR;}

    strncpy(*dest, source, len);
    (*dest)[len]=0;

    while (*p=='\r' || *p=='\n' || *p=='|') p++;

    if (*p) return p;  // not-last text field
    else return NULL;  // last text field
}

static subtitle * __FASTCALL__ sub_read_line_microdvd(std::ifstream& fd,subtitle *current) {
    char line[LINE_LEN+1];
    char line2[LINE_LEN+1];
    char *p;
    const char *next;
    int i;

    do {
	fd.getline(line,LINE_LEN);
	if (!fd.good()) return NULL;
    } while ((sscanf (line,
		      "{%ld}{}%[^\r\n]",
		      &(current->start), line2) < 2) &&
	     (sscanf (line,
		      "{%ld}{%ld}%[^\r\n]",
		      &(current->start), &(current->end), line2) < 3));

    p=line2;

    next=p, i=0;
    while ((next =sub_readtext (next, &(current->text[i])))) {
	if (current->text[i]==(char*)ERR) {return (subtitle*)ERR;}
	i++;
	if (i>=SUB_MAX_TEXT) { mpxp_err<<"Too many lines in a subtitle"<<std::endl; current->lines=i;return current;}
    }
    current->lines= ++i;

    return current;
}

static subtitle * __FASTCALL__ sub_read_line_subrip(std::ifstream& fd, subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL, *q=NULL;
    int len;

    while (1) {
	fd.getline(line, LINE_LEN);
	if (!fd.good()) return NULL;
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) continue;
	current->start = a1*360000+a2*6000+a3*100+a4;
	current->end   = b1*360000+b2*6000+b3*100+b4;

	fd.getline(line, LINE_LEN);
	if (!fd.good()) return NULL;

	p=q=line;
	for (current->lines=1; current->lines < SUB_MAX_TEXT; current->lines++) {
	    for (q=p,len=0; *p && *p!='\r' && *p!='\n' && strncmp(p,"[br]",4); p++,len++);
	    current->text[current->lines-1]=new char[len+1];
	    if (!current->text[current->lines-1]) return (subtitle*)ERR;
	    strncpy (current->text[current->lines-1], q, len);
	    current->text[current->lines-1][len]='\0';
	    if (!*p || *p=='\r' || *p=='\n') break;
	    while (*p++!=']');
	}
	break;
    }
    return current;
}

static subtitle * __FASTCALL__ sub_read_line_subviewer(std::ifstream& fd,subtitle *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL;
    int i,len;

    while (!current->text[0]) {
	fd.getline(line, LINE_LEN);
	if (!fd.good()) return NULL;
	if ((len=sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	for (i=0; i<SUB_MAX_TEXT;) {
	    fd.getline(line, LINE_LEN);
	    if (!fd.good()) break;
	    len=0;
	    for (p=line; *p!='\n' && *p!='\r' && *p; p++,len++);
	    if (len) {
		current->text[i]=new char [len+1];
		if (!current->text[i]) return (subtitle*)ERR;
		strncpy (current->text[i], line, len); current->text[i][len]='\0';
		i++;
	    } else {
		break;
	    }
	}
	current->lines=i;
    }
    return current;
}

static subtitle * __FASTCALL__ sub_read_line_vplayer(std::ifstream& fd,subtitle *current) {
	char line[LINE_LEN+1];
	int a1,a2,a3;
	char *p=NULL, separator;
	const char *next;
	int i,len,plen;

	while (!current->text[0]) {
		fd.getline(line, LINE_LEN);
		if (!fd.good()) return NULL;
		if ((len=sscanf (line, "%d:%d:%d%c%n",&a1,&a2,&a3,&separator,&plen)) < 4)
			continue;

		if (!(current->start = a1*360000+a2*6000+a3*100))
			continue;
		// by wodzu: hey! this time we know what length it has! what is
		// that magic for? it can't deal with space instead of third
		// colon! look, what simple it can be:
		p = &line[ plen ];

		i=0;
		if (*p!='|') {
			//
			next = p,i=0;
			while ((next =sub_readtext (next, &(current->text[i])))) {
				if (current->text[i]==(char*)ERR) {return (subtitle*)ERR;}
				i++;
				if (i>=SUB_MAX_TEXT) { mpxp_err<<"Too many lines in a subtitle"<<std::endl; current->lines=i;return current;}
			}
			current->lines=i+1;
		}
	}
	return current;
}

static subtitle * __FASTCALL__ sub_read_line_rt(std::ifstream& fd,subtitle *current) {
	//TODO: This format uses quite rich (sub/super)set of xhtml
	// I couldn't check it since DTD is not included.
	// WARNING: full XML parses can be required for proper parsing
    char line[LINE_LEN+1];
    int a1,a2,a3,a4,b1,b2,b3,b4;
    char *p=NULL;
    const char *next=NULL;
    int i,len,plen;

    while (!current->text[0]) {
	fd.getline(line, LINE_LEN);
	if (!fd.good()) return NULL;
	//TODO: it seems that format of time is not easily determined, it may be 1:12, 1:12.0 or 0:1:12.0
	//to describe the same moment in time. Maybe there are even more formats in use.
	//if ((len=sscanf (line, "<Time Begin=\"%d:%d:%d.%d\" End=\"%d:%d:%d.%d\"",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
	plen=a1=a2=a3=a4=b1=b2=b3=b4=0;
	if (
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&plen)) < 4) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&b4,&plen)) < 5) &&
//	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&plen)) < 5) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&b4,&plen)) < 6) &&
	((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d:%d.%d\" %*[Ee]nd=\"%d:%d:%d.%d\"%*[^<]<clear/>%n",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4,&plen)) < 8)
	)
	    continue;
	current->start = a1*360000+a2*6000+a3*100+a4/10;
	current->end   = b1*360000+b2*6000+b3*100+b4/10;
	p=line;	p+=plen;i=0;
	// TODO: I don't know what kind of convention is here for marking multiline subs, maybe <br/> like in xml?
	next = strstr(line,"<clear/>")+8;i=0;
	while ((next =sub_readtext (next, &(current->text[i])))) {
		if (current->text[i]==(char*)ERR) {return (subtitle*)ERR;}
		i++;
		if (i>=SUB_MAX_TEXT) { mpxp_err<<"Too many lines in a subtitle"<<std::endl; current->lines=i;return current;}
	}
			current->lines=i+1;
    }
    return current;
}

static subtitle * __FASTCALL__ sub_read_line_ssa(std::ifstream& fd,subtitle *current) {
	int hour1, min1, sec1, hunsec1,
	    hour2, min2, sec2, hunsec2, nothing;
	int num;

	char line[LINE_LEN+1],
	     line3[LINE_LEN+1],
	     *line2;
	char *tmp;

	do {
	    fd.getline(line, LINE_LEN);
	    if (!fd.good()) return NULL;
	} while (sscanf (line, "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
			"%[^\n\r]", &nothing,
			&hour1, &min1, &sec1, &hunsec1,
			&hour2, &min2, &sec2, &hunsec2,
			line3) < 9);
	line2=strstr(line3,",,");
	if (!line2) return NULL;
	line2 ++;
	line2 ++;

	current->lines=1;num=0;
	current->start = 360000*hour1 + 6000*min1 + 100*sec1 + hunsec1;
	current->end   = 360000*hour2 + 6000*min2 + 100*sec2 + hunsec2;

	while ((tmp=strstr(line2, "\\n")) != NULL) {
		current->text[num]=new char[tmp-line2+1];
		strncpy (current->text[num], line2, tmp-line2);
		current->text[num][tmp-line2]='\0';
		line2=tmp+2;
		num++;
		current->lines++;
		if (current->lines >=  SUB_MAX_TEXT) return current;
	}


	current->text[num]=mp_strdup(line2);

	return current;
}

static subtitle * __FASTCALL__ sub_read_line_dunnowhat(std::ifstream& fd,subtitle *current) {
    char line[LINE_LEN+1];
    char text[LINE_LEN+1];

    fd.getline(line, LINE_LEN);
    if (!fd.good()) return NULL;
    if (sscanf (line, "%ld,%ld,\"%[^\"]", &(current->start),
		&(current->end), text) <3)
	return (subtitle*)ERR;
    current->text[0] = mp_strdup(text);
    current->lines = 1;

    return current;
}

static subtitle * __FASTCALL__ sub_read_line_mpsub(std::ifstream& fd, subtitle *current) {
	char line[LINE_LEN+1];
	float a,b;
	int num=0;
	char *p, *q;

	do
	{
	    fd.getline(line, LINE_LEN);
	    if (!fd.good()) return NULL;
	} while (sscanf (line, "%f %f", &a, &b) !=2);

	mpsub_position += a*(sub_uses_time ? 100.0 : 1.0);
	current->start=(int) mpsub_position;
	mpsub_position += b*(sub_uses_time ? 100.0 : 1.0);
	current->end=(int) mpsub_position;

	while (num < SUB_MAX_TEXT) {
		fd.getline(line, LINE_LEN);
		if (!fd.good()) {
			if (num == 0) return NULL;
			else return current;
		}
		p=line;
		while (isspace(*p)) p++;
		if (eol(*p) && num > 0) return current;
		if (eol(*p)) return NULL;

		for (q=p; !eol(*q); q++);
		*q='\0';
		if (strlen(p)) {
			current->text[num]=mp_strdup(p);
			current->lines = ++num;
		} else {
			if (num) return current;
			else return NULL;
		}
	}
	return NULL; // we should have returned before if it's OK
}

static subtitle *previous_aqt_sub = NULL;

static subtitle * __FASTCALL__ sub_read_line_aqt(std::ifstream& fd,subtitle *current) {
    char line[LINE_LEN+1];

    while (1) {
    // try to locate next subtitle
	fd.getline(line, LINE_LEN);
	if (!fd.good())
		return NULL;
	if (!(sscanf (line, "-->> %ld", &(current->start)) <1))
		break;
    }

    if (previous_aqt_sub != NULL)
	previous_aqt_sub->end = current->start-1;

    previous_aqt_sub = current;

    fd.getline(line, LINE_LEN);
    if (!fd.good()) return NULL;

    sub_readtext(line,&current->text[0]);
    current->lines = 1;
    current->end = current->start; // will be corrected by next subtitle

    fd.getline(line, LINE_LEN);
    if (!fd.good()) return current;

    sub_readtext(line,&current->text[1]);
    current->lines = 2;

    if (strcmp(current->text[0],"")==0 && strcmp(current->text[1],"")==0) {
	// void subtitle -> end of previous marked and exit
	previous_aqt_sub = NULL;
	return NULL;
	}

    return current;
}

static int sub_autodetect (std::ifstream& fd) {
    char line[LINE_LEN+1];
    int i,j=0;
    char p;

    while (j < 100) {
	j++;
	fd.getline(line, LINE_LEN);
	if (!fd.good()) return SUB_INVALID;

	if (sscanf (line, "{%d}{%d}", &i, &i)==2)
		{sub_uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "{%d}{}", &i)==1)
		{sub_uses_time=0;return SUB_MICRODVD;}
	if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{sub_uses_time=1;return SUB_SUBRIP;}
	if (sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d", &i, &i, &i, &i, &i, &i, &i, &i)==8)
		{sub_uses_time=1;return SUB_SUBVIEWER;}
	if (strstr (line, "<SAMI>"))
		{sub_uses_time=1; return SUB_SAMI;}
	if (sscanf (line, "%d:%d:%d:",     &i, &i, &i )==3)
		{sub_uses_time=1;return SUB_VPLAYER;}
	if (sscanf (line, "%d:%d:%d ",     &i, &i, &i )==3)
		{sub_uses_time=1;return SUB_VPLAYER;}
	//TODO: just checking if first line of sub starts with "<" is WAY
	// too weak test for RT
	// Please someone who knows the format of RT... FIX IT!!!
	// It may conflict with other sub formats in the future (actually it doesn't)
	if ( *line == '<' )
		{sub_uses_time=1;return SUB_RT;}

	if (!memcmp(line, "Dialogue: Marked", 16))
		{sub_uses_time=1; return SUB_SSA;}
	if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3)
		{sub_uses_time=0;return SUB_DUNNOWHAT;}
	if (sscanf (line, "FORMAT=%d", &i) == 1)
		{sub_uses_time=0; return SUB_MPSUB;}
	if (sscanf (line, "FORMAT=TIM%c", &p)==1 && p=='E')
		{sub_uses_time=1; return SUB_MPSUB;}
	if (strstr (line, "-->>"))
		{sub_uses_time=0; return SUB_MPSUB;}
    }

    return SUB_INVALID;  // too many bad lines
}

#ifdef USE_ICONV
static iconv_t icdsc;

void	subcp_open (void)
{
	const char *tocp = "UTF-8";
	icdsc = (iconv_t)(-1);
	if (sub_data.cp){
		if ((icdsc = iconv_open (tocp, sub_data.cp)) != (iconv_t)(-1)){
		    mpxp_dbg2<<"SUB: opened iconv descriptor"<<std::endl;
		    sub_data.utf8 = 2;
		} else
		    mpxp_dbg2<<"SUB: error opening iconv descriptor"<<std::endl;
	}
}

void	subcp_close (void)
{
	if (icdsc != (iconv_t)(-1)){
		(void) iconv_close (icdsc);
		mpxp_dbg2<<"SUB: closed iconv descriptor"<<std::endl;
	}
}

static const int ICBUFFSIZE=512;
static char icbuffer[ICBUFFSIZE];

subtitle* subcp_recode (subtitle *sub)
{
	int l=sub->lines;
	size_t ileft, oleft;
	char *op, *ip, *ot;

	while (l){
		op = icbuffer;
		ip = sub->text[--l];
		ileft = strlen(ip);
		oleft = ICBUFFSIZE - 1;

		if (iconv(icdsc, &ip, &ileft,
			  &op, &oleft) == (size_t)(-1)) {
			mpxp_err<<"SUB: error recoding line"<<std::endl;
			l++;
			break;
		}
		ot = new char[op - icbuffer + 1];
		*op='\0' ;
		strcpy (ot, icbuffer);
		delete sub->text[l];
		sub->text[l] = ot;
	}
	if (l){
		for (l = sub->lines; l;)
			delete sub->text[--l];
		return (subtitle*)ERR;
	}
	return sub;
}

subtitle* subcp_recode1 (subtitle *sub)
{
  int l=sub->lines;
  size_t ileft, oleft;

  if(icdsc == (iconv_t)(-1)) return sub;

  while (l){
     char *ip = icbuffer;
     char *op = sub->text[--l];
     strncpy(ip, op, ICBUFFSIZE);
     ileft = strlen(ip);
     oleft = ICBUFFSIZE - 1;

     if (iconv(icdsc, &ip, &ileft,
	      &op, &oleft) == (size_t)(-1)) {
	mpxp_v<<"SUB: error recoding line (2)"<<std::endl;
	return sub;
     }
     *op='\0' ;
  }
  return sub;
}

#endif

static void adjust_subs_time(subtitle* sub, float subtime, float fps){
	int n,m;
	subtitle* nextsub;
	int i = sub_num;
	unsigned long subfms = (sub_uses_time ? 100 : fps) * subtime;

	n=m=0;
	if (i)	for (;;){
		if (sub->end <= sub->start){
			sub->end = sub->start + subfms;
			m++;
			n++;
		}
		if (!--i) break;
		nextsub = sub + 1;
		if (sub->end >= nextsub->start){
			sub->end = nextsub->start - 1;
			if (sub->end - sub->start > subfms)
				sub->end = sub->start + subfms;
			if (!m)
				n++;
		}
		sub = nextsub;
		m = 0;
	}
	if (n) mpxp_dbg2<<"SUB: Adjusted "<<n<<" subtitle(s)"<<std::endl;
}

static const char *fmtname[] = { "microdvd", "subrip", "subviewer", "sami", "vplayer",
			"rt", "ssa", "dunnowhat", "mpsub", "aqt" };
static subtitle * (*__FASTCALL__ func[])(std::ifstream& fd,subtitle *dest)=
{
	sub_read_line_microdvd,
	sub_read_line_subrip,
	sub_read_line_subviewer,
	sub_read_line_sami,
	sub_read_line_vplayer,
	sub_read_line_rt,
	sub_read_line_ssa,
	sub_read_line_dunnowhat,
	sub_read_line_mpsub,
	sub_read_line_aqt

};
subtitle* sub_read_file (const std::string& filename, float fps) {
    std::ifstream fd;
    int n_max;
    subtitle *first;
    if(filename.empty()) return NULL; //qnx segfault
    fd.open (filename.c_str(),std::ios_base::in); if (!fd.is_open()) return NULL;

    sub_format=sub_autodetect (fd);
    if (sub_format==SUB_INVALID) { mpxp_err<<"SUB: Could not determine file format"<<std::endl; fd.close(); return NULL;}
    mpxp_info<<"SUB: Detected subtitle file format: "<<fmtname[sub_format]<<std::endl;

    fd.seekg(0,std::ios_base::beg);

#ifdef USE_ICONV
    subcp_open();
#endif

    sub_num=0;n_max=32;
    first=new subtitle[n_max];

    while(1){
	subtitle *sub;
	if(sub_num>=n_max){
	    n_max+=16;
	    first=(subtitle*)mp_realloc(first,n_max*sizeof(subtitle));
	}
	sub = &first[sub_num];
	memset(sub, '\0', sizeof(subtitle));
	sub=func[sub_format](fd,sub);
	if(!sub) break;   // EOF
#ifdef USE_ICONV
	if ((sub!=ERR) && (sub_data.utf8 & 2)) sub=subcp_recode(sub);
#endif
	if(sub==ERR) ++sub_errs; else ++sub_num; // Error vs. Valid
    }

    fd.close();

#ifdef USE_ICONV
    subcp_close();
#endif

    mpxp_dbg2<<"SUB: Read "<<sub_num<<" subtitles";
    if (sub_errs) mpxp_dbg2<<", "<<sub_errs<<" bad line(s).";
    else 	  mpxp_dbg2<<".";
    mpxp_dbg2<<std::endl;

    if(sub_num<=0){
	delete first;
	return NULL;
    }

    adjust_subs_time(first, 6.0, fps); /* ~6 secs AST */
    return first;
}

#if 0
char * strreplace( char * in,char * what,char * whereof )
{
 int i;
 char * tmp;

 if ( ( in == NULL )||( what == NULL )||( whereof == NULL )||( ( tmp=strstr( in,what ) ) == NULL ) ) return NULL;
 for( i=0;i<strlen( whereof );i++ ) tmp[i]=whereof[i];
 if ( strlen( what ) > strlen( whereof ) ) tmp[i]=0;
 return in;
}
#endif

static const char * sub_exts[] =
{   ".utf",
    ".UTF",
    ".sub",
    ".SUB",
    ".srt",
    ".SRT",
    ".smi",
    ".SMI",
    ".rt",
    ".RT",
    ".txt",
    ".TXT",
    ".ssa",
    ".SSA",
    ".aqt",
    ".AQT"
};
std::string sub_filename(const std::string& path,const std::string& fname )
{
 char * sub_name1;
 char * sub_name2;
 char * aviptr1, * aviptr2;
 unsigned i,j;
 std::ifstream f;

 if (fname.empty()) return NULL;

 sub_name1=(char *)strrchr(fname.c_str(),'.');
 if (!sub_name1) return NULL;

 sub_name1=new char [fname.length()+8];
 strcpy(sub_name1,fname.c_str());

 sub_name2=new char [path.length() + fname.length() + 8];
 size_t tmp=fname.rfind('/');
 if (tmp!=std::string::npos)
	sprintf (sub_name2, "%s%s", path.c_str(), fname.substr(tmp+1).c_str());
 else
	sprintf (sub_name2, "%s%s", path.c_str(), fname.c_str());

 aviptr1=strrchr(sub_name1,'.');
 aviptr2=strrchr(sub_name2,'.');

 for(j=0;j<=1;j++){
  char* sub_name=j?sub_name1:sub_name2;
#ifdef USE_ICONV
  for ( i=(sub_data.cp?2:0);i<(sizeof(sub_exts)/sizeof(char*));i++ ) {
#else
  for ( i=0;i<(sizeof(sub_exts)/sizeof(char*));i++ ) {
#endif
   strcpy(j?aviptr1:aviptr2,sub_exts[i]);
   f.open(sub_name);
   if(f.is_open()) {
     f.close();
     mpxp_info<<"SUB: Detected sub file: "<<sub_name<<std::endl;
     if (i<2) sub_data.utf8=1;
     return sub_name;
   }
  }
 }
 delete sub_name1;
 delete sub_name2;
 return "";
}

void list_sub_file(subtitle* subs){
    int i,j;

    for(j=0;j<sub_num;j++){
	subtitle* egysub=&subs[j];
	mpxp_info<<""<<egysub->lines<<" line"<<((1==egysub->lines)?' ':'s')<<" ("<<egysub->start<<"-"<<egysub->end<<")"<<std::endl;
	for (i=0; i<egysub->lines; i++) {
	    mpxp_info<<egysub->text[i]<<(i==egysub->lines-1?"":" <BREAK> ");
	}
	mpxp_info<<std::endl;
    }
    mpxp_info<<"Subtitle format "<<(sub_uses_time?"uses":"doesn't use")<<" time."<<std::endl;
    mpxp_info<<"Read "<<sub_num<<" subtitles, "<<sub_errs<<" errors"<<std::endl;
}

void dump_mpsub(subtitle* subs, float fps){
	int i,j;
	std::ofstream fd;
	float a,b;

	mpsub_position=0;
	if (mp_conf.sub_fps==0) mp_conf.sub_fps=fps;

	fd.open ("dump.mpsub",std::ios_base::out);
	if (!fd.is_open()) {
		perror ("dump_mpsub: fopen");
		return;
	}


	if (sub_uses_time) fd<<"FORMAT=TIME"<<std::endl<<std::endl;
	else fd<<"FORMAT="<<fps<<std::endl<<std::endl;

	for(j=0;j<sub_num;j++){
		subtitle* egysub=&subs[j];
		if (sub_uses_time) {
			a=((egysub->start-mpsub_position)/100.0);
			b=((egysub->end-egysub->start)/100.0);
			if ( (float)((int)a) == a) fd<<(int)a;
			else			fd<<a;

			if ( (float)((int)b) == b) fd<<(int)b;
			else			fd<<b;
		} else {
			fd<<(long)((egysub->start*(fps/mp_conf.sub_fps))-((mpsub_position*(fps/mp_conf.sub_fps))))
			<<" "<<(long)(((egysub->end)-(egysub->start))*(fps/mp_conf.sub_fps));
		}

		mpsub_position = egysub->end;
		for (i=0; i<egysub->lines; i++) {
			fd<<" "<<egysub->text[i]<<std::endl;
		}
		fd<<std::endl;
	}
	fd.close ();
	mpxp_dbg2<<"SUB: Subtitles dumped in \'dump.mpsub\'"<<std::endl;
}

void sub_free( subtitle * subs )
{
 int i;

 if ( !subs ) return;

 sub_num=0;
 sub_errs=0;
 for ( i=0;i<subs->lines;i++ ) delete subs->text[i] ;
 delete subs ;
 subs=NULL;
}
