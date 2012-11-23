#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "osdep_msg.h"
namespace mpxp {
char *get_path(const char *filename){
	char *homedir;
	char *buff;
	static const char *config_dir = "/."PROGNAME;
	int len;

	if ((homedir = getenv("HOME")) == NULL)
		return NULL;
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = (char *) mp_malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = (char *) mp_malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}
	MSG_V("get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}
}// namespace mpxp
