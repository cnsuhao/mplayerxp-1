/*
 * codec.conf parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * to compile tester app: gcc -Iloader/ -DTESTING -o codec-cfg codec-cfg.c
 * to compile CODECS2HTML: gcc -Iloader/ -DCODECS2HTML -o codecs2html codecs-cfg.c
 *
 * TODO: implement informat in CODECS2HTML too
 */

#define DEBUG

//disable asserts
#define NDEBUG 

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

// for mmioFOURCC:
#include "wine/avifmt.h"
#define MSGT_CLASS MSGT_CODECCFG
#include "__mp_msg.h"
#include "libvo/img_format.h"
#include "codec-cfg.h"


#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

#define TYPE_VIDEO	0
#define TYPE_AUDIO	1

#define PRINT_LINENUM MSG_ERR(" at line %d\n", line_num)

static int add_to_fourcc(const char *s, char *alias, unsigned int *fourcc,
		unsigned int *map)
{
	int i, j, freeslots;
	unsigned int tmp;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_FOURCC - i;
	if (!freeslots)
		goto err_out_too_many;

	do {
		tmp = mmioFOURCC(s[0], s[1], s[2], s[3]);
		for (j = 0; j < i; j++)
			if (tmp == fourcc[j])
				goto err_out_duplicated;
		fourcc[i] = tmp;
		map[i] = alias ? mmioFOURCC(alias[0], alias[1], alias[2], alias[3]) : tmp;
		s += 4;
		i++;
	} while ((*(s++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;
	if (*(--s) != '\0')
		goto err_out_parse_error;
	return 1;
err_out_duplicated:
	MSG_ERR("duplicated fourcc/format");
	return 0;
err_out_too_many:
	MSG_ERR("too many fourcc/format...");
	return 0;
err_out_parse_error:
	MSG_ERR("parse error");
	return 0;
}

static int add_to_format(const char *s, unsigned int *fourcc, unsigned int *fourccmap)
{
	int i, j;
	char *endptr;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		MSG_ERR("too many fourcc/format...");
		return 0;
	}

        fourcc[i]=fourccmap[i]=strtoul(s,&endptr,0);
	if (*endptr != '\0') {
		MSG_ERR("parse error");
		return 0;
	}
	for (j = 0; j < i; j++)
		if (fourcc[j] == fourcc[i]) {
			MSG_ERR("duplicated fourcc/format");
			return 0;
		}

	return 1;
}

static const struct {
    const char *name;
    const uint32_t num;
} fmt_table[] = {

{"Y800",  IMGFMT_Y800},

{"YVU9",  IMGFMT_YVU9},
{"IF09",  IMGFMT_IF09},

{"YV12",  IMGFMT_YV12},
{"I420",  IMGFMT_I420},
{"IYUV",  IMGFMT_IYUV},

{"YUY2",  IMGFMT_YUY2},
{"UYVY",  IMGFMT_UYVY},
{"YVYU",  IMGFMT_YVYU},

{"CLPL",  IMGFMT_CLPL},
{"CLJR",  IMGFMT_CLJR},
{"CYUV",  IMGFMT_cyuv},

{"444P",  IMGFMT_444P},
{"422P",  IMGFMT_422P},
{"411P",  IMGFMT_411P},

{"NV12",  IMGFMT_NV12},
{"NV21",  IMGFMT_NV21},
{"HM12",  IMGFMT_HM12},

{"IY41",  IMGFMT_IY41},
{"IYU1",  IMGFMT_IYU1},
{"IYU2",  IMGFMT_IYU2},

{"Y422",  IMGFMT_Y422},
{"Y211",  IMGFMT_Y211},
{"Y41P",  IMGFMT_Y41P},
{"Y41T",  IMGFMT_Y41T},
{"Y42T",  IMGFMT_Y42T},
{"YUNV",  IMGFMT_YUNV},
{"IYU2",  IMGFMT_IYU2},
{"V422",  IMGFMT_V422},
{"V655",  IMGFMT_V655},
{"YUVP",  IMGFMT_YUVP},
{"UYVP",  IMGFMT_UYVP},

{"RGB4",  IMGFMT_RGB|4},
{"RGB8",  IMGFMT_RGB|8},
{"RGB15", IMGFMT_RGB|15}, 
{"RGB16", IMGFMT_RGB|16},
{"RGB24", IMGFMT_RGB|24},
{"RGB32", IMGFMT_RGB|32},
{"RGBA",  IMGFMT_RGBA},
{"ARGB",  IMGFMT_ARGB},
{"RGB1",  IMGFMT_RGB|1},

{"BGR4",  IMGFMT_BGR|4},
{"BGR8",  IMGFMT_BGR|8},
{"BGR15", IMGFMT_BGR|15},
{"BGR16", IMGFMT_BGR|16},
{"BGR24", IMGFMT_BGR|24},
{"BGR32", IMGFMT_BGR|32},
{"BGRA",  IMGFMT_BGRA},
{"ABGR",  IMGFMT_ABGR},
{"BGR1",  IMGFMT_BGR|1},

{"MPES",  IMGFMT_MPEGPES},
{"JPEGNI", IMGFMT_ZRMJPEGNI},
{"JPEGIT", IMGFMT_ZRMJPEGIT},
{"JPEGIB", IMGFMT_ZRMJPEGIB},
{"MOCO_MPEG2",  IMGFMT_XVMC_MOCO_MPEG2},
{"IDCT_MPEG2",  IMGFMT_XVMC_IDCT_MPEG2},
{NULL,    0}
};


static int add_to_inout(const char *sfmt,const char *sflags, unsigned int *outfmt,
		unsigned char *outflags)
{

	static const char *flagstr[] = {
		"flip",
		"noflip",
		"yuvhack",
		NULL
	};

	int i, j, freeslots;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_OUTFMT - i;
	if (!freeslots)
		goto err_out_too_many;

	flags = 0;
	if(sflags) {
		do {
			for (j = 0; flagstr[j] != NULL; j++)
				if (!strncmp(sflags, flagstr[j],
							strlen(flagstr[j])))
					break;
			if (flagstr[j] == NULL)
				goto err_out_parse_error;
			flags|=(1<<j);
			sflags+=strlen(flagstr[j]);
		} while (*(sflags++) == ',');

		if (*(--sflags) != '\0')
			goto err_out_parse_error;
	}

	do {
		for (j = 0; fmt_table[j].name != NULL; j++)
			if (!strncmp(sfmt, fmt_table[j].name, strlen(fmt_table[j].name)))
				break;
		if (fmt_table[j].name == NULL)
			goto err_out_parse_error;
		outfmt[i] = fmt_table[j].num;
		outflags[i] = flags;
                ++i;
		sfmt+=strlen(fmt_table[j].name);
	} while ((*(sfmt++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;

	if (*(--sfmt) != '\0')
		goto err_out_parse_error;
        
	return 1;
err_out_too_many:
	MSG_ERR("too many out...");
	return 0;
err_out_parse_error:
	MSG_ERR("parse error");
	return 0;
}

static int validate_codec(codecs_t *c, int type)
{
	unsigned i;
	char *tmp_name = strdup(c->codec_name);

	for (i = 0; i < strlen(tmp_name) && isalnum(tmp_name[i]); i++)
		/* NOTHING */;

	if (i < strlen(tmp_name)) {
		MSG_ERR("\ncodec(%s) name is not valid!\n", c->codec_name);
		free(tmp_name);
		return 0;
	}

	if (!c->s_info) 
	{
	    strncpy(c->s_info,c->codec_name,sizeof(c->s_info));
	    c->s_info[sizeof(c->s_info)-1]=0;
	}
	free(tmp_name);
	return 1;
}

static short get_cpuflags(char *s)
{
	static const char *flagstr[] = {
		"mmx",
		"sse",
		"3dnow",
		NULL
	};
        int i;
	short flags = 0;

	do {
		for (i = 0; flagstr[i]; i++)
			if (!strncmp(s, flagstr[i], strlen(flagstr[i])))
				break;
		if (!flagstr[i])
			goto err_out_parse_error;
		flags |= 1<<i;
		s += strlen(flagstr[i]);
	} while (*(s++) == ',');

	if (*(--s) != '\0')
		goto err_out_parse_error;

	return flags;
err_out_parse_error:
	return 0;
}

static FILE *fp;
static int line_num = 0;
static char *line;
static char *token[MAX_NR_TOKEN];

static int get_token(int min, int max)
{
	static int read_nextline = 1;
	static int line_pos;
	int i;
	char c;

	if (max >= MAX_NR_TOKEN) {
		MSG_ERR("get_token(): max >= MAX_NR_TOKEN!");
		goto out_eof;
	}

	memset(token, 0x00, sizeof(*token) * max);

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto out_eof;
		line_pos = 0;
		++line_num;
		read_nextline = 0;
	}
	for (i = 0; i < max; i++) {
		while (isspace(line[line_pos]))
			++line_pos;
		if (line[line_pos] == '\0' || line[line_pos] == '#' ||
				line[line_pos] == ';') {
			read_nextline = 1;
			if (i >= min)
				goto out_ok;
			goto out_eol;
		}
		token[i] = line + line_pos;
		c = line[line_pos];
		if (c == '"' || c == '\'') {
			token[i]++;
			while (line[++line_pos] != c && line[line_pos])
				/* NOTHING */;
		} else {
			for (/* NOTHING */; !isspace(line[line_pos]) &&
					line[line_pos]; line_pos++)
				/* NOTHING */;
		}
		if (!line[line_pos]) {
			read_nextline = 1;
			if (i >= min - 1)
				goto out_ok;
			goto out_eol;
		}
		line[line_pos] = '\0';
		line_pos++;
	}
out_ok:
	return i;
out_eof:
	read_nextline = 1;
	return RET_EOF;
out_eol:
	return RET_EOL;
}

static codecs_t *video_codecs=NULL;
static codecs_t *audio_codecs=NULL;
static int nr_vcodecs = 0;
static int nr_acodecs = 0;

int parse_codec_cfg(const char *cfgfile)
{
	codecs_t *codec = NULL; // current codec
	codecs_t **codecsp = NULL;// points to audio_codecs or to video_codecs
	char *endptr;	// strtoul()...
	int *nr_codecsp;
	int codec_type;		/* TYPE_VIDEO/TYPE_AUDIO */
	int tmp, i;
	
	// in case we call it secont time
	if(video_codecs!=NULL)free(video_codecs);
	else video_codecs=NULL;
 
 	if(audio_codecs!=NULL)free(audio_codecs);
	else audio_codecs=NULL;
	
	nr_vcodecs = 0;
	nr_acodecs = 0;

	if(cfgfile==NULL)return 0; 
	
	if ((fp = fopen(cfgfile, "r")) == NULL) {
		MSG_FATAL("can't open '%s': %s\n", cfgfile, strerror(errno));
		return 0;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		MSG_FATAL("can't get memory for 'line': %s\n", strerror(errno));
		return 0;
	}

	MSG_INFO("Reading %s: ", cfgfile);
	/*
	 * check if the cfgfile starts with 'audiocodec' or
	 * with 'videocodec'
	 */
	while ((tmp = get_token(1, 1)) == RET_EOL)
		/* NOTHING */;
	if (tmp == RET_EOF)
		goto out;
	if (!strcmp(token[0], "audiocodec") || !strcmp(token[0], "videocodec"))
		goto loop_enter;
	goto err_out_parse_error;

	while ((tmp = get_token(1, 1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token[0], "audiocodec") ||
				!strcmp(token[0], "videocodec")) {
			if (!validate_codec(codec, codec_type))
				goto err_out_not_valid;
		loop_enter:
			if (*token[0] == 'v') {
				codec_type = TYPE_VIDEO;
				nr_codecsp = &nr_vcodecs;
				codecsp = &video_codecs;
			} else if (*token[0] == 'a') {
				codec_type = TYPE_AUDIO;
				nr_codecsp = &nr_acodecs;
				codecsp = &audio_codecs;
#ifdef DEBUG
			} else {
				MSG_ERR("picsba\n");
				goto err_out;
#endif
			}
		        if (!(*codecsp = (codecs_t *) realloc(*codecsp,
				sizeof(codecs_t) * (*nr_codecsp + 2)))) {
			    MSG_FATAL("can't realloc '*codecsp': %s\n", strerror(errno));
			    goto err_out;
		        }
			codec=*codecsp + *nr_codecsp;
			++*nr_codecsp;
                        memset(codec,0,sizeof(codecs_t));
			memset(codec->fourcc, 0xff, sizeof(codec->fourcc));
			memset(codec->outfmt, 0xff, sizeof(codec->outfmt));
			memset(codec->infmt, 0xff, sizeof(codec->infmt));
                        
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			for (i = 0; i < *nr_codecsp - 1; i++) {
				if(( (*codecsp)[i].codec_name!=NULL) && 
				    (!strcmp(token[0], (*codecsp)[i].codec_name)) ) {
					MSG_ERR("codec name '%s' isn't unique", token[0]);
					goto err_out_print_linenum;
				}
			}
			strncpy(codec->codec_name,token[0],sizeof(codec->codec_name));
			codec->codec_name[sizeof(codec->codec_name)-1]=0;
		} else if (!strcmp(token[0], "info")) {
			if (codec->s_info[0] || get_token(1, 1) < 0)
				goto err_out_parse_error;
			strncpy(codec->s_info,token[0],sizeof(codec->s_info));
			codec->s_info[sizeof(codec->s_info)-1]=0;
		} else if (!strcmp(token[0], "comment")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			strncat(codec->s_comment,token[0],sizeof(codec->s_comment));
			codec->s_comment[sizeof(codec->s_comment)-1]=0;
		} else if (!strcmp(token[0], "fourcc")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_fourcc(token[0], token[1],
						codec->fourcc,
						codec->fourccmap))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "format")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!add_to_format(token[0], codec->fourcc,codec->fourccmap))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "driver")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			strncpy(codec->driver_name,token[0],sizeof(codec->driver_name));
			codec->driver_name[sizeof(codec->driver_name)-1]=0;
		} else if (!strcmp(token[0], "dll")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			strncpy(codec->dll_name,token[0],sizeof(codec->dll_name));
			codec->dll_name[sizeof(codec->dll_name)-1]=0;
		} else if (!strcmp(token[0], "guid")) {
			if (get_token(11, 11) < 0)
				goto err_out_parse_error;
                        codec->guid.f1=strtoul(token[0],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
                        codec->guid.f2=strtoul(token[1],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
                        codec->guid.f3=strtoul(token[2],&endptr,0);
			if ((*endptr != ',' || *(endptr + 1) != '\0') &&
					*endptr != '\0')
				goto err_out_parse_error;
			for (i = 0; i < 8; i++) {
                            codec->guid.f4[i]=strtoul(token[i + 3],&endptr,0);
				if ((*endptr != ',' || *(endptr + 1) != '\0') &&
						*endptr != '\0')
					goto err_out_parse_error;
			}
		} else if (!strcmp(token[0], "out")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_inout(token[0], token[1], codec->outfmt,
						codec->outflags))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "in")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_inout(token[0], token[1], codec->infmt,
						codec->inflags))
				goto err_out_print_linenum;
		} else if (!strcmp(token[0], "flags")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "seekable"))
				codec->flags |= CODECS_FLAG_SEEKABLE;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "status")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcasecmp(token[0], "working"))
				codec->status = CODECS_STATUS_WORKING;
			else if (!strcasecmp(token[0], "crashing"))
				codec->status = CODECS_STATUS_NOT_WORKING;
			else if (!strcasecmp(token[0], "untested"))
				codec->status = CODECS_STATUS_UNTESTED;
			else if (!strcasecmp(token[0], "buggy"))
				codec->status = CODECS_STATUS_PROBLEMS;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "cpuflags")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->cpuflags = get_cpuflags(token[0])))
				goto err_out_parse_error;
    } else if (!strcasecmp(token[0], "priority")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
      codec->priority = atoi(token[0]);
		} else
			goto err_out_parse_error;
	}
	if (!validate_codec(codec, codec_type))
		goto err_out_not_valid;
	MSG_INFO("%d audio & %d video codecs\n", nr_acodecs, nr_vcodecs);
	if(video_codecs) video_codecs[nr_vcodecs].codec_name[0] = '\0';
	if(audio_codecs) audio_codecs[nr_acodecs].codec_name[0] = '\0';
out:
	free(line);
	line=NULL;
	fclose(fp);
	return 1;

err_out_parse_error:
	MSG_ERR("parse error");
err_out_print_linenum:
	PRINT_LINENUM;
err_out:
	if (audio_codecs)
		free(audio_codecs);
	if (video_codecs)
		free(video_codecs);
	video_codecs=NULL;
	audio_codecs=NULL;

	free(line);
	line=NULL;
	fclose(fp);
	return 0;
err_out_not_valid:
	MSG_ERR("codec is not defined correctly");
	goto err_out_print_linenum;
}

codecs_t *find_audio_codec(unsigned int fourcc, unsigned int *fourccmap,
		const codecs_t *start)
{
	return find_codec(fourcc, fourccmap, start, 1);
}

codecs_t *find_video_codec(unsigned int fourcc, unsigned int *fourccmap,
		const codecs_t *start)
{
	return find_codec(fourcc, fourccmap, start, 0);
}

codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,
		const codecs_t *start, int audioflag)
{
	int i, j;
	codecs_t *c;
        {
		if (audioflag) {
			i = nr_acodecs;
			c = audio_codecs;
		} else {
			i = nr_vcodecs;
			c = video_codecs;
		}
		if(!i) return NULL;
		for (/* NOTHING */; i--; c++) {
                        if(start && c<=start) continue;
			for (j = 0; j < CODECS_MAX_FOURCC; j++) {
				if (c->fourcc[j]==fourcc || c->driver_name==0) {
					if (fourccmap)
						*fourccmap = c->fourccmap[j];
					return c;
				}
			}
		}
	}
	return NULL;
}

void codecs_reset_selection(int audioflag){
	int i;
	codecs_t *c;
	if (audioflag) {
		i = nr_acodecs;
		c = audio_codecs;
	} else {
		i = nr_vcodecs;
		c = video_codecs;
	}
	if(i)
	for (/* NOTHING */; i--; c++)
		c->flags&=(~CODECS_FLAG_SELECTED);
}

void list_codecs(int audioflag){
	int i;
	codecs_t *c;

		if (audioflag) {
			i = nr_acodecs;
			c = audio_codecs;
			MSG_INFO("ac:      afm: status:   info:  [lib/dll]\n");
		} else {
			i = nr_vcodecs;
			c = video_codecs;
			MSG_INFO("vc:      vfm: status:   info:  [lib/dll]\n");
		}
		if(!i) return;
		for (/* NOTHING */; i--; c++) {
			char* s="unknown ";
			switch(c->status){
			  case CODECS_STATUS_WORKING:     s="working ";break;
			  case CODECS_STATUS_PROBLEMS:    s="problems";break;
			  case CODECS_STATUS_NOT_WORKING: s="crashing";break;
			  case CODECS_STATUS_UNTESTED:    s="untested";break;
			}
			if(c->dll_name)
			  MSG_INFO("%-11s %s  %s  %s  [%s]\n",c->codec_name,c->driver_name,s,c->s_info,c->dll_name);
			else
			  MSG_INFO("%-11s %s  %s  %s\n",c->codec_name,c->driver_name,s,c->s_info);
			
		}

}



#ifdef CODECS2HTML

void wrapline(FILE *f2,char *s){
    int c;
    if(!s){
        fprintf(f2,"-");
        return;
    }
    while((c=*s++)){
        if(c==',') fprintf(f2,"<br>"); else fputc(c,f2);
    }
}

void parsehtml(FILE *f1,FILE *f2,codecs_t *codec,int section,int dshow){
        int c,d;
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                fputc(c,f2);
                continue;
            }
            d=fgetc(f1);
            
            switch(d){
            case '.':
                return; // end of section
            case 'n':
                wrapline(f2,codec->name); break;
            case 'i':
                wrapline(f2,codec->info); break;
            case 'c':
                wrapline(f2,codec->comment); break;
            case 'd':
                wrapline(f2,codec->dll); break;
            case 'D':
                fprintf(f2,"%c",codec->driver==dshow?'+':'-'); break;
            case 'F':
                for(d=0;d<CODECS_MAX_FOURCC;d++)
                    if(!d || codec->fourcc[d]!=0xFFFFFFFF)
                        fprintf(f2,"%s%.4s",d?"<br>":"",(codec->fourcc[d]==0xFFFFFFFF || codec->fourcc[d]<0x20202020)?!d?"-":"":(char*) &codec->fourcc[d]);
                break;
            case 'f':
                for(d=0;d<CODECS_MAX_FOURCC;d++)
                    if(codec->fourcc[d]!=0xFFFFFFFF)
                        fprintf(f2,"%s0x%X",d?"<br>":"",codec->fourcc[d]);
                break;
            case 'Y':
                for(d=0;d<CODECS_MAX_OUTFMT;d++)
                    if(codec->outfmt[d]!=0xFFFFFFFF){
		        for (c=0; fmt_table[c].name; c++)
                            if(fmt_table[c].num==codec->outfmt[d]) break;
                        if(fmt_table[c].name)
                            fprintf(f2,"%s%s",d?"<br>":"",fmt_table[c].name);
                    }
                break;
            default:
                fputc(c,f2);
                fputc(d,f2);
            }
        }

}

void skiphtml(FILE *f1){
        int c,d;
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                continue;
            }
            d=fgetc(f1);
            if(d=='.') return; // end of section
        }
}

int main(void)
{
	codecs_t *cl;
        FILE *f1;
        FILE *f2;
        int c,d,i;
        int pos;
        int section=-1;
        int nr_codecs;
        int win32=-1;
        int dshow=-1;
        int win32ex=-1;

	if (!(nr_codecs = parse_codec_cfg("etc/codecs.conf")))
		return 0;

        f1=fopen("DOCS/codecs-in.html","rb"); if(!f1) exit(1);
        f2=fopen("DOCS/codecs-status.html","wb"); if(!f2) exit(1);
        
        while((c=fgetc(f1))>=0){
            if(c!='%'){
                fputc(c,f2);
                continue;
            }
            d=fgetc(f1);
            if(d>='0' && d<='9'){
                // begin section
                section=d-'0';
                printf("BEGIN %d\n",section);
                if(section>=5){
                    // audio
		    cl = audio_codecs;
		    nr_codecs = nr_acodecs;
                    dshow=7;win32=4;
                } else {
                    // video
		    cl = video_codecs;
		    nr_codecs = nr_vcodecs;
                    dshow=4;win32=2;win32ex=6;
                }
                pos=ftello(f1);
                for(i=0;i<nr_codecs;i++){
                    fseeko(f1,pos,SEEK_SET);
                    switch(section){
                    case 0:
                    case 5:
                        if(cl[i].status==CODECS_STATUS_WORKING)
                            if(!(cl[i].driver==win32 || cl[i].driver==dshow || cl[i].driver==win32ex))
                                parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 1:
                    case 6:
                        if(cl[i].status==CODECS_STATUS_WORKING)
                            if(cl[i].driver==win32 || cl[i].driver==dshow || cl[i].driver==win32ex)
                                parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 2:
                    case 7:
                        if(cl[i].status==CODECS_STATUS_PROBLEMS)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 3:
                    case 8:
                        if(cl[i].status==CODECS_STATUS_NOT_WORKING)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    case 4:
                    case 9:
                        if(cl[i].status==CODECS_STATUS_UNTESTED)
                            parsehtml(f1,f2,&cl[i],section,dshow);
                        break;
                    default:
                        printf("Warning! unimplemented section: %d\n",section);
                    }
                }
                fseeko(f1,pos,SEEK_SET);
                skiphtml(f1);
//void parsehtml(FILE *f1,FILE *f2,codecs_t *codec,int section,int dshow){
                
                continue;
            }
            fputc(c,f2);
            fputc(d,f2);
        }
        
        fclose(f2);
        fclose(f1);
	return 0;
}

#endif

#ifdef TESTING
int main(void)
{
	codecs_t *c;
        int i,j, nr_codecs, state;

	if (!(parse_codec_cfg("etc/codecs.conf")))
		return 0;
	if (!video_codecs)
		printf("no videoconfig.\n");
	if (!audio_codecs)
		printf("no audioconfig.\n");

	printf("videocodecs:\n");
	c = video_codecs;
	nr_codecs = nr_vcodecs;
	state = 0;
next:
	if (c) {
		printf("number of %scodecs: %d\n", state==0?"video":"audio",
		    nr_codecs);
		for(i=0;i<nr_codecs;i++, c++){
		    printf("\n============== %scodec %02d ===============\n",
			state==0?"video":"audio",i);
		    printf("name='%s'\n",c->name);
		    printf("info='%s'\n",c->info);
		    printf("comment='%s'\n",c->comment);
		    printf("dll='%s'\n",c->dll);
		    printf("flags=%X  driver=%d status=%d cpuflags=%d\n",
				    c->flags, c->driver, c->status, c->cpuflags);

		    for(j=0;j<CODECS_MAX_FOURCC;j++){
		      if(c->fourcc[j]!=0xFFFFFFFF){
			  printf("fourcc %02d:  %08X (%.4s) ===> %08X (%.4s)\n",j,c->fourcc[j],(char *) &c->fourcc[j],c->fourccmap[j],(char *) &c->fourccmap[j]);
		      }
		    }

		    for(j=0;j<CODECS_MAX_OUTFMT;j++){
		      if(c->outfmt[j]!=0xFFFFFFFF){
			  printf("outfmt %02d:  %08X (%.4s)  flags: %d\n",j,c->outfmt[j],(char *) &c->outfmt[j],c->outflags[j]);
		      }
		    }

		    for(j=0;j<CODECS_MAX_INFMT;j++){
		      if(c->infmt[j]!=0xFFFFFFFF){
			  printf("infmt %02d:  %08X (%.4s)  flags: %d\n",j,c->infmt[j],(char *) &c->infmt[j],c->inflags[j]);
		      }
		    }

		    printf("GUID: %08lX %04X %04X",c->guid.f1,c->guid.f2,c->guid.f3);
		    for(j=0;j<8;j++) printf(" %02X",c->guid.f4[j]);
		    printf("\n");

		    
		}
	}
	if (!state) {
		printf("audiocodecs:\n");
		c = audio_codecs;
		nr_codecs = nr_acodecs;
		state = 1;
		goto next;
	}
	return 0;
}

#endif
