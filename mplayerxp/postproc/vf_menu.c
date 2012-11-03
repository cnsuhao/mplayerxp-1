
#include "mp_config.h"
#include "help_mp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mplayer.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"

#include "osdep/fastmemcpy.h"
#include "libvo/video_out.h"
#include "libvo/font_load.h"
#include "input/input.h"
#include "libmpconf/m_struct.h"
#include "libmenu/menu.h"
#include "osdep/mplib.h"
#include "pp_msg.h"


static struct vf_priv_s* st_priv = NULL;

static mp_image_t* pause_mpi = NULL;
static int go2pause = 0;
/// if nonzero display menu at startup
int attribute_used menu_startup = 0;

struct vf_priv_s {
  menu_t* root;
  menu_t* current;
  int passthrough;
};

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi);

void vf_menu_pause_update(struct vf_instance_s* vf) {
  if(pause_mpi) {
    vf->control(vf,VFCTRL_START_FRAME,NULL);
    put_slice(vf,pause_mpi);
  }
}

static void __FASTCALL__ set_menu(struct vf_priv_s * priv,const char *name)
{
	const char* menu = name;
	menu_t* l = priv->current;
	priv->current = menu_open(menu);
	if(!priv->current) {
	    MSG_WARN("[vf_menu] Failed to open menu: %s\n",menu);
	    priv->current = l;
	    priv->current->show = 0;
	} else {
	    priv->current->show = 1;
	    priv->current->parent = l;
	}
}

static int cmd_filter(mp_cmd_t* cmd, int paused, struct vf_priv_s * priv)
{
  switch(cmd->id) {
    case MP_CMD_PAUSE :
	if (!priv->current->show &&
	    (!priv->current->parent || !priv->current->parent->show))
	break;
	if(!paused && !go2pause) { // Initial pause cmd -> wait the next put_image
	    go2pause = 1;
	    return 1;
	}
	if(go2pause == 2) // Msg resent by put_image after saving the image
	    go2pause = 0;
	break;
    case MP_CMD_MENU : {  // Convert txt cmd from the users into libmenu stuff
	char* arg = cmd->args[0].v.s;
	if(!priv->current) MSG_WARN("[vf_menu] menu was not initialized\n");
	if(!(strcmp(arg,"enter") == 0)) priv->current->show = 1;
	else
	if(!(strcmp(arg,"hide") == 0)) priv->current->show = 0;
	else
	    MSG_WARN("[vf_menu] Unknown menu command: %s\n",arg);
	return 1;
    }
    case MP_CMD_SEEK:
	if(priv->current->show==1)
	{
	    int v = cmd->args[0].v.i;
	    switch(v)
	    {
		case -10: menu_read_cmd(priv->current,MENU_CMD_LEFT); break;
		case 10: menu_read_cmd(priv->current,MENU_CMD_RIGHT); break;
		case 60: menu_read_cmd(priv->current,MENU_CMD_UP); break;
		case -60: menu_read_cmd(priv->current,MENU_CMD_DOWN); break;
	    }
	    return 1;
	}
	break;
	case MP_CMD_CEXIT:
	case MP_CMD_QUIT:
	if(priv->current->show==1)
	{
	    menu_read_cmd(priv->current,MENU_CMD_CANCEL); break;
	    return 1;
	}
	break;
    case MP_CMD_PLAY_TREE_STEP:
	if(priv->current->show==1)
	{
	    menu_read_cmd(priv->current,MENU_CMD_OK); break;
	    return 1;
	}
	break;
    case MP_CMD_SET_MENU : {
	set_menu(priv,cmd->args[0].v.s);
	return 1;
    }
  }
  return 0;
}

static void __FASTCALL__ get_image(struct vf_instance_s* vf, mp_image_t *mpi){
  mp_image_t *dmpi;

  if(mpi->type == MP_IMGTYPE_TEMP && (!(mpi->flags&MP_IMGFLAG_PRESERVE)) ) {
    dmpi = vf_get_image(vf->next,mpi->imgfmt,mpi->type, mpi->flags,mpi->w, mpi->h,mpi->xp_idx);
    memcpy(mpi->planes,dmpi->planes,MP_MAX_PLANES*sizeof(unsigned char*));
    memcpy(mpi->stride,dmpi->stride,MP_MAX_PLANES*sizeof(unsigned int));
    mpi->flags|=MP_IMGFLAG_DIRECT;
    mpi->priv=(any_t*)dmpi;
    return;
  }
}
  
static void key_cb(int code) {
  menu_read_key(st_priv->current,code);
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
  mp_image_t *dmpi = NULL;

  if (vf->priv->passthrough) {
    dmpi=vf_get_image(vf->next, IMGFMT_MPEGPES, MP_IMGTYPE_EXPORT,
                      0, mpi->w, mpi->h,mpi->xp_idx);
    dmpi->planes[0]=mpi->planes[0];
    return vf_next_put_slice(vf,dmpi);
  }

  if(vf->priv->current->show 
  || (vf->priv->current->parent && vf->priv->current->parent->show)) {
  // Close all menu who requested it
  while(vf->priv->current->cl && vf->priv->current != vf->priv->root) {
    menu_t* m = vf->priv->current;
    vf->priv->current = m->parent ? m->parent :  vf->priv->root;
    menu_close(m);
  }

  // Step 1 : save the picture
  while(go2pause == 1) {
    static char delay = 0; // Hack : wait the 2 frame to be sure to show the right picture
    delay ^= 1; // after a seek
    if(!delay) break;

    if(pause_mpi && (mpi->w != pause_mpi->w || mpi->h != pause_mpi->h ||
		     mpi->imgfmt != pause_mpi->imgfmt)) {
      free_mp_image(pause_mpi);
      pause_mpi = NULL;
    }
    if(!pause_mpi)
      pause_mpi = alloc_mpi(mpi->w,mpi->h,mpi->imgfmt,XP_IDX_INVALID);
    copy_mpi(pause_mpi,mpi);
    mp_input_queue_cmd(mp_input_parse_cmd("pause"));
    go2pause = 2;
    break;
  }

  // Grab // Ungrab the keys
  if(!mp_input_key_cb && vf->priv->current->show)
    mp_input_key_cb = key_cb;
  if(mp_input_key_cb && !vf->priv->current->show)
    mp_input_key_cb = NULL;

  if(mpi->flags&MP_IMGFLAG_DIRECT)
    dmpi = mpi->priv;
  else {
    dmpi = vf_get_image(vf->next,mpi->imgfmt,
			MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			mpi->w,mpi->h,mpi->xp_idx);
    copy_mpi(dmpi,mpi);
  }
  menu_draw(vf->priv->current,dmpi);

  } else {
    if(mp_input_key_cb)
      mp_input_key_cb = NULL;

    if(mpi->flags&MP_IMGFLAG_DIRECT)
      dmpi = mpi->priv;
    else {
      dmpi = vf_get_image(vf->next,mpi->imgfmt,
                          MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,
                          mpi->w,mpi->h,mpi->xp_idx);

      dmpi->stride[0] = mpi->stride[0];
      dmpi->stride[1] = mpi->stride[1];
      dmpi->stride[2] = mpi->stride[2];
      dmpi->planes[0] = mpi->planes[0];
      dmpi->planes[1] = mpi->planes[1];
      dmpi->planes[2] = mpi->planes[2];
      dmpi->priv      = mpi->priv;
    }
  }
  return vf_next_put_slice(vf,dmpi);
}

static void __FASTCALL__ uninit(vf_instance_t *vf) {
     vf->priv=NULL;
     if(pause_mpi) {
       free_mp_image(pause_mpi);
       pause_mpi = NULL;
     }
}

static int __FASTCALL__ config(struct vf_instance_s* vf, int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt,any_t*tune) { 
#ifdef HAVE_FREETYPE
  // here is the right place to get screen dimensions
  if (force_load_font) {
    force_load_font = 0;
    load_font_ft(width,height,&vo_font,mp_conf.font_name);
  }
#endif
  if(outfmt == IMGFMT_MPEGPES) vf->priv->passthrough = 1;
  return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt,tune);
}

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
  return vf_next_query_format(vf,fmt,w,h);
}

static ControlCodes __FASTCALL__ open_vf(vf_instance_t *vf,const char* args){
  if(!st_priv) {
    st_priv = mp_calloc(1,sizeof(struct vf_priv_s));
    st_priv->root = st_priv->current = menu_open(args);
    if(!st_priv->current) {
      mp_free(st_priv);
      st_priv = NULL;
      return CONTROL_FALSE;
    }
    st_priv->root->show = menu_startup;
    mp_input_add_cmd_filter((mp_input_cmd_filter)cmd_filter,st_priv);
  }

  vf->config = config;
  vf->query_format=query_format;
  vf->put_slice = put_slice;
  vf->get_image = get_image;
  vf->uninit=uninit;
  vf->priv=st_priv;
  go2pause=0;

  return CONTROL_OK;
}

const vf_info_t vf_info_menu  = {
  "Internal filter for libmenu",
  "menu",
  "Albeu",
  "",
  VF_FLAGS_THREADS,
  open_vf
};



