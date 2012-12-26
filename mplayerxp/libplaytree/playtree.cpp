#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "libmpstream2/stream.h"
#include "playtree.h"
#include "playtree_msg.h"

static int
play_tree_is_valid(play_tree_t* pt);

play_tree_t*
play_tree_new(void) {
  play_tree_t* r = (play_tree_t*)mp_calloc(1,sizeof(play_tree_t));
  if(r == NULL)
    MSG_ERR("Can't allocate %d bytes of memory\n",sizeof(play_tree_t));
  r->entry_type = PLAY_TREE_ENTRY_NODE;
  return r;
}

void
play_tree_free(play_tree_t* pt, int childs) {
  play_tree_t* iter;

  if(childs) {
    for(iter = pt->child; iter != NULL; ) {
      play_tree_t* nxt=iter->next;
      play_tree_free(iter,1);
      iter = nxt;
    }
    pt->child = NULL;
  }

  play_tree_remove(pt,0,0);

  for(iter = pt->child ; iter != NULL ; iter = iter->next)
    iter->parent = NULL;

  //if(pt->params) delete pt->params;
  if(pt->files) {
    int i;
    for(i = 0 ; pt->files[i] != NULL ; i++)
      delete pt->files[i];
    delete pt->files;
  }

  delete pt;
}

void
play_tree_free_list(play_tree_t* pt, int childs) {
  play_tree_t* iter;

  for(iter = pt ; iter->prev != NULL ; iter = iter->prev)
    /* NOTHING */;

  while(iter) {
    play_tree_t* nxt = iter->next;
    play_tree_free(iter, childs);
    iter = nxt;
  }

}

void
play_tree_append_entry(play_tree_t* pt, play_tree_t* entry) {
  play_tree_t* iter;

  if(pt == entry)
    return;

  for(iter = pt ; iter->next != NULL ; iter = iter->next)
    /* NOTHING */;

  entry->parent = iter->parent;
  entry->prev = iter;
  entry->next = NULL;
  iter->next = entry;
}

void
play_tree_prepend_entry(play_tree_t* pt, play_tree_t* entry) {
  play_tree_t* iter;

  for(iter = pt ; iter->prev != NULL; iter = iter->prev)
    /* NOTHING */;

  entry->prev = NULL;
  entry->next = iter;
  entry->parent = iter->parent;

  iter->prev = entry;
  if(entry->parent) {
    entry->parent->child = entry;
  }
}

void
play_tree_insert_entry(play_tree_t* pt, play_tree_t* entry) {

  entry->parent = pt->parent;
  entry->prev = pt;
  if(pt->next) {
    entry->next = pt->next;
    entry->next->prev = entry;
  } else
    entry->next = NULL;
  pt->next = entry;

}

void
play_tree_remove(play_tree_t* pt, int free_it,int with_childs) {

  // Middle of list
  if(pt->prev && pt->next) {
    pt->prev->next = pt->next;
    pt->next->prev = pt->prev;
  } // End of list
  else if(pt->prev) {
    pt->prev->next = NULL;
  } // Begining of list
  else if(pt->next) {
    pt->next->prev = NULL;
    if(pt->parent) {
      pt->parent->child = pt->next;
    }
  } // The only one
  else if(pt->parent) {
    pt->parent->child = NULL;
  }

  pt->prev = pt->next = pt->parent = NULL;
  if(free_it)
    play_tree_free(pt,with_childs);

}

void
play_tree_set_child(play_tree_t* pt, play_tree_t* child) {
  play_tree_t* iter;

  for(iter = pt->child ; iter != NULL ; iter = iter->next)
    iter->parent = NULL;

  // Go back to first one
  for(iter = child ; iter->prev != NULL ; iter = iter->prev)
    /* NOTHING */;

  pt->child = iter;

  for( ; iter != NULL ; iter= iter->next)
    iter->parent = pt;

}

void
play_tree_set_parent(play_tree_t* pt, play_tree_t* parent) {
  play_tree_t* iter;

  if(pt->parent)
    pt->parent->child = NULL;

  for(iter = pt ; iter != NULL ; iter = iter->next)
    iter->parent = parent;

  if(pt->prev) {
    for(iter = pt->prev ; iter->prev != NULL ; iter = iter->prev)
      iter->parent = parent;
    iter->parent = parent;
    parent->child = iter;
  } else
    parent->child = pt;

}


void
play_tree_add_file(play_tree_t* pt,const char* file) {
  int n = 0;
  char* e;

  if(pt->entry_type != PLAY_TREE_ENTRY_NODE &&
     pt->entry_type != PLAY_TREE_ENTRY_FILE)
    return;

  if(pt->files) {
    for(n = 0 ; pt->files[n] != NULL ; n++)
      /* NOTHING */;
  }
  pt->files = (char**)mp_realloc((any_t*)pt->files,(n+2)*sizeof(char*));
  if(pt->files ==NULL) {
    MSG_ERR("Can't allocate %d bytes of memory\n",(n+2)*sizeof(char*));
    return;
  }

  e = pt->files[n] = mp_strdup(file);
  pt->files[n+1] = NULL;

  if(strncasecmp(e,"vcd://",6) == 0) {
    pt->entry_type = PLAY_TREE_ENTRY_VCD;
    memmove(e,e + 6,strlen(&e[6])+1);
  } else if(strncasecmp(e,"dvd://",6) == 0) {
    pt->entry_type = PLAY_TREE_ENTRY_DVD;
    memmove(e,&e[6],strlen(&e[6])+1);
  } else if(strncasecmp(e,"tv://",5) == 0) {
    pt->entry_type = PLAY_TREE_ENTRY_TV;
    memmove(e,&e[5],strlen(&e[5])+1);
  } else
    pt->entry_type = PLAY_TREE_ENTRY_FILE;

}

int
play_tree_remove_file(play_tree_t* pt,const char* file) {
  int n,f = -1;

  for(n=0 ; pt->files[n] != NULL ; n++) {
    if(strcmp(file,pt->files[n]) == 0)
      f = n;
  }

  if(f < 0) // Not found
    return 0;

  delete pt->files[f];

  if(n > 1) {
    memmove(&pt->files[f],&pt->files[f+1],(n-f)*sizeof(char*));
    pt->files = (char**)mp_realloc((any_t*)pt->files,n*sizeof(char*));
    if(pt->files == NULL) {
      MSG_ERR("Can't allocate %d bytes of memory\n",(n+2)*sizeof(char*));
      return -1;
    }
  } else {
    delete pt->files;
    pt->files = NULL;
  }

  return 1;
}

void play_tree_set_param(play_tree_t* pt,const char* name,const char* val) {
  int n = 0,ni = -1;

  if(pt->params) {
    for( ; pt->params[n].name != NULL ; n++) {
      if(strcasecmp(pt->params[n].name,name) == 0)
	ni = n;
    }
  }

  if(ni > 0) {
    if(pt->params[n].value != NULL) delete pt->params[n].value;
    pt->params[n].value = val != NULL ? mp_strdup(val) : NULL;
    return;
  }

  pt->params = (play_tree_param_t*)mp_realloc(pt->params,(n+2)*sizeof(play_tree_param_t));
  if(pt->params == NULL)
  {
    MSG_FATAL("Can't mp_realloc params\n");
    return;
  }
  pt->params[n].name = mp_strdup(name);
  pt->params[n].value = val != NULL ? mp_strdup(val) : NULL;
  memset(&pt->params[n+1],0,sizeof(play_tree_param_t));

  return;
}

int play_tree_unset_param(play_tree_t* pt,const char* name) {
  int n,ni = -1;

  for(n = 0 ; pt->params[n].name != NULL ; n++) {
    if(strcasecmp(pt->params[n].name,name) == 0)
      ni = n;
  }

  if(ni < 0)
    return 0;

  if(pt->params[ni].name) delete pt->params[ni].name;
  if(pt->params[ni].value) delete pt->params[ni].value;

  if(n > 1) {
    memmove(&pt->params[ni],&pt->params[ni+1],(n-ni)*sizeof(play_tree_param_t));
    pt->params = (play_tree_param_t*)mp_realloc(pt->params,n*sizeof(play_tree_param_t));
    if(pt->params == NULL) {
      MSG_ERR("Can't allocate %d bytes of memory\n",n*sizeof(play_tree_param_t));
      return -1;
    }
  } else {
    delete pt->params;
    pt->params = NULL;
  }

  return 1;
}

void play_tree_set_params_from(play_tree_t* dest,const play_tree_t* src) {
  int i;

  if(!src->params)
    return;

  for(i = 0; src->params[i].name != NULL ; i++)
    play_tree_set_param(dest,src->params[i].name,src->params[i].value);
  if(src->flags & PLAY_TREE_RND) // pass the random flag too
    dest->flags |= PLAY_TREE_RND;

}

// all children if deep < 0
void play_tree_set_flag(play_tree_t* pt, int flags , int deep) {
  play_tree_t*  i;

  pt->flags |= flags;

  if(deep && pt->child) {
    if(deep > 0) deep--;
    for(i = pt->child ; i ; i = i->next)
      play_tree_set_flag(i,flags,deep);
  }
}

void play_tree_unset_flag(play_tree_t* pt, int flags , int deep) {
  play_tree_t*  i;

  pt->flags &= ~flags;

  if(deep && pt->child) {
    if(deep > 0) deep--;
    for(i = pt->child ; i ; i = i->next)
      play_tree_unset_flag(i,flags,deep);
  }
}

static play_tree_t*
play_tree_rnd_step(play_tree_t* pt) {
  int count = 0;
  int r,rnd;
  time_t tim;
  play_tree_t *i,*head;

  // Count how many mp_free choice we have
  for(i = pt ; i->prev ; i = i->prev)
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;
  head = i;
  if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;
  if(pt->next)
  for(i = pt->next ; i ; i = i->next)
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;

  if(!count) return NULL;
  /* make it time depended */
  time(&tim);
  /* integer between 0 and RAND_MAX inclusive. */
  rnd=rand();
  r = (int)(((float)(count) * rnd) / (RAND_MAX + 1.0));
  if(r) rnd = r = count - (tim%r);

  for(i = head ; i  ; i=i->next) {
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) r--;
    if(r < 0) return i;
  }
  for(i = head ; i  ; i=i->next) {
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) return i;
  }

  MSG_ERR("Random stepping error r=%i\n",rnd);
  return NULL;
}

namespace mpxp {
_PlayTree_Iter::_PlayTree_Iter(play_tree_t* pt,m_config_t& _config)
		:root(pt),
		tree(NULL),
		config(_config) {
    if(pt->parent) loop = pt->parent->loop;
}

_PlayTree_Iter::_PlayTree_Iter(const _PlayTree_Iter& old)
		:root(old.root),
		tree(old.tree),
		config(old.config),
		loop(old.loop),
		file(old.file),
		num_files(old.num_files),
		mode(old.mode) {}

_PlayTree_Iter::~_PlayTree_Iter() {}

void _PlayTree_Iter::push_params() {
    int n;
    play_tree_t* pt;

    pt = tree;

    // We always push a config because we can set some option
    // while playing
    m_config_push(config);

    if(pt->params == NULL) return;

    for(n = 0; pt->params[n].name != NULL ; n++) {
	int e;
	if((e = m_config_set_option(config,pt->params[n].name,pt->params[n].value)) < 0)
	    mpxp_err<<"Error "<<e<<" while setting option '"<<pt->params[n].name<<"' with value '"<<pt->params[n].value<<"'"<<std::endl;
    }

    if(!pt->child) entry_pushed = 1;
    return;
}

int _PlayTree_Iter::step(int d,int with_nodes) {
    play_tree_t* pt;
    int rnd;

    if(tree == NULL) {
	tree = root;
	return step(0,with_nodes);
    }

    if(entry_pushed > 0) {
	entry_pushed = 0;
	m_config_pop(config);
    }

    if(tree->parent && (tree->parent->flags & PLAY_TREE_RND))
	rnd=mode = PLAY_TREE_ITER_RND;
    else
	rnd=mode = PLAY_TREE_ITER_NORMAL;

    file = -1;
    if(mode == PLAY_TREE_ITER_RND) pt = play_tree_rnd_step(tree);
    else if( d > 0 ) {
	int i;
	pt = tree;
	for(i = d ; i > 0 && pt ; i--) pt = pt->next;
	d = i ? i : 1;
    } else if(d < 0) {
	int i;
	pt = tree;
	for(i = d ; i < 0 && pt ; i++)  pt = pt->prev;
	d = i ? i : -1;
    } else pt = tree;

    if(pt == NULL) { // No next
	// Must we loop?
	if (mode == PLAY_TREE_ITER_RND) {
	    if (root->loop == 0) return PLAY_TREE_ITER_END;
	    play_tree_unset_flag(root, PLAY_TREE_RND_PLAYED, -1);
	    if (root->loop > 0) root->loop--;
	    // try again
	    return step(0, with_nodes);
	} else if(tree->parent && tree->parent->loop != 0 && ((d > 0 && loop != 0) || ( d < 0 && (loop < 0 || loop < tree->parent->loop)))) {
	    if(d > 0) { // Go back to the first one
		for(pt = tree ; pt->prev != NULL; pt = pt->prev) /* NOTHNG */;
		if(loop > 0) loop--;
	    } else if( d < 0 ) { // Or the last one
		for(pt = tree ; pt->next != NULL; pt = pt->next) /* NOTHNG */;
		if(loop >= 0 && loop < tree->parent->loop) loop++;
	    }
	    tree = pt;
	    return step(0,with_nodes);
	}
	// Go up one level
	return up_step(d,with_nodes);
    }
    // Is there any valid child?
    if(pt->child && play_tree_is_valid(pt->child)) {
	tree = pt;
	if(with_nodes) { // Stop on the node
	    return PLAY_TREE_ITER_NODE;
	} else      // Or follow it
	    return down_step(d,with_nodes);
    }
    // Is it a valid entry?
    if(! play_tree_is_valid(pt)) {
	if(d == 0 && rnd==PLAY_TREE_ITER_NORMAL) { // Can this happen ? FF: Yes!
	    mpxp_err<<"What to do now ???? Infinite loop if we continue"<<std::endl;
	    return PLAY_TREE_ITER_ERROR;
	} // Not a valid entry : go to next one
	return step(d,with_nodes);
    }

    tree = pt;

    for(d = 0 ; tree->files[d] != NULL ; d++) /* NOTHING */;
    num_files = d;

    push_params();
    entry_pushed = 1;
    if(mode == PLAY_TREE_ITER_RND) pt->flags |= PLAY_TREE_RND_PLAYED;

    return PLAY_TREE_ITER_ENTRY;
}

int _PlayTree_Iter::up_step(int d,int with_nodes) {
    file = -1;
    if(tree->parent == root->parent) return PLAY_TREE_ITER_END;

    loop = status_stack.top(); status_stack.pop();
    tree = tree->parent;

    // Pop subtree params
    m_config_pop(config);
    if(mode == PLAY_TREE_ITER_RND) tree->flags |= PLAY_TREE_RND_PLAYED;
    return step(d,with_nodes);
}

int _PlayTree_Iter::down_step(int d,int with_nodes) {
    file = -1;

    //  Push subtree params
    push_params();

    status_stack.push(loop);
    // Set new status
    loop = tree->loop-1;
    if(d >= 0) tree = tree->child;
    else {
	play_tree_t* pt;
	for(pt = tree->child ; pt->next != NULL ; pt = pt->next) /*NOTING*/;
	tree = pt;
    }
    return step(0,with_nodes);
}

std::string _PlayTree_Iter::get_file(int d) {
    const char* entry;

    if(tree->files == NULL) return "";
    if(file >= num_files-1 || file < -1) return "";
    if(d > 0) {
	if(file >= num_files - 1) file = 0;
	else file++;
    } else if(d < 0) {
	if(file <= 0) file = num_files - 1;
	else file--;
    }
    entry = tree->files[file];

    switch(tree->entry_type) {
	case PLAY_TREE_ENTRY_DVD :
	    if(strlen(entry) == 0) entry = "1";
	    m_config_set_option(config,"dvd",entry);
	    return std::string("DVD title ")+entry;
	case PLAY_TREE_ENTRY_VCD :
	    if(strlen(entry) == 0) entry = "1";
	    m_config_set_option(config,"vcd",entry);
	    return std::string("vcd://")+entry;
	case PLAY_TREE_ENTRY_TV: {
	    if(strlen(entry) != 0) {
		char *val = new char [strlen(entry) + 11 + 1];
		const char* e;
		std::string rs;
		sprintf(val,"on:channel=%s",entry);
		m_config_set_option(config,"tv",val);
		rs="TV channel ";
		e = strchr(entry,':');
		if(!e) rs+=std::string(entry).substr(0,255-11);
		else {
		    if(entry-e > 255) e = entry+255;
		    rs+=std::string(entry).substr(0,val-e);
		}
		delete val;
		return rs;
	    } else m_config_set_option(config,"tv","on");
	    return "TV";
	}
        default: break;
    }
    return entry;
}

} // namespace mpxp


static int
play_tree_is_valid(play_tree_t* pt) {
  play_tree_t* iter;

  if(pt->entry_type != PLAY_TREE_ENTRY_NODE) {
    return 1;
  }
  else if (pt->child != NULL) {
    for(iter = pt->child ; iter != NULL ; iter = iter->next) {
      if(play_tree_is_valid(iter))
	return 1;
    }
  }
  return 0;
}

play_tree_t*
play_tree_cleanup(play_tree_t* pt) {
  play_tree_t* iter, *tmp, *first;

  if( ! play_tree_is_valid(pt)) {
    play_tree_remove(pt,1,1);
    return NULL;
  }

  first = pt->child;

  for(iter = pt->child ; iter != NULL ; ) {
    tmp = iter;
    iter = iter->next;
    if(! play_tree_is_valid(tmp)) {
      play_tree_remove(tmp,1,1);
      if(tmp == first) first = iter;
    }
  }

  for(iter = first ; iter != NULL ; ) {
    tmp = iter;
    iter = iter->next;
    play_tree_cleanup(tmp);
  }

  return pt;

}

// HIGH Level API, by Fabian Franz (mplayer@fabian-franz.de)
//
_PlayTree_Iter* pt_iter_create(play_tree_t** ppt, m_config_t& config)
{
  _PlayTree_Iter* r=NULL;

  *ppt=play_tree_cleanup(*ppt);

  if(*ppt) {
    r = new _PlayTree_Iter(*ppt,config);
    if (r && r->step(0,0) != PLAY_TREE_ITER_ENTRY)
    {
      delete r;
      r = NULL;
    }
  }

  return r;
}

void pt_iter_destroy(_PlayTree_Iter** iter)
{
  if (iter && *iter)
  {
    delete *iter;
    iter=NULL;
  }
}

std::string pt_iter_get_file(_PlayTree_Iter* iter, int d)
{
  int i=0;
  std::string r;

  if (iter==NULL)
    return NULL;

  r = iter->get_file(d);

  while (r.empty() && d!=0)
  {
    if (iter->step(d,0) != PLAY_TREE_ITER_ENTRY) break;
    r=iter->get_file(d);
    i++;
  }

  return r;
}

void pt_iter_insert_entry(_PlayTree_Iter* iter, play_tree_t* entry)
{
  play_tree_t *pt = iter->get_tree();

  play_tree_insert_entry(pt, entry);
  play_tree_set_params_from(entry,pt);
}

void pt_iter_replace_entry(_PlayTree_Iter* iter, play_tree_t* entry)
{
  play_tree_t *pt = iter->get_tree();

  pt_iter_insert_entry(iter, entry);
  play_tree_remove(pt, 1, 1);
  iter->set_tree(entry);
}

//Add a new file as a new entry
void pt_add_file(play_tree_t** ppt,const char* filename)
{
  play_tree_t *pt = *ppt, *entry = play_tree_new();

  play_tree_add_file(entry, filename);
  if (pt)
    play_tree_append_entry(pt, entry);
  else
  {
    pt=entry;
    *ppt=pt;
  }
  play_tree_set_params_from(entry,pt);
}

void pt_add_gui_file(play_tree_t** ppt,const char* path,const char* file)
{
  char* wholename = new char [strlen(path)+strlen(file)+2];

  if (wholename)
  {
    strcpy(wholename, path);
    strcat(wholename, "/");
    strcat(wholename, file);
    pt_add_file(ppt, wholename);
    delete wholename; // As pt_add_file strdups it anyway!
  }
}

void pt_iter_goto_head(_PlayTree_Iter* iter)
{
  iter->reset_tree();
  iter->step(0, 0);
}
