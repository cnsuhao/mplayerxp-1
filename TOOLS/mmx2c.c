/*
    This small utility was designed to convert MMX.S sources into .c files
*/
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buff[0xFFFF];

char *parse_insn(const char *name)
{
    static char rval[0xFFFF];
    strcpy(rval,"_m_");
    strcat(rval,name);
    return rval;
}

char *parse_ptr(const char *name)
{
    unsigned i,j;
    char idx[255],nam[0xFFFF];
    static char rval[0xFFFF];
    i=0;
    idx[0]='0';
    idx[1]=0;
    while(isdigit(name[i])) { idx[i]=name[i]; i++; }
    if(i) idx[i]=0;
    if(name[i]=='(') i++;
    j=0;
    while(name[i]!=')') nam[j++] = name[i++];
    nam[j]=0;
    if(nam[0]=='%') nam[0]='_';
    strcpy(rval,"&(((char *)");
    strcat(rval,nam);
    strcat(rval,")[");
    strcat(rval,idx);
    strcat(rval,"])");
    return rval;
}

char *parse_reg(const char *name)
{
    static char rval[0xFFFF];
    strcpy(rval,"mm[ ]");
    if(name[0]=='$') return &name[1];
    else
    if(isdigit(name[0]) || name[0]=='(') return parse_ptr(name);
    else
    if(strncmp(name,"%mm",3)==0) { rval[3]=name[3]; return rval; }
    else return name;
}

char *parse_line(const char *line)
{
    int is_movq;
    unsigned i,len;
    const char *p,*e;
    static char ret[0xFFFF];
    char insn[0xFFFF],dest[0xFFFF],src[0xFFFF];
    if(!strlen(line)) return line;
    if(strchr(line,'#')||strchr(line,'/')) return line;
    i=0;
    is_movq=0;
    dest[0]=src[0]='\0';
    while(isspace(line[i])) i++;
    p = &line[i];
    while(!isspace(line[i]))i++;
    e = &line[i];
    len = e-p;
    memcpy(insn,p,len);
    insn[len]='\0';
    if(strcmp(insn,"movq")==0 || strcmp(insn,"movd")==0) is_movq=1;
    while(isspace(line[i]))i++;
    p = &line[i];
    e = strchr(p,',');
    if(e) {
	len = e-p;
	memcpy(src,p,len);
	src[len]='\0';
	e=e+1;
	while(isspace(*e)) e++;
	strcpy(dest,e);
	i=0;
	while(!isspace(dest[i]))i++;
	dest[i]='\0';
    }
    strcpy(ret,"    ");
    if(dest[0] && !is_movq) {
	strcat(ret,parse_reg(dest));
	strcat(ret,"=");
	strcat(ret,parse_insn(insn));
    }
    else {
	if(is_movq) {
	    if(dest[0]=='%' && src[0]=='%') {
		strcat(ret,parse_reg(dest));
		strcat(ret,"=");
		strcat(ret,parse_reg(src));
		strcat(ret,";\n");
		return ret;
	    }
	    else
	    if(strncmp(dest,"%mm",3)==0) {
		strcat(ret,parse_reg(dest));
		strcat(ret,"=");
		strcat(ret,insn[3]=='q'?"_m_load(":"_m_load_half(");
		strcat(ret,parse_reg(src));
		strcat(ret,");\n");
		return ret;
	    }
	    else			 strcat(ret,insn[3]=='q'?"_m_store":"_m_store_half");
	}
	else
	strcat(ret,parse_insn(insn));
    }
    strcat(ret,"(");
    if(src[0]) {
	const char *ptr;
	int has_src=0;
	strcat(ret,parse_reg(dest));
	strcat(ret,",");
	ptr=parse_reg(src);
	if(ptr) {
	    if(ptr[0]=='&') {
		strcat(ret,"_m_load(");
		strcat(ret,ptr);
		strcat(ret,")");
		has_src=1;
	    }
	}
	if(!has_src) strcat(ret,ptr);
    }
    strcat(ret,");\n");
    return ret;
}

int main( int argc, char *argv[] )
{
    FILE *in,*out;
    if(argc<3) {
	printf(	"Too few arguments\n"
		"Usage: %s asmfile.S cfile.c\n"
		,argv[0]);
	return EXIT_FAILURE;
    }
    if(!(in = fopen(argv[1],"rt"))) {
	fprintf(stderr,"Can't open: '%s' due %s\n",argv[1],strerror(errno));
	return EXIT_FAILURE;
    }
    if(!(out = fopen(argv[2],"wt"))) {
	fclose(in);
	fprintf(stderr,"Can't open: '%s' due %s\n",argv[1],strerror(errno));
	return EXIT_FAILURE;
    }
    fputs("    __m64 mm[8];\n\n",out);
    while(!feof(in)) {
	fgets(buff,sizeof(buff),in);
	fputs(parse_line(buff),out);
    }
    fclose(in);
    fclose(out);
    return EXIT_SUCCESS;
}
