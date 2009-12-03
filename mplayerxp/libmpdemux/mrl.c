#include <stdlib.h>
#include <string.h>
#include "mrl.h"
#include "demux_msg.h"

#undef TEST_MRL

#ifdef TEST_MRL
#include <stdio.h>
#endif

const char *mrl_parse_line(const char *line,char **user,char **pass,char **ms,char **port)
{
    unsigned ssize;
    const char *endp,*endl;
    if(user) *user=NULL;
    if(pass) *pass=NULL;
    if(ms) *ms=NULL;
    if(port) *port=NULL;
    if(!line) return line;
    endl=line+strlen(line);
    if(*line == '~')
    {
	line++;
	endp=strpbrk(line,"*@:#");
	if(!endp) endp=endl;
	if(user)
	{
	    ssize=endp-line+1;
	    *user=malloc(ssize);
	    memcpy(*user,line,ssize-1);
	    (*user)[ssize-1]='\0';
	}
	line=endp;
    }
    if(*line == '*')
    {
	line++;
	endp=strpbrk(line,"@:#");
	if(!endp) endp=endl;
	if(pass)
	{
	    ssize=endp-line+1;
	    *pass=malloc(ssize);
	    memcpy(*pass,line,ssize-1);
	    (*pass)[ssize-1]='\0';
	}
	line=endp;
    }
    if(*line=='@') line++;
    endp=strpbrk(line,":#");
    if(endp && endp!=line)
    {
	if(ms)
	{
	    ssize=endp-line+1;
	    *ms=malloc(ssize);
	    memcpy(*ms,line,ssize-1);
	    (*ms)[ssize-1]='\0';
	}
	line=endp;
    }
    if(*line == ':')
    {
	line++;
	endp=strchr(line,'#');
	if(!endp) endp=endl;
	if(port)
	{
	    ssize=endp-line+1;
	    *port=malloc(ssize);
	    memcpy(*port,line,ssize-1);
	    (*port)[ssize-1]='\0';
	}
	line=endp;
    }
    if(*line=='#') line++;
    return line;
}

static void mrl_store_args(const char *arg,char *value, mrl_config_t * args)
{
#ifdef TEST_MRL
    printf("arg='%s' value='%s'\n",arg,value);
    return;
#endif
    unsigned i;
    int done=0;
    i=0;
    while(arg!=NULL)
    {
	if(strcmp(arg,args[i].arg)==0)
	{
	    done=1;
	    switch(args[i].type)
	    {
		case MRL_TYPE_PRINT:
			MSG_INFO("%s", (char *)args[i].value);
		default:
			free(value);
			break;
		case MRL_TYPE_BOOL:
			if(strcasecmp(value,"on")==0 ||
			   strcasecmp(value,"yes")==0 ||
			   strcasecmp(value,"1")==0)
			    *((int *)args[i].value)=args[i].max;
			else
			    *((int *)args[i].value)=args[i].min;
			free(value);
			break;
		case MRL_TYPE_INT:
		{
		    int result=atoi(value);
		    free(value);
		    if(result < args[i].min) result=args[i].min;
		    if(result > args[i].max) result=args[i].max;
		    *((int *)args[i].value)=result;
		}
		break;
		case MRL_TYPE_FLOAT:
		{
		    int result=atof(value);
		    free(value);
		    if(result < args[i].min) result=args[i].min;
		    if(result > args[i].max) result=args[i].max;
		    *((float *)args[i].value)=result;
		}
		break;
		case MRL_TYPE_STRING:
		{
		    char *p=args[i].value;
		    p=value;
		    break;
		}
	    }
	    break;
	}
	i++;
    }
    if(!done) MSG_WARN(" Can't handle argument: '%s'",arg);
}

#define MRL_ARG_SEP ','

const char * mrl_parse_params(const char *param, mrl_config_t * args)
{
    const char *sep,*endp,*endl;
    char *arg=NULL,*value=NULL;
    unsigned ssize;
    endl=param+strlen(param);
    while(*param)
    {
	sep=strchr(param,'=');
	if(sep)
	{
	    sep++;
	    endp=strchr(sep,MRL_ARG_SEP);
	    if(!endp) endp=endl;
	    ssize=sep-param-1;
	    if(arg) free(arg);
	    arg=malloc(ssize+1);
	    memcpy(arg,param,ssize);
	    arg[ssize]='\0';
	    ssize=endp-sep;
	    value=malloc(ssize+1);
	    memcpy(value,sep,ssize);
	    value[ssize]='\0';
	    mrl_store_args(arg,value,args);
	    value=NULL;
	}
	else break;
	param=endp+1;
	if(endp==endl) { param--; break; }
    }
    if(arg) free(arg);
    if(value) free(value);
    return param;
}

#ifdef TEST_MRL
int main(int argc, char *argv[])
{
    char *user,*pass,*ms,*port;
    const char *param;
    if(argc < 2)
    {
	fprintf(stderr,"Needs argument\n");
	return EXIT_FAILURE;
    }
    if(memcmp(argv[1],"mrl://",6)!=0)
    {
	fprintf(stderr,"argument's line doesn't start from 'mrl://'!\n");
	return EXIT_FAILURE;
    }
    param=mrl_parse_line(&argv[1][6],&user,&pass,&ms,&port);
    printf("Source line: '%s'\n",argv[1]);
    printf("user: '%s'\n",user);
    printf("pass: '%s'\n",pass);
    printf("ms: '%s'\n",ms);
    printf("port: '%s'\n",port);
    printf("arguments: '%s'\n",param);
    param=mrl_parse_params(param,NULL);
    printf("unparsed params: '%s'\n",param);
    if(user) free(user);
    if(pass) free(pass);
    if(ms) free(ms);
    if(port) free(port);
    return EXIT_SUCCESS;
}
#endif
