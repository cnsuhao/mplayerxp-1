#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "libmpstream2/stream.h"
#include "asxparser.h"
#include "playtree.h"
#include "playtreeparser.h"
#include "playtree_msg.h"

namespace	usr {

static const int BUF_STEP=1024;
static const char* WHITES=" \n\r\t";

static void __FASTCALL__ strstrip(char* str) {
  char* i;

  for(i = str ; i[0] != '\0' && strchr(WHITES,i[0]) != NULL; i++)
    /* NOTHING */;
  if(i[0] != '\0') {
    memmove(str,i,strlen(i));
    for(i = str + strlen(str) ; strchr(WHITES,i[0]) != NULL; i--)
      /* NOTHING */;
    i[1] = '\0';
  } else
    str[0] = '\0';
}

char* PlayTree_Parser::get_line() {
    char *end,*line_end;
    int r,resize = 0;

    if(buffer == NULL) {
	buffer = new char [BUF_STEP];
	buffer_size = BUF_STEP;
	iter = buffer;
    }

    if(stream->eof() && (buffer_end == 0 || iter[0] == '\0')) return NULL;

    while(1) {
	if(resize) {
	    r = iter - buffer;
	    buffer = (char*)mp_realloc(buffer,buffer_size+BUF_STEP);
	    iter = buffer + r;
	    buffer_size += BUF_STEP;
	    resize = 0;
	}

	if(buffer_size - buffer_end > 1 && !stream->eof()) {
	    binary_packet bp = stream->read(buffer_size - buffer_end - 1); memcpy(buffer + buffer_end,bp.data(),bp.size());
	    r = bp.size();
	    if(r > 0) {
		buffer_end += r;
		buffer[buffer_end] = '\0';
	    }
	}

	end = strchr(iter,'\n');
	if(!end) {
	    if(stream->eof()) {
		end = buffer + buffer_end;
		break;
	    }
	    resize = 1;
	    continue;
	}
	break;
    }
    line_end = (end > iter && *(end-1) == '\r') ? end-1 : end;
    if(line_end - iter >= 0)
	line = (char*)mp_realloc(line,line_end - iter+1);
    else
	return NULL;
    if(line_end - iter > 0)
	strncpy(line,iter,line_end - iter);
    line[line_end - iter] = '\0';
    if(end[0] != '\0') end++;
    if(!keep) {
	if(end[0] != '\0') {
	    buffer_end -= end-iter;
	    memmove(buffer,end,buffer_end);
	} else
	    buffer_end = 0;
	    iter = buffer;
    } else iter = end;
    return line;
}

void PlayTree_Parser::reset() { iter = buffer; }

void PlayTree_Parser::stop_keeping() {
    keep = 0;
    if(iter && iter != buffer) {
	buffer_end -= iter - buffer;
	if(buffer_end) memmove(buffer,iter,buffer_end);
	iter = buffer;
    }
}

PlayTree* PlayTree_Parser::parse_asx(libinput_t& libinput) {
    int comments = 0,_get_line = 1;
    char* _line = NULL;

    mpxp_v<<"Trying asx..."<<std::endl;

    while(1) {
	if(_get_line) {
	    _line = get_line();
	    if(!_line) return NULL;
	    strstrip(_line);
	    if(_line[0] == '\0') continue;
	}
	if(!comments) {
	    if(_line[0] != '<') {
		mpxp_dbg2<<"First char isn't '<' but '"<<_line[0]<<"'"<<std::endl;
		mpxp_dbg3<<"Buffer = ["<<buffer<<"]"<<std::endl;
		return NULL;
	    } else if(strncmp(_line,"<!--",4) == 0) { // Comments
		comments = 1;
		_line += 4;
		if(_line[0] != '\0' && strlen(_line) > 0) _get_line = 0;
	    } else if(strncasecmp(_line,"<ASX",4) == 0) // We got an asx element
		break;
	    else // We don't get an asx
		return NULL;
	} else { // Comments
	    char* c;
	    c = strchr(_line,'-');
	    if(c) {
		if(strncmp(c,"--!>",4) == 0) { // End of comments
		    comments = 0;
		    _line = c+4;
		    if(_line[0] != '\0') _get_line = 0; // There is some more data on this line : keep it
		} else {
		    _line = c+1; // Jump the -
		    if(_line[0] != '\0') // Some more data
			_get_line = 0;
		    else  // End of line
			_get_line = 1;
		}
	    } else // No - on this line (or rest of line) : get next one
		_get_line = 1;
	}
    }
    mpxp_v<<"Detected asx format"<<std::endl;
    // We have an asx : load it in memory and parse
    while((_line = get_line()) != NULL)/* NOTHING */;
    mpxp_dbg3<<"Parsing asx file : ["<<buffer<<"]"<<std::endl;
    return ASX_Parser::build_tree(libinput,buffer,deep);
}

static char* __FASTCALL__ pls_entry_get_value(const char* line) {
  char* i;

  i = strchr(const_cast<char*>(line),'=');
  if(!i || i[1] == '\0')
    return NULL;
  else
    return i+1;
}

struct pls_entry_t {
  char* file;
  char* title;
  char* length;
};

static int __FASTCALL__ pls_read_entry(const char* line,pls_entry_t** _e,int* _max_entry,char** val) {
  int num,max_entry = (*_max_entry);
  pls_entry_t* e = (*_e);
  char* v;

  v = pls_entry_get_value(line);
  if(!v) {
    mpxp_err<<"No value in entry "<<line<<std::endl;
    return 0;
  }

  num = atoi(line);
  if(num < 0) {
    num = max_entry+1;
    mpxp_warn<<"No entry index in entry "<<line<<std::endl;
    mpxp_warn<<"Assuming "<<num<<std::endl;
  }
  if(num > max_entry) {
    e = (pls_entry_t*)mp_realloc(e,num*sizeof(pls_entry_t));
    memset(&e[max_entry],0,(num-max_entry)*sizeof(pls_entry_t));
    max_entry = num;
  }
  (*_e) = e;
  (*_max_entry) = max_entry;
  (*val) = v;

  return num;
}


PlayTree* PlayTree_Parser::parse_pls() {
    char *_line,*v;
    pls_entry_t* entries = NULL;
    int n_entries = 0,max_entry=0,num;
    PlayTree *list = NULL, *entry = NULL;

    mpxp_v<<"Trying winamp playlist..."<<std::endl;
    _line = get_line();
    strstrip(_line);
    if(strcasecmp(_line,"[playlist]")) return NULL;
    mpxp_v<<"Detected winamp playlist format"<<std::endl;
    stop_keeping();
    _line = get_line();
    if(!_line) return NULL;
    strstrip(_line);
    if(strncasecmp(_line,"NumberOfEntries",15) == 0) {
	v = pls_entry_get_value(_line);
	n_entries = atoi(v);
	if(n_entries < 0) mpxp_dbg2<<"Invalid number of entries : very funny !!!"<<std::endl;
	else		  mpxp_dbg2<<"Playlist claim to have "<<n_entries<<" entries. Let's see"<<std::endl;
	_line = get_line();
    }

    while(_line) {
	strstrip(_line);
	if(_line[0] == '\0') {
	    _line = get_line();
	    continue;
	}
	if(strncasecmp(_line,"File",4) == 0) {
	    num = pls_read_entry(_line+4,&entries,&max_entry,&v);
	    if(num < 0) mpxp_err<<"No value in entry "<<_line<<std::endl;
	    else	entries[num-1].file = mp_strdup(v);
	} else if(strncasecmp(_line,"Title",5) == 0) {
	    num = pls_read_entry(_line+5,&entries,&max_entry,&v);
	    if(num < 0) mpxp_err<<"No value in entry "<<_line<<std::endl;
	    else	entries[num-1].title = mp_strdup(v);
	} else if(strncasecmp(_line,"Length",6) == 0) {
	    num = pls_read_entry(_line+6,&entries,&max_entry,&v);
	    if(num < 0) mpxp_err<<"No value in entry "<<_line<<std::endl;
	    else	entries[num-1].length = mp_strdup(v);
	} else mpxp_warn<<"Unknow entry type "<<_line<<std::endl;
	_line = get_line();
    }

    for(num = 0; num < max_entry ; num++) {
	if(entries[num].file == NULL)
	    mpxp_err<<"Entry "<<(num+1)<<" don't have a file !!!!"<<std::endl;
	else {
	    mpxp_dbg2<<"Adding entry "<<entries[num].file<<std::endl;
	    entry = new(zeromem) PlayTree;
	    entry->add_file(entries[num].file);
	    delete entries[num].file;
	    if(list)	list->append_entry(entry);
	    else	list = entry;
	}
	if(entries[num].title) {
	    // When we have info in playtree we add this info
	    delete entries[num].title;
	}
	if(entries[num].length) {
	    // When we have info in playtree we add this info
	    delete entries[num].length;
	}
    }
    delete entries;

    entry = new(zeromem) PlayTree;
    entry->set_child(list);
    return entry;
}

PlayTree* PlayTree_Parser::parse_textplain() {
    char* _line;
    PlayTree *list = NULL, *entry = NULL;

    mpxp_v<<"Trying plaintext..."<<std::endl;
    stop_keeping();

    while((_line = get_line()) != NULL) {
	strstrip(_line);
	if(_line[0] == '\0') continue;
	entry = new(zeromem) PlayTree;
	entry->add_file(_line);
	if(!list)	list = entry;
	else		list->append_entry(entry);
    }
    if(!list) return NULL;
    entry = new(zeromem) PlayTree;
    entry->set_child(list);
    return entry;
}

PlayTree_Parser::PlayTree_Parser(Stream* _stream,int _deep)
		:stream(_stream),
		deep(_deep)
{
    keep = 1;
}

PlayTree_Parser::~PlayTree_Parser() {
    if(buffer) delete buffer;
    if(line) delete line;
}

PlayTree* PlayTree_Parser::get_play_tree(libinput_t& libinput) {
    PlayTree* tree = NULL;

    while(get_line() != NULL) {
	reset();

	tree = parse_asx(libinput);
	if(tree) break;
	reset();

	tree = parse_pls();
	if(tree) break;
	reset();

	// Here come the others formats ( textplain must stay the last one )
	tree = parse_textplain();
	if(tree) break;
	break;
    }

    if(tree)	mpxp_v<<"Playlist succefully parsed"<<std::endl;
    else	mpxp_err<<"Error while parsing playlist"<<std::endl;
    if(tree->cleanup()!=MPXP_Ok) mpxp_warn<<"Warning empty playlist"<<std::endl;
    return tree;
}
} // namespace	usr
