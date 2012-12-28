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

#include "libplaytree2/playtree.h"
#include "input2/input.h"
#include "pp_msg.h"

static inline std::string mp_basename(const std::string& s) {
    size_t pos;
    pos=s.rfind('/');
    return (pos==std::string::npos)?s:s.substr(pos+1);
}

struct list_entry_s {
  struct list_entry p;
  PlayTree* pt;
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
    PlayTree* i;
    mp_cmd_t* c;
    PlayTree_Iter& pt_iter =mpxp_get_playtree_iter();

    if(pt_iter.get_tree() == mpriv->p.current->pt)
      break;

    if(pt_iter.get_tree()->get_parent() && mpriv->p.current->pt == pt_iter.get_tree()->get_parent())
      snprintf(str,15,"pt_up_step 1");
    else {
      for(i = pt_iter.get_tree()->get_next(); i != NULL ; i = i->get_next()) {
	if(i == mpriv->p.current->pt)
	  break;
	d++;
      }
      if(i == NULL) {
	d = -1;
	for(i = pt_iter.get_tree()->get_prev(); i != NULL ; i = i->get_prev()) {
	  if(i == mpriv->p.current->pt)
	    break;
	  d--;
	}
	if(i == NULL) {
	  mpxp_warn<<"[libmenu] Can't find the target item"<<std::endl;
	  break;
	}
      }
      snprintf(str,15,"pt_step %d",d);
    }
    c = mp_input_parse_cmd(str);
    if(c)
      mp_input_queue_cmd(menu->libinput,c);
    else
      mpxp_warn<<"[libmenu] Failed to build command: "<<str<<std::endl;
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
  PlayTree* i;
  list_entry_t* e;
  PlayTree_Iter& pt_iter = mpxp_get_playtree_iter();

  args = NULL; // Warning kill

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = close_menu;

  menu_list_init(menu);

  mpriv->p.title = mpriv->title;

  if(pt_iter.get_tree()->get_parent() != pt_iter.get_root()) {
    e = new(zeromem) list_entry_t;
    e->p.txt = "..";
    e->pt = pt_iter.get_tree()->get_parent();
    menu_list_add_entry(menu,e);
  }

  for(i = pt_iter.get_tree() ; i->get_prev() != NULL ; i = i->get_prev())
    /* NOP */;
  for( ; i != NULL ; i = i->get_next() ) {
    e = new(zeromem) list_entry_t;
    if(!i->get_files().empty())
      e->p.txt = mp_basename(i->get_file(0));
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
