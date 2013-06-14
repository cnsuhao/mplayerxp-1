#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * HTTP Cookies
 * Reads Netscape and Mozilla cookies.txt files
 *
 * by Dave Lambley <mplayer@davel.me.uk>
 */
#include <limits>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <inttypes.h>
#include <limits.h>

#include "cookies.h"
#include "http.h"
#include "network.h"
#include "stream_msg.h"

namespace	usr {
static const int MAX_COOKIES=20;
typedef struct cookie_list_type {
    char *name;
    char *value;
    char *domain;
    char *path;

    int secure;

    struct cookie_list_type *next;
} cookie_list_t;

/* Pointer to the linked list of cookies */
static struct cookie_list_type *cookie_list = NULL;


/* Like mp_strdup, but stops at anything <31. */
static char *col_dup(const char *src)
{
    char *dst;
    int length = 0;

    while (src[length] > 31)
	length++;

    dst = new char [length + 1];
    strncpy(dst, src, length);
    dst[length] = 0;

    return dst;
}

static int right_hand_strcmp(const char *cookie_domain, const char *url_domain)
{
    int c_l;
    int u_l;

    c_l = strlen(cookie_domain);
    u_l = strlen(url_domain);

    if (c_l > u_l)
	return -1;
    return strcmp(cookie_domain, url_domain + u_l - c_l);
}

static int left_hand_strcmp(const char *cookie_path, const char *url_path)
{
    return strncmp(cookie_path, url_path, strlen(cookie_path));
}

/* Finds the start of all the columns */
static int parse_line(char **ptr, char *cols[6])
{
    int col;
    cols[0] = *ptr;

    for (col = 1; col < 7; col++) {
	for (; (**ptr) > 31; (*ptr)++);
	if (**ptr == 0)
	    return 0;
	(*ptr)++;
	if ((*ptr)[-1] != 9)
	    return 0;
	cols[col] = (*ptr);
    }

    return 1;
}

/* Loads a file into RAM */
static char *load_file(const std::string& filename, off_t * length)
{
    int fd;
    char *buffer;

    mpxp_v<<"Loading cookie file: "<<filename<<std::endl;

    fd = ::open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
	mpxp_v<<"Could not open"<<std::endl;
	return NULL;
    }

    *length = ::lseek(fd, 0, SEEK_END);

    if (*length < 0) {
	mpxp_v<<"Could not find EOF"<<std::endl;
	::close(fd);
	return NULL;
    }

    if (unsigned(*length) > std::numeric_limits<size_t>::max() - 1) {
	mpxp_v<<"File too big, could not mp_malloc"<<std::endl;
	::close(fd);
	return NULL;
    }

    ::lseek(fd, SEEK_SET, 0);

    buffer = new char [*length + 1];
    if (::read(fd, buffer, *length) != *length) {
	delete buffer;
	mpxp_v<<"Read is behaving funny"<<std::endl;
	::close(fd);
	return NULL;
    }
    ::close(fd);
    buffer[*length] = 0;

    return buffer;
}

/* Loads a cookies.txt file into a linked list. */
static struct cookie_list_type *load_cookies_from(const std::string& filename,
						  struct cookie_list_type
						  *list)
{
    char *ptr;
    off_t length;

    mpxp_v<<"Loading cookie file: "<<filename<<std::endl;

    ptr = load_file(filename, &length);
    if (!ptr)
	return list;

    while (*ptr > 0) {
	char *cols[7];
	if (parse_line(&ptr, cols)) {
	    struct cookie_list_type *newc;
	    newc = new(zeromem) cookie_list_t;
	    newc->name = col_dup(cols[5]);
	    newc->value = col_dup(cols[6]);
	    newc->path = col_dup(cols[2]);
	    newc->domain = col_dup(cols[0]);
	    newc->secure = (*(cols[3]) == 't') || (*(cols[3]) == 'T');
	    newc->next = list;
	    list = newc;
	}
    }
    return list;
}

/* Attempt to load cookies.txt from various locations. Returns a pointer to the linked list contain the cookies. */
static struct cookie_list_type *load_cookies(void)
{
    DIR *dir;
    struct dirent *ent;
    struct cookie_list_type *list = NULL;
    std::string homedir,buf;

    if (net_conf.cookies_file)
	return load_cookies_from(net_conf.cookies_file, list);

    const std::map<std::string,std::string>& envm=mpxp_get_environment();
    std::map<std::string,std::string>::const_iterator it;
    it = envm.find("HOME");
    if(it==envm.end()) throw "No 'HOME' environment found";
    homedir = (*it).second;
    buf=homedir+"/.mozilla/default";

    dir = ::opendir(buf.c_str());

    if (dir) {
	while ((ent = ::readdir(dir)) != NULL) {
	    if ((ent->d_name)[0] != '.') {
		std::string buf2;
		buf2=buf+"/"+std::string(ent->d_name)+"/cookies.txt";
		list = load_cookies_from(buf2, list);
	    }
	}
	::closedir(dir);
    }

    buf=homedir+"/.netscape/cookies.txt";
    list = load_cookies_from(buf, list);

    return list;
}

/* Take an HTTP_header_t, and insert the correct headers. The cookie files are read if necessary. */
void HTTP_Header::cookies_set(const std::string& domain, const std::string& url)
{
    int found_cookies = 0;
    struct cookie_list_type *cookies[MAX_COOKIES];
    struct cookie_list_type *list, *start;
    int i;
    const char *path;
    char *buf;

    path = strchr(url.c_str(), '/');
    if (!path)
	path = "";

    if (!cookie_list)
	cookie_list = load_cookies();


    list = start = cookie_list;

    /* Find which cookies we want, removing duplicates. Cookies with the longest domain, then longest path take priority */
    while (list) {
	/* Check the cookie domain and path. Also, we never send "secure" cookies. These should only be sent over HTTPS. */
	if ((right_hand_strcmp(list->domain, domain.c_str()) == 0)
	    && (left_hand_strcmp(list->path, path) == 0) && !list->secure) {
	    int replacing = 0;
	    for (i = 0; i < found_cookies; i++) {
		if (strcmp(list->name, cookies[i]->name) == 0) {
		    replacing = 0;
		    if (strlen(list->domain) <= strlen(cookies[i]->domain)) {
			cookies[i] = list;
		    } else if (strlen(list->path) <= strlen(cookies[i]->path)) {
			cookies[i] = list;
		    }
		}
	    }
	    if (found_cookies > MAX_COOKIES) {
		/* Cookie jar overflow! */
		break;
	    }
	    if (!replacing)
		cookies[found_cookies++] = list;
	}
	list = list->next;
    }


    buf = mp_strdup("Cookie:");

    for (i = 0; i < found_cookies; i++) {
	char *nbuf;

	nbuf = new char [strlen(buf) + strlen(" ") + strlen(cookies[i]->name) +
		    strlen("=") + strlen(cookies[i]->value) + strlen(";") + 1];
	sprintf(nbuf, "%s %s=%s;", buf, cookies[i]->name,
		 cookies[i]->value);
	delete buf;
	buf = nbuf;
    }

    if (found_cookies)
	set_field(buf);
    else
	delete buf;
}
} // namespace	usr
