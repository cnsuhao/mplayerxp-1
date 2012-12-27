#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

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

static char* __FASTCALL__ play_tree_parser_get_line(play_tree_parser_t* p) {
  char *end,*line_end;
  int r,resize = 0;

  if(p->buffer == NULL) {
    p->buffer = (char*)mp_malloc(BUF_STEP);
    p->buffer_size = BUF_STEP;
    p->iter = p->buffer;
  }

  if(p->stream->eof() && (p->buffer_end == 0 || p->iter[0] == '\0'))
    return NULL;

  while(1) {

    if(resize) {
      r = p->iter - p->buffer;
      p->buffer = (char*)mp_realloc(p->buffer,p->buffer_size+BUF_STEP);
      p->iter = p->buffer + r;
      p->buffer_size += BUF_STEP;
      resize = 0;
    }

    if(p->buffer_size - p->buffer_end > 1 && !p->stream->eof()) {
      r = p->stream->read(p->buffer + p->buffer_end,p->buffer_size - p->buffer_end - 1);
      if(r > 0) {
	p->buffer_end += r;
	p->buffer[p->buffer_end] = '\0';
      }
    }

    end = strchr(p->iter,'\n');
    if(!end) {
      if(p->stream->eof()) {
	end = p->buffer + p->buffer_end;
	break;
      }
      resize = 1;
      continue;
    }
    break;
  }

  line_end = (end > p->iter && *(end-1) == '\r') ? end-1 : end;
  if(line_end - p->iter >= 0)
    p->line = (char*)mp_realloc(p->line,line_end - p->iter+1);
  else
    return NULL;
  if(line_end - p->iter > 0)
    strncpy(p->line,p->iter,line_end - p->iter);
  p->line[line_end - p->iter] = '\0';
  if(end[0] != '\0')
    end++;

  if(!p->keep) {
    if(end[0] != '\0') {
      p->buffer_end -= end-p->iter;
      memmove(p->buffer,end,p->buffer_end);
    } else
      p->buffer_end = 0;
    p->iter = p->buffer;
  } else
    p->iter = end;

  return p->line;
}

static void __FASTCALL__ play_tree_parser_reset(play_tree_parser_t* p) {
  p->iter = p->buffer;
}

static void __FASTCALL__ play_tree_parser_stop_keeping(play_tree_parser_t* p) {
  p->keep = 0;
  if(p->iter && p->iter != p->buffer) {
    p->buffer_end -= p->iter -p->buffer;
    if(p->buffer_end)
      memmove(p->buffer,p->iter,p->buffer_end);
    p->iter = p->buffer;
  }
}


static PlayTree* parse_asx(libinput_t& libinput,play_tree_parser_t* p) {
  int comments = 0,get_line = 1;
  char* line = NULL;

  mpxp_v<<"Trying asx..."<<std::endl;

  while(1) {
    if(get_line) {
      line = play_tree_parser_get_line(p);
      if(!line)
	return NULL;
      strstrip(line);
      if(line[0] == '\0')
	continue;
    }
    if(!comments) {
      if(line[0] != '<') {
	mpxp_dbg2<<"First char isn't '<' but '"<<line[0]<<"'"<<std::endl;
	mpxp_dbg3<<"Buffer = ["<<p->buffer<<"]"<<std::endl;
	return NULL;
      } else if(strncmp(line,"<!--",4) == 0) { // Comments
	comments = 1;
	line += 4;
	if(line[0] != '\0' && strlen(line) > 0)
	  get_line = 0;
      } else if(strncasecmp(line,"<ASX",4) == 0) // We got an asx element
	break;
      else // We don't get an asx
	return NULL;
    } else { // Comments
      char* c;
      c = strchr(line,'-');
      if(c) {
	if (strncmp(c,"--!>",4) == 0) { // End of comments
	  comments = 0;
	  line = c+4;
	  if(line[0] != '\0') // There is some more data on this line : keep it
	    get_line = 0;

	} else {
	  line = c+1; // Jump the -
	  if(line[0] != '\0') // Some more data
	    get_line = 0;
	  else  // End of line
	    get_line = 1;
	}
      } else // No - on this line (or rest of line) : get next one
	get_line = 1;
    }
  }

  mpxp_v<<"Detected asx format"<<std::endl;

  // We have an asx : load it in memory and parse

  while((line = play_tree_parser_get_line(p)) != NULL)
    /* NOTHING */;

 mpxp_dbg3<<"Parsing asx file : ["<<p->buffer<<"]"<<std::endl;
 return ASX_Parser::build_tree(libinput,p->buffer,p->deep);
}

static char* __FASTCALL__ pls_entry_get_value(char* line) {
  char* i;

  i = strchr(line,'=');
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

static int __FASTCALL__ pls_read_entry(char* line,pls_entry_t** _e,int* _max_entry,char** val) {
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


PlayTree*
parse_pls(play_tree_parser_t* p) {
  char *line,*v;
  pls_entry_t* entries = NULL;
  int n_entries = 0,max_entry=0,num;
  PlayTree *list = NULL, *entry = NULL;

  mpxp_v<<"Trying winamp playlist..."<<std::endl;
  line = play_tree_parser_get_line(p);
  strstrip(line);
  if(strcasecmp(line,"[playlist]"))
    return NULL;
  mpxp_v<<"Detected winamp playlist format"<<std::endl;
  play_tree_parser_stop_keeping(p);
  line = play_tree_parser_get_line(p);
  if(!line)
    return NULL;
  strstrip(line);
  if(strncasecmp(line,"NumberOfEntries",15) == 0) {
    v = pls_entry_get_value(line);
    n_entries = atoi(v);
    if(n_entries < 0)
      mpxp_dbg2<<"Invalid number of entries : very funny !!!"<<std::endl;
    else
      mpxp_dbg2<<"Playlist claim to have "<<n_entries<<" entries. Let's see"<<std::endl;
    line = play_tree_parser_get_line(p);
  }

  while(line) {
    strstrip(line);
    if(line[0] == '\0') {
      line = play_tree_parser_get_line(p);
      continue;
    }
    if(strncasecmp(line,"File",4) == 0) {
      num = pls_read_entry(line+4,&entries,&max_entry,&v);
      if(num < 0)
	mpxp_err<<"No value in entry "<<line<<std::endl;
      else
	entries[num-1].file = mp_strdup(v);
    } else if(strncasecmp(line,"Title",5) == 0) {
      num = pls_read_entry(line+5,&entries,&max_entry,&v);
      if(num < 0)
	mpxp_err<<"No value in entry "<<line<<std::endl;
      else
	entries[num-1].title = mp_strdup(v);
    } else if(strncasecmp(line,"Length",6) == 0) {
      num = pls_read_entry(line+6,&entries,&max_entry,&v);
      if(num < 0)
	mpxp_err<<"No value in entry "<<line<<std::endl;
      else
	entries[num-1].length = mp_strdup(v);
    } else
      mpxp_warn<<"Unknow entry type "<<line<<std::endl;
    line = play_tree_parser_get_line(p);
  }

  for(num = 0; num < max_entry ; num++) {
    if(entries[num].file == NULL)
      mpxp_err<<"Entry "<<(num+1)<<" don't have a file !!!!"<<std::endl;
    else {
      mpxp_dbg2<<"Adding entry "<<entries[num].file<<std::endl;
      entry = new(zeromem) PlayTree;
      entry->add_file(entries[num].file);
      delete entries[num].file;
      if(list)
	list->append_entry(entry);
      else
	list = entry;
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

PlayTree*
parse_textplain(play_tree_parser_t* p) {
  char* line;
  PlayTree *list = NULL, *entry = NULL;

  mpxp_v<<"Trying plaintext..."<<std::endl;
  play_tree_parser_stop_keeping(p);

  while((line = play_tree_parser_get_line(p)) != NULL) {
    strstrip(line);
    if(line[0] == '\0')
      continue;
    entry = new(zeromem) PlayTree;
    entry->add_file(line);
    if(!list)
      list = entry;
    else
      list->append_entry(entry);
  }

  if(!list) return NULL;
  entry = new(zeromem) PlayTree;
  entry->set_child(list);
  return entry;
}

namespace mpxp {
PlayTree* PlayTree::parse_playtree(libinput_t&libinput,Stream* stream) {
  play_tree_parser_t* p;
  PlayTree* ret;

  p = play_tree_parser_new(stream,0);
  if(!p)
    return NULL;

  ret = play_tree_parser_get_play_tree(libinput,p);
  play_tree_parser_free(p);

  return ret;
}

PlayTree* PlayTree::parse_playlist_file(libinput_t&libinput,const std::string& file) {
  Stream *stream;
  PlayTree* ret;
  int ff;

  mpxp_v<<"Parsing playlist file "<<file<<"..."<<std::endl;
  ff=0;
  stream = new(zeromem) Stream;
  stream->open(libinput,file,&ff);
  stream->type(Stream::Type_Text);
  ret = PlayTree::parse_playtree(libinput,stream);
  delete stream;

  return ret;
}
} // namespace mpxp

play_tree_parser_t* play_tree_parser_new(Stream* stream,int deep) {
  play_tree_parser_t* p;

  p = (play_tree_parser_t*)mp_calloc(1,sizeof(play_tree_parser_t));
  if(!p)
    return NULL;
  p->stream = stream;
  p->deep = deep;
  p->keep = 1;

  return p;

}

void
play_tree_parser_free(play_tree_parser_t* p) {

  if(p->buffer) delete p->buffer;
  if(p->line) delete p->line;
  delete p;
}

PlayTree*
play_tree_parser_get_play_tree(libinput_t& libinput,play_tree_parser_t* p) {
  PlayTree* tree = NULL;

  while(play_tree_parser_get_line(p) != NULL) {
    play_tree_parser_reset(p);

    tree = parse_asx(libinput,p);
    if(tree) break;
    play_tree_parser_reset(p);

    tree = parse_pls(p);
    if(tree) break;
    play_tree_parser_reset(p);

    // Here come the others formats ( textplain must stay the last one )
    tree = parse_textplain(p);
    if(tree) break;
    break;
  }

  if(tree)
    mpxp_v<<"Playlist succefully parsed"<<std::endl;
  else mpxp_err<<"Error while parsing playlist"<<std::endl;

  if(tree->cleanup()!=MPXP_Ok) mpxp_warn<<"Warning empty playlist"<<std::endl;

  return tree;
}
