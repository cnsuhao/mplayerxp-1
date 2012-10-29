#include "mp_config.h"
#include "help_mp.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "libvo/img_format.h"
#include "mp_image.h"

#include "libmpconf/m_option.h"
#include "libmpconf/m_struct.h"
#include "libmpdemux/stream.h"
#include "libplaytree/asxparser.h"
#include "menu.h"
#include "menu_list.h"

#include "libvo/font_load.h"

#include "input/input.h"
#include "version.h"
#include "pp_msg.h"
#include "osdep/mplib.h"

struct list_entry_s {
  struct list_entry p;

  char* ok;
  char* cancel;
  char* left;
  char* right;
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
      if(c) mp_input_queue_cmd(c);
      break;
    } // fallback on ok if right is not defined
  case MENU_CMD_OK: {
    if(mpriv->p.current->ok) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->ok);
      if(c)
        {
          if (mpriv->auto_close)
              mp_input_queue_cmd (mp_input_parse_cmd ("menu hide"));
	mp_input_queue_cmd(c);
        }
    }
   } break;
  case MENU_CMD_LEFT:
    if(mpriv->p.current->left) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->left);
      if(c) mp_input_queue_cmd(c);
      break;
    } // fallback on cancel if left is not defined
  case MENU_CMD_CANCEL:
    if(mpriv->p.current->cancel) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->cancel);
      if(c)
	mp_input_queue_cmd(c);
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
    mp_free(entry->ok);
  if(entry->cancel)
    mp_free(entry->cancel);
  mp_free(entry->p.txt);
  mp_free(entry);
}

static void close_menu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static int parse_args(menu_t* menu,const char* args) {
  char *element,*body, **attribs, *name;
  list_entry_t* m = NULL;
  int r;
  ASX_Parser_t* parser = asx_parser_new();

  while(1) {
    r = asx_get_element(parser,&args,&element,&body,&attribs);
    if(r < 0) {
      MSG_WARN("[libmenu] Syntax error at line: %i\n",parser->line);
      asx_parser_free(parser);
      return -1;
    } else if(r == 0) {      
      asx_parser_free(parser);
      if(!m)
	MSG_WARN("[libmenu] No entry found in the menu definition\n");
      return m ? 1 : 0;
    }
    // Has it a name ?
    name = asx_get_attrib("name",attribs);
    if(!name) {
      MSG_WARN("[libmenu] ListMenu entry definitions need a name: %i\n",parser->line);
      mp_free(element);
      if(body) mp_free(body);
      asx_free_attribs(attribs);
      continue;
    }
    m = mp_calloc(1,sizeof(struct list_entry_s));
    m->p.txt = name;
    m->ok = asx_get_attrib("ok",attribs);
    m->cancel = asx_get_attrib("cancel",attribs);
    m->left = asx_get_attrib("left",attribs);
    m->right = asx_get_attrib("right",attribs);
    menu_list_add_entry(menu,m);

    mp_free(element);
    if(body) mp_free(body);
    asx_free_attribs(attribs);
  }
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

const menu_info_t menu_info_cmdlist = {
  "Command list menu",
  "cmdlist",
  "Albeu",
  "",
  {
    "cmdlist_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  open_cmdlist
};
