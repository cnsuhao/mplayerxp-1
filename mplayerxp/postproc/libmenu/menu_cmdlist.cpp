#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "help_mp.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "libvo2/img_format.h"
#include "xmpcore/mp_image.h"

#include "libmpconf/m_option.h"
#include "libmpconf/m_struct.h"
#include "libplaytree/asxparser.h"
#include "menu.h"
#include "menu_list.h"

#include "libvo2/font_load.h"

#include "input2/input.h"
#include "version.h"
#include "pp_msg.h"

struct list_entry_s {
  struct list_entry p;

  const char* ok;
  const char* cancel;
  const char* left;
  const char* right;
};

struct menu_priv_s {
  menu_list_priv_t p;
  int auto_close;
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s, m)

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  0,
};

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title",M_ST_OFF(struct menu_priv_s,p.title), MCONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF(auto_close), MCONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_RIGHT:
    if(mpriv->p.current->right) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->right);
      if(c) mp_input_queue_cmd(menu->libinput,c);
      break;
    } // fallback on ok if right is not defined
  case MENU_CMD_OK: {
    if(mpriv->p.current->ok) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->ok);
      if(c)
	{
	  if (mpriv->auto_close)
	      mp_input_queue_cmd (menu->libinput,mp_input_parse_cmd ("menu hide"));
	mp_input_queue_cmd(menu->libinput,c);
	}
    }
   } break;
  case MENU_CMD_LEFT:
    if(mpriv->p.current->left) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->left);
      if(c) mp_input_queue_cmd(menu->libinput,c);
      break;
    } // fallback on cancel if left is not defined
  case MENU_CMD_CANCEL:
    if(mpriv->p.current->cancel) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->cancel);
      if(c)
	mp_input_queue_cmd(menu->libinput,c);
      break;
    }
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static void read_key(menu_t* menu,int c){
  menu_list_read_key(menu,c,0);
}

static void free_entry(list_entry_t* entry) {
  if(entry->ok)
    delete entry->ok;
  if(entry->cancel)
    delete entry->cancel;
  delete entry->p.txt;
  delete entry;
}

static void close_menu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static int parse_args(menu_t* menu,const char* args) {
  ASX_Element element;
  std::string name;
  list_entry_t* m = NULL;
  int r;
  ASX_Parser& parser = *new(zeromem) ASX_Parser;

  while(1) {
    r = parser.get_element(&args,element);
    if(r < 0) {
      MSG_WARN("[libmenu] Syntax error at line: %i\n",parser.get_line());
      delete &parser;
      return -1;
    } else if(r == 0) {
      delete &parser;
      if(!m)
	MSG_WARN("[libmenu] No entry found in the menu definition\n");
      return m ? 1 : 0;
    }
    // Has it a name ?
    name = element.attribs().get("name");
    if(name.empty()) {
      MSG_WARN("[libmenu] ListMenu entry definitions need a name: %i\n",parser.get_line());
      continue;
    }
    m = new(zeromem) struct list_entry_s;
    m->p.txt = mp_strdup(name.c_str());
    m->ok = mp_strdup(element.attribs().get("ok").c_str());
    m->cancel = mp_strdup(element.attribs().get("cancel").c_str());
    m->left = mp_strdup(element.attribs().get("left").c_str());
    m->right = mp_strdup(element.attribs().get("right").c_str());
    menu_list_add_entry(menu,m);
  }
  delete &parser;
  return -1;
}

static int open_cmdlist(menu_t* menu, const char* args) {
  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = close_menu;

  if(!args) {
    MSG_WARN("libmenu] ListMenu needs an argument\n");
    return 0;
  }

  menu_list_init(menu);
  if(!parse_args(menu,args))
    return 0;
  return 1;
}

static const m_struct_t m_priv =
{
    "cmdlist_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
};

extern const menu_info_t menu_info_cmdlist = {
  "Command list menu",
  "cmdlist",
  "Albeu",
  "",
  &m_priv,
  open_cmdlist
};
