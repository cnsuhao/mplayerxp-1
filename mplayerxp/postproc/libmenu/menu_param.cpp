#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "help_mp.h"

#include "libmpconf/cfgparser.h"
#include "libmpconf/m_struct.h"
#include "libmpconf/m_option.h"
#include "libmpconf/m_property.h"
#include "libplaytree/asxparser.h"

#include "libvo2/img_format.h"
#include "xmpcore/mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "input2/input.h"
#include "osdep/keycodes.h"
#include "pp_msg.h"

struct list_entry_s {
  struct list_entry p;
  const char* name;
  const char* txt;
  const char* prop;
  m_option_t* opt;
  const char* menu;
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* ptr;
  int edit;
  /// Cfg fields
  const char* na;
  int hide_na;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  NULL,
  0,
  "N/A",
  1
};

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", M_ST_OFF(menu_list_priv_t,title), MCONF_TYPE_STRING, 0, 0, 0, NULL },
  { "na", M_ST_OFF(struct menu_priv_s,na), MCONF_TYPE_STRING, 0, 0, 0, NULL },
  { "hide-na", M_ST_OFF(struct menu_priv_s,hide_na), MCONF_TYPE_FLAG, CONF_RANGE, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void entry_set_text(menu_t* menu, list_entry_t* e) {
  const char* val = e->txt ? m_properties_expand_string(e->opt, e->txt, menu->ctx) :
    mp_property_print(e->prop, menu->ctx);
  int l,edit = (mpriv->edit && e == mpriv->p.current);
  if(!val || !val[0]) {
    if(val) delete val;
    if(mpriv->hide_na) {
      e->p.hide = 1;
      return;
    }
    val = mp_strdup(mpriv->na);
  } else if(mpriv->hide_na)
      e->p.hide = 0;
  l = strlen(e->name) + 2 + strlen(val) + (edit ? 4 : 0) + 1;
  if(e->p.txt) delete e->p.txt;
  char * etxt = new char [l];
  sprintf(etxt,"%s: %s%s%s",e->name,edit ? "> " : "",val,edit ? " <" : "");
  e->p.txt=etxt;
  delete val;
}

static void update_entries(menu_t* menu) {
  list_entry_t* e;
  for(e = mpriv->p.menu ; e ; e = e->p.next)
    if(e->txt || e->prop) entry_set_text(menu,e);
}

static int parse_args(menu_t* menu,const char* args) {
  char *element,*body;
  ASX_Attrib attribs;
  std::string name,txt;
  list_entry_t* m = NULL;
  int r;
  m_option_t* opt;
  ASX_Parser& parser = *new(zeromem) ASX_Parser;

  while(1) {
    r = parser.get_element(&args,&element,&body,attribs);
    if(r < 0) {
      MSG_ERR("[libmenu] Syntax error at line: %s\n",parser.get_line());
      delete &parser;
      return -1;
    } else if(r == 0) {
      delete &parser;
      if(!m)
	MSG_WARN("[libmenu] No entry found in the menu definition\n");
      m = new(zeromem) struct list_entry_s;
      m->p.txt = mp_strdup("Back");
      menu_list_add_entry(menu,m);
      return 1;
    }
    if(!strcmp(element,"menu")) {
      name = attribs.get("menu");
      if(name.empty()) {
	MSG_WARN("[libmenu] Submenu definition need a menu attribut\n");
	goto next_element;
      }
      m = new(zeromem) struct list_entry_s;
      m->menu = mp_strdup(name.c_str());
      m->p.txt = mp_strdup(attribs.get("name").c_str());
      if(!m->p.txt) m->p.txt = mp_strdup(m->menu);
      menu_list_add_entry(menu,m);
      goto next_element;
    }

    name = attribs.get("property");
    opt = NULL;
    if(!name.empty() && mp_property_do(name.c_str(),M_PROPERTY_GET_TYPE,&opt,menu->ctx) <= 0) {
      MSG_WARN("[libmenu] Invalid property: %s %i\n",
	     name.c_str(),parser.get_line());
      goto next_element;
    }
    txt = attribs.get("txt");
    if(name.empty() || txt.empty()) {
      MSG_WARN("[libmenu] PrefMenu entry definitions need: %i\n",parser.get_line());
      goto next_element;
    }
    m = new(zeromem) struct list_entry_s;
    m->opt = opt;
    m->txt = mp_strdup(txt.c_str());
    m->prop = mp_strdup(name.c_str());
    m->name = mp_strdup(attribs.get("name").c_str());
    if(!m->name) m->name = mp_strdup(opt ? opt->name : "-");
    entry_set_text(menu,m);
    menu_list_add_entry(menu,m);

  next_element:
    delete element;
    if(body) delete body;
  }
  delete &parser;
  return -1;
}

static void read_key(menu_t* menu,int c) {
  menu_list_read_key(menu,c,0);
}

static void read_cmd(menu_t* menu,int cmd) {
  list_entry_t* e = mpriv->p.current;

  if(e->opt) {
    switch(cmd) {
    case MENU_CMD_UP:
      if(!mpriv->edit) break;
    case MENU_CMD_RIGHT:
      if(mp_property_do(e->prop,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
	update_entries(menu);
      return;
    case MENU_CMD_DOWN:
      if(!mpriv->edit) break;
    case MENU_CMD_LEFT:
      if(mp_property_do(e->prop,M_PROPERTY_STEP_DOWN,NULL,menu->ctx) > 0)
	update_entries(menu);
      return;

    case MENU_CMD_OK:
      // check that the property is writable
      if(mp_property_do(e->prop,M_PROPERTY_SET,NULL,menu->ctx) < 0) return;
      // shortcut for flags
      if(e->opt->type == MCONF_TYPE_FLAG) {
	if(mp_property_do(e->prop,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
	  update_entries(menu);
	return;
      }
      // switch
      mpriv->edit = !mpriv->edit;
      // update the menu
      update_entries(menu);
      // switch the pointer
      if(mpriv->edit) {
	mpriv->ptr = mpriv->p.ptr;
	mpriv->p.ptr = NULL;
      } else
	mpriv->p.ptr = mpriv->ptr;
      return;
    case MENU_CMD_CANCEL:
      if(!mpriv->edit) break;
      mpriv->edit = 0;
      update_entries(menu);
      mpriv->p.ptr = mpriv->ptr;
      return;
    }
  } else if(e->menu) {
    switch(cmd) {
    case MENU_CMD_RIGHT:
    case MENU_CMD_OK: {
      mp_cmd_t* c;
      char* txt = new char [10 + strlen(e->menu) + 1];
      sprintf(txt,"set_menu %s",e->menu);
      c = mp_input_parse_cmd(txt);
      if(c) mp_input_queue_cmd(menu->libinput,c);
      return;
    }
    }
  } else {
    switch(cmd) {
    case MENU_CMD_RIGHT:
    case MENU_CMD_OK:
      menu->show = 0;
      menu->cl = 1;
      return;
    }
  }
  menu_list_read_cmd(menu,cmd);
}

static void free_entry(list_entry_t* entry) {
  delete entry->p.txt;
  if(entry->name) delete entry->name;
  if(entry->txt)  delete entry->txt;
  if(entry->prop) delete entry->prop;
  if(entry->menu) delete entry->menu;
  delete entry;
}

static void closeMenu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static int openMenu(menu_t* menu,const char* args) {

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = closeMenu;


  if(!args) {
    MSG_ERR("[libmenu] PrefMenu needs an argument\n");
    return 0;
  }

  menu_list_init(menu);
  return parse_args(menu,args);
}

extern const menu_info_t menu_info_pref = {
  "Preferences menu",
  "pref",
  "Albeu",
  "",
  {
    "pref_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  openMenu
};
