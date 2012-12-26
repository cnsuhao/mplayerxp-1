#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mpxp_help.h"

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"

#include "libmpconf/m_struct.h"
#include "libmpconf/m_option.h"
#include "menu.h"
#include "menu_list.h"

#include "libplaytree/playtree.h"
#include "input2/input.h"
#include "pp_msg.h"

static inline const char* mp_basename(const char *s) {
    return strrchr((s),'/')==NULL?(const char*)(s):(strrchr((s),'/')+1);
}

struct list_entry_s {
  struct list_entry p;
  play_tree_t* pt;
};


struct menu_priv_s {
  menu_list_priv_t p;
  const char* title;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  "Jump to"
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", ST_OFF(title),  MCONF_TYPE_STRING, 0, 0, 0, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_RIGHT:
  case MENU_CMD_OK: {
    int d = 1;
    char str[15];
    play_tree_t* i;
    mp_cmd_t* c;
    _PlayTree_Iter* _playtree_iter =mpxp_get_playtree_iter();

    if(_playtree_iter->get_tree() == mpriv->p.current->pt)
      break;

    if(_playtree_iter->get_tree()->parent && mpriv->p.current->pt == _playtree_iter->get_tree()->parent)
      snprintf(str,15,"pt_up_step 1");
    else {
      for(i = _playtree_iter->get_tree()->next; i != NULL ; i = i->next) {
	if(i == mpriv->p.current->pt)
	  break;
	d++;
      }
      if(i == NULL) {
	d = -1;
	for(i = _playtree_iter->get_tree()->prev; i != NULL ; i = i->prev) {
	  if(i == mpriv->p.current->pt)
	    break;
	  d--;
	}
	if(i == NULL) {
	  MSG_WARN("[libmenu] Can't find the target item\n");
	  break;
	}
      }
      snprintf(str,15,"pt_step %d",d);
    }
    c = mp_input_parse_cmd(str);
    if(c)
      mp_input_queue_cmd(menu->libinput,c);
    else
      MSG_WARN("[libmenu] Failed to build command: %s\n",str);
  } break;
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static void read_key(menu_t* menu,int c){
  menu_list_read_key(menu,c,1);
}

static void close_menu(menu_t* menu) {
  menu_list_uninit(menu,NULL);
}

static int op(menu_t* menu,const char* args) {
  play_tree_t* i;
  list_entry_t* e;
  _PlayTree_Iter* _playtree_iter = mpxp_get_playtree_iter();

  args = NULL; // Warning kill

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = close_menu;

  menu_list_init(menu);

  mpriv->p.title = mpriv->title;

  if(_playtree_iter->get_tree()->parent != _playtree_iter->get_root()) {
    e = new(zeromem) list_entry_t;
    e->p.txt = "..";
    e->pt = _playtree_iter->get_tree()->parent;
    menu_list_add_entry(menu,e);
  }

  for(i = _playtree_iter->get_tree() ; i->prev != NULL ; i = i->prev)
    /* NOP */;
  for( ; i != NULL ; i = i->next ) {
    e = new(zeromem) list_entry_t;
    if(i->files)
      e->p.txt = mp_basename(i->files[0]);
    else
      e->p.txt = "Group ...";
    e->pt = i;
    menu_list_add_entry(menu,e);
  }

  return 1;
}

static const m_struct_t m_priv =
{
    "pt_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
};

extern const menu_info_t menu_info_pt = {
  "Playtree menu",
  "pt",
  "Albeu",
  "",
  &m_priv,
  op
};
