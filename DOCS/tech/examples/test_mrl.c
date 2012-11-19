#include "mrl.h"

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
    if(user) mp_free(user);
    if(pass) mp_free(pass);
    if(ms) mp_free(ms);
    if(port) mp_free(port);
    return EXIT_SUCCESS;
}
#endif
