#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

char *get_path(char *filename){
	char *homedir;
	char *buff;
	static char *config_dir = "/."PROGNAME;
	int len;

	if ((homedir = getenv("HOME")) == NULL)
		return NULL;
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}
	fprintf(stderr,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}
