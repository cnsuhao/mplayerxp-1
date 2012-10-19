#include "mp_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_MALLOC
#include <malloc.h>
#endif

#include "../mplayer.h"
#include "video_out.h"
#include "font_load.h"
#include "sub.h"
#include "osd.h"
#include "../spudec.h"
#include "../vobsub.h"
#include "../libmpdemux/stream.h"
#define MSGT_CLASS MSGT_OSD
#include "../__mp_msg.h"

static const char * __sub_osd_names[]={
    "Seekbar",
    "Play",
    "Pause",
    "Stop",
    "Rewind",
    "Forward",
    "Clock",
    "Contrast",
    "Saturation",
    "Volume",
    "Brightness",
    "Hue"
};
static const char * __sub_osd_names_short[] ={ "", "|>", "||", "[]", "<<" , ">>", "", "", "", "", "", ""};
static int vo_osd_changed_status = 0;
static rect_highlight_t nav_hl;
static int draw_alpha_init_flag=0;
static mp_osd_obj_t* vo_osd_list=NULL;

sub_data_t sub_data = { NULL, 0, 0, 100, 0, 0 };


void __FASTCALL__ osd_set_nav_box (uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey) {
  nav_hl.sx = sx;
  nav_hl.sy = sy;
  nav_hl.ex = ex;
  nav_hl.ey = ey;
}

static void alloc_buf(mp_osd_obj_t* obj)
{
    int len;
    if (obj->bbox.x2 < obj->bbox.x1) obj->bbox.x2 = obj->bbox.x1;
    if (obj->bbox.y2 < obj->bbox.y1) obj->bbox.y2 = obj->bbox.y1;
    obj->stride = ((obj->bbox.x2-obj->bbox.x1)+7)&(~7);
    len = obj->stride*(obj->bbox.y2-obj->bbox.y1);
    if (obj->allocated<len) {
	obj->allocated = len;
	free(obj->bitmap_buffer);
	free(obj->alpha_buffer);
	obj->bitmap_buffer = (unsigned char *)memalign(16, len);
	obj->alpha_buffer = (unsigned char *)memalign(16, len);
    }
    memset(obj->bitmap_buffer, sub_data.bg_color, len);
    memset(obj->alpha_buffer, sub_data.bg_alpha, len);
}

// renders the buffer
inline static void vo_draw_text_from_buffer(unsigned idx,mp_osd_obj_t* obj,draw_osd_f draw_alpha){
    if (obj->allocated > 0) {
	draw_alpha(idx,
		   obj->bbox.x1,obj->bbox.y1,
		   obj->bbox.x2-obj->bbox.x1,
		   obj->bbox.y2-obj->bbox.y1,
		   obj->bitmap_buffer,
		   obj->alpha_buffer,
		   obj->stride);
    }
}

#define OSD_NAV_BOX_ALPHA 0x7f
static void vo_update_nav (mp_osd_obj_t *obj, int dxs, int dys) {
  int len;

  obj->bbox.x1 = obj->x = nav_hl.sx;
  obj->bbox.y1 = obj->y = nav_hl.sy;
  obj->bbox.x2 = nav_hl.ex;
  obj->bbox.y2 = nav_hl.ey;
  
  alloc_buf (obj);
  len = obj->stride * (obj->bbox.y2 - obj->bbox.y1);
  memset (obj->bitmap_buffer, OSD_NAV_BOX_ALPHA, len);
  memset (obj->alpha_buffer, OSD_NAV_BOX_ALPHA, len);
  obj->flags |= OSDFLAG_BBOX | OSDFLAG_CHANGED;
  if (obj->bbox.y2 > obj->bbox.y1 && obj->bbox.x2 > obj->bbox.x1)
    obj->flags |= OSDFLAG_VISIBLE;
}

// return the real height of a char:
static inline int __FASTCALL__ get_height(int c,int h){
    int font;
    if ((font=vo.font->font[c])>=0)
	if(h<vo.font->pic_a[font]->h) h=vo.font->pic_a[font]->h;
    return h;
}

int __FASTCALL__ get_osd_height(int c,int h)
{
    return vo.font?get_height(c,h):0;
}

inline static void __FASTCALL__ vo_update_text_osd(mp_osd_obj_t* obj,int dxs,int dys){
	unsigned char *cp=(unsigned char *)vo.osd_text;
	int x=20;
	int h=0;
	UNUSED(dxs);
	UNUSED(dys);
        obj->bbox.x1=obj->x=x;
        obj->bbox.y1=obj->y=10;

        while (*cp){
          int c=*cp++;
          x+=vo.font->width[c]+vo.font->charspace;
	  h=get_height(c,h);
        }
	
	obj->bbox.x2=x-vo.font->charspace;
	obj->bbox.y2=obj->bbox.y1+h;
	obj->flags|=OSDFLAG_BBOX;

}

inline static void __FASTCALL__ vo_draw_text_osd(unsigned idx,mp_osd_obj_t* obj,draw_osd_f draw_alpha){
	unsigned char *cp=(unsigned char *)vo.osd_text;
	int font;
        int x=obj->x;

        while (*cp){
          int c=*cp++;
          if ((font=vo.font->font[c])>=0 && c != ' ')
	    draw_alpha( idx,
			x,obj->y,
			vo.font->width[c],
			vo.font->pic_a[font]->h,
			vo.font->pic_b[font]->bmp+vo.font->start[c],
			vo.font->pic_a[font]->bmp+vo.font->start[c],
			vo.font->pic_a[font]->w);
	    x+=vo.font->width[c]+vo.font->charspace;
        }
}

// if we have n=256 bars then OSD progbar looks like below
// 
// 0   1    2    3 ... 256  <= vo_osd_progbar_value
// |   |    |    |       |
// [ ===  ===  === ... === ]
// 
//  the above schema is rescalled to n=elems bars

inline static void __FASTCALL__ vo_update_text_progbar(mp_osd_obj_t* obj,int dxs,int dys){

    obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;

    if(vo.osd_progbar_type<0 || !vo.font){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
    }

    {	int h=0;
        int y=(dys-vo.font->height)/2;
        int delimw=vo.font->width[OSD_PB_START]
		  +vo.font->width[OSD_PB_END]
		  +vo.font->charspace;
        int width=(2*dxs-3*delimw)/3;
	int charw=vo.font->width[OSD_PB_0]+vo.font->charspace;
        int elems=width/charw;
	int x=(dxs-elems*charw-delimw)/2;
	h=get_height(OSD_PB_START,h);
	h=get_height(OSD_PB_END,h);
	h=get_height(OSD_PB_0,h);
	h=get_height(OSD_PB_1,h);
	obj->bbox.x1=obj->x=x;
	obj->bbox.y1=obj->y=y;
	obj->bbox.x2=x+width+delimw;
	obj->bbox.y2=y+h; //vo.font->height;
	obj->flags|=OSDFLAG_BBOX;
	obj->params.progbar.elems=elems;
    }

}

inline static void __FASTCALL__ vo_draw_text_progbar(unsigned idx,mp_osd_obj_t* obj,draw_osd_f draw_alpha){
	unsigned char *s;
	unsigned char *sa;
	int i,w,h,st,mark;
	int x=obj->x;
	int y=obj->y;
	int c,font;
	int charw=vo.font->width[OSD_PB_0]+vo.font->charspace;
	int elems=obj->params.progbar.elems;

	if (vo.osd_progbar_value<=0)
	   mark=0;
	else {
	   int ev=vo.osd_progbar_value*elems;
	   mark=ev>>8;
	   if (ev & 0xFF)  mark++;
	   if (mark>elems) mark=elems;
	}

	c=vo.osd_progbar_type;
	if(vo.osd_progbar_type>0 && (font=vo.font->font[c])>=0) {
	    int xp=x-vo.font->width[c]-vo.font->spacewidth;
	   draw_alpha(idx,(xp<0?0:xp),y,
	      vo.font->width[c],
	      vo.font->pic_a[font]->h,
	      vo.font->pic_b[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->w);
	}

	c=OSD_PB_START;
	if ((font=vo.font->font[c])>=0)
	    draw_alpha(idx,x,y,
	      vo.font->width[c],
	      vo.font->pic_a[font]->h,
	      vo.font->pic_b[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->w);
	x+=vo.font->width[c]+vo.font->charspace;

	c=OSD_PB_0;
	if ((font=vo.font->font[c])>=0){
	   w=vo.font->width[c];
	   h=vo.font->pic_a[font]->h;
	   s=vo.font->pic_b[font]->bmp+vo.font->start[c];
	   sa=vo.font->pic_a[font]->bmp+vo.font->start[c];
	   st=vo.font->pic_a[font]->w;
	   if ((i=mark)) do {
	       draw_alpha(idx,x,y,w,h,s,sa,st);
	       x+=charw;
	   } while(--i);
	}

	c=OSD_PB_1;
	if ((font=vo.font->font[c])>=0){
	   w=vo.font->width[c];
	   h=vo.font->pic_a[font]->h;
	   s =vo.font->pic_b[font]->bmp+vo.font->start[c];
	   sa=vo.font->pic_a[font]->bmp+vo.font->start[c];
	   st=vo.font->pic_a[font]->w;
	   if ((i=elems-mark)) do {
	       draw_alpha(idx,x,y,w,h,s,sa,st);
	       x+=charw;
	   } while(--i);
	}

	c=OSD_PB_END;
	if ((font=vo.font->font[c])>=0)
	    draw_alpha(idx,x,y,
	      vo.font->width[c],
	      vo.font->pic_a[font]->h,
	      vo.font->pic_b[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->bmp+vo.font->start[c],
	      vo.font->pic_a[font]->w);
//        x+=vo.font->width[c]+vo.font->charspace;


//        vo_osd_progbar_value=(vo_osd_progbar_value+1)&0xFF;

}

// vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))

inline static void __FASTCALL__ vo_update_text_sub(mp_osd_obj_t* obj,int dxs,int dys){
   unsigned char *t;
   int c,i,j,l,font;
   int len;
   int k,lastk=0;
   int lastStripPosition;
   int xsize,lastxsize=0;
   int xmin=dxs,xmax=0;
   int h,lasth;

   obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;

   if(!vo.sub || !vo.font){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
   }

   obj->bbox.y2=obj->y=dys;
   obj->params.subtitle.lines=0;

      // too long lines divide into a smaller ones
      i=k=lasth=0;
      h=vo.font->height;
      xsize=-vo.font->charspace;
      lastStripPosition=-1;
      l=vo.sub->lines;

      while (l) {
	  l--;
	  t=vo.sub->text[i++];
	  len=strlen(t)-1;

	  for (j=0;j<=len;j++){
	      if ((c=t[j])>=0x80){
		 if (sub_data.utf8){
		    if ((c & 0xe0) == 0xc0)    /* 2 bytes U+00080..U+0007FF*/
		       c = (c & 0x1f)<<6 | (t[++j] & 0x3f);
		    else if((c & 0xf0) == 0xe0)/* 3 bytes U+00800..U+00FFFF*/
		       c = ((c & 0x0f)<<6 |
			    (t[++j] & 0x3f))<<6 | (t[++j] & 0x3f);
		 } else if (sub_data.unicode)
		       c = (c<<8) + t[++j];
	      }
	      if (k==MAX_UCS){
		 len=j; // end here
		 MSG_WARN("\nMAX_UCS exceeded!\n");
	      }
	      if (!c) c++; // avoid UCS 0
	      if (c==' '){
		 lastk=k;
		 lastStripPosition=j;
		 lastxsize=xsize;
	      } else if ((font=vo.font->font[c])>=0){
		  if (vo.font->pic_a[font]->h > h){
		     h=vo.font->pic_a[font]->h;
		  }
	      }
	      obj->params.subtitle.utbl[k++]=c;
	      xsize+=vo.font->width[c]+vo.font->charspace;
	      if (dxs<xsize){
		 if (lastStripPosition>0){
		    j=lastStripPosition;
		    xsize=lastxsize;
		    k=lastk;
		 } else {
		    xsize -=vo.font->width[c]+vo.font->charspace; // go back
		    k--; // cut line here
		    while (t[j] && t[j]!=' ') j++; // jump to the nearest space
		 }
	      } else if (j<len)
		   continue;
	      if (h>obj->y){ // out of the screen so end parsing
		 obj->y -= lasth - vo.font->height; // correct the y position
		 l=0;
		 break;
	      }
	      obj->params.subtitle.utbl[k++]=0;
	      obj->params.subtitle.xtbl[obj->params.subtitle.lines++]=(dxs-xsize)/2;
	      if(xmin>(dxs-xsize)/2) xmin=(dxs-xsize)/2;
	      if(xmax<(dxs+xsize)/2) xmax=(dxs+xsize)/2;
	      if (obj->params.subtitle.lines==MAX_UCSLINES||k>MAX_UCS){
		 l=0; len=j; // end parsing
	      } else if(l || j<len){ // not the last line or not the last char
		 lastStripPosition=-1;
		 xsize=-vo.font->charspace;
		 lasth=h;
		 h=vo.font->height;
	      }
	      obj->y -=h; // according to max of vo.font->pic_a[font]->h 
	  }
      }

    if (obj->y >= (dys * sub_data.pos / 100)){
	int old=obj->y;
	obj->y = dys * sub_data.pos /100;
	obj->bbox.y2-=old-obj->y;
    }

    // calculate bbox:
    obj->bbox.x1=xmin;
    obj->bbox.x2=xmax;
    obj->bbox.y1=obj->y;
//    obj->bbox.y2=obj->y+obj->params.subtitle.lines*vo.font->height;
    obj->flags|=OSDFLAG_BBOX;

}

inline static void __FASTCALL__ vo_draw_text_sub(unsigned idx,mp_osd_obj_t* obj,draw_osd_f draw_alpha){
   int i,j,c,x,l,font;
   int y=obj->y;

   i=j=0;
   if ((l=obj->params.subtitle.lines)) for (;;) {
	 x=obj->params.subtitle.xtbl[i++]; 
	 while ((c=obj->params.subtitle.utbl[j++])){
	       if ((font=vo.font->font[c])>=0)
		  draw_alpha(idx,x,y,
			     vo.font->width[c],
			     vo.font->pic_a[font]->h+y<obj->dys ? vo.font->pic_a[font]->h : obj->dys-y,
			     vo.font->pic_b[font]->bmp+vo.font->start[c],
			     vo.font->pic_a[font]->bmp+vo.font->start[c],
			     vo.font->pic_a[font]->w);
	          x+=vo.font->width[c]+vo.font->charspace;
	 }
         if (!--l) break;
         y+=vo.font->height;
   }
}

mp_osd_obj_t* __FASTCALL__ new_osd_obj(int type){
    mp_osd_obj_t* osd=malloc(sizeof(mp_osd_obj_t));
    memset(osd,0,sizeof(mp_osd_obj_t));
    osd->next=vo_osd_list;
    vo_osd_list=osd;
    osd->type=type;
    return osd;
}

void free_osd_list(void){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	mp_osd_obj_t* next=obj->next;
	free(obj);
	obj=next;
    }
    vo_osd_list=NULL;
}

int __FASTCALL__ vo_update_osd(int dxs,int dys){
    mp_osd_obj_t* obj=vo_osd_list;
    int chg=0;
    while(obj){
      if(dxs!=obj->dxs || dys!=obj->dys || obj->flags&OSDFLAG_FORCE_UPDATE){
        int vis=obj->flags&OSDFLAG_VISIBLE;
	obj->flags&=~OSDFLAG_BBOX;
	switch(obj->type){
	case OSDTYPE_SUBTITLE:
	    vo_update_text_sub(obj,dxs,dys);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_update_text_progbar(obj,dxs,dys);
	    break;
        case OSDTYPE_DVDNAV:
           vo_update_nav(obj,dxs,dys);
           break;
	case OSDTYPE_SPU:
	    if(vo.spudec && spudec_visible(vo.spudec))
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	case OSDTYPE_VOBSUB:
	    if(vo.vobsub)
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	case OSDTYPE_OSD:
	    if(vo.font && vo.osd_text && vo.osd_text[0]){
		vo_update_text_osd(obj,dxs,dys); // update bbox
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    } else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	}
	// check bbox:
	if(!(obj->flags&OSDFLAG_BBOX)){
	    // we don't know, so assume the whole screen changed :(
	    obj->bbox.x1=obj->bbox.y1=0;
	    obj->bbox.x2=dxs;
	    obj->bbox.y2=dys;
	    obj->flags|=OSDFLAG_BBOX;
	} else {
	    // check bbox, reduce it if it's out of bounds (corners):
	    if(obj->bbox.x1<0) obj->bbox.x1=0;
	    if(obj->bbox.y1<0) obj->bbox.y1=0;
	    if(obj->bbox.x2>dxs) obj->bbox.x2=dxs;
	    if(obj->bbox.y2>dys) obj->bbox.y2=dys;
	    if(obj->flags&OSDFLAG_VISIBLE)
	    // debug:
	    MSG_DBG2("OSD update: %d;%d %dx%d  \n",
		obj->bbox.x1,obj->bbox.y1,obj->bbox.x2-obj->bbox.x1,
		obj->bbox.y2-obj->bbox.y1);
	}
	// check if visibility changed:
	if(vis != (obj->flags&OSDFLAG_VISIBLE) ) obj->flags|=OSDFLAG_CHANGED;
	// remove the cause of automatic update:
	obj->dxs=dxs; obj->dys=dys;
	obj->flags&=~OSDFLAG_FORCE_UPDATE;
      }
      if(obj->flags&OSDFLAG_CHANGED){
        chg|=1<<obj->type;
	MSG_DBG2("OSD chg: %d  V: %s  pb:%d  \n",obj->type,(obj->flags&OSDFLAG_VISIBLE)?"yes":"no",vo.osd_progbar_type);
      }
      obj=obj->next;
    }
    return chg;
}

void vo_init_osd(void){
    if(!draw_alpha_init_flag){
	draw_alpha_init_flag=1;
	vo_draw_alpha_init();
    }
    if(vo_osd_list) free_osd_list();
    // temp hack, should be moved to mplayer/mencoder later
    new_osd_obj(OSDTYPE_OSD);
    new_osd_obj(OSDTYPE_SUBTITLE);
    new_osd_obj(OSDTYPE_PROGBAR);
    new_osd_obj(OSDTYPE_SPU);
    new_osd_obj(OSDTYPE_VOBSUB);
    new_osd_obj(OSDTYPE_DVDNAV);
}

void __FASTCALL__ vo_remove_text(unsigned idx,int dxs,int dys,clear_osd_f f_remove){
    mp_osd_obj_t* obj=vo_osd_list;
    vo_update_osd(dxs,dys);
    while(obj){
      if(((obj->flags&OSDFLAG_CHANGED) || (obj->flags&OSDFLAG_VISIBLE) || 
          (obj->cleared_frames>=0)) && 
         (obj->flags&OSDFLAG_OLD_BBOX)){
          int w=obj->old_bbox.x2-obj->old_bbox.x1;
	  int h=obj->old_bbox.y2-obj->old_bbox.y1;
	  if(w>0 && h>0){
	      vo.osd_changed_flag=obj->flags&OSDFLAG_CHANGED;	// temp hack
              f_remove(idx,obj->old_bbox.x1,obj->old_bbox.y1,w,h);
	  }
//	  obj->flags&=~OSDFLAG_OLD_BBOX;
          if(obj->cleared_frames>=0) {
              obj->cleared_frames++;
              if(obj->cleared_frames>=xp_num_frames)
                  obj->cleared_frames=-1;  // All cleared stop
          }
      }
      obj=obj->next;
    }
}

void __FASTCALL__ vo_draw_text(unsigned idx,int dxs,int dys,draw_osd_f draw_alpha){
    mp_osd_obj_t* obj=vo_osd_list;
    vo_update_osd(dxs,dys);

    while(obj){
      if(obj->flags&OSDFLAG_VISIBLE){
        obj->cleared_frames=0;
	vo.osd_changed_flag=obj->flags&OSDFLAG_CHANGED;	// temp hack
	switch(obj->type){
	case OSDTYPE_SPU:
	    spudec_draw_scaled(vo.spudec, dxs, dys, draw_alpha); // FIXME
	    break;
	case OSDTYPE_VOBSUB:
	    if(vo.spudec) spudec_draw_scaled(vo.spudec, dxs, dys, draw_alpha);  // FIXME
	    break;
	case OSDTYPE_OSD:
	    vo_draw_text_osd(idx,obj,draw_alpha);
	    break;
	case OSDTYPE_SUBTITLE:
	    vo_draw_text_sub(idx,obj,draw_alpha);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_draw_text_progbar(idx,obj,draw_alpha);
	    break;
        case OSDTYPE_DVDNAV:
	    vo_draw_text_from_buffer(idx,obj,draw_alpha);
	    break;
	}
	obj->old_bbox=obj->bbox;
	obj->flags|=OSDFLAG_OLD_BBOX;
      }
      obj->flags&=~OSDFLAG_CHANGED;
      obj=obj->next;
    }
}

int __FASTCALL__ vo_osd_changed(int new_value)
{
    mp_osd_obj_t* obj=vo_osd_list;
    int ret = vo_osd_changed_status;
    vo_osd_changed_status = new_value;

    while(obj){
	if(obj->type==new_value) obj->flags|=OSDFLAG_FORCE_UPDATE;
	obj=obj->next;
    }

    return ret;
}

//      BBBBBBBBBBBB   AAAAAAAAAAAAA  BBBBBBBBBBB
//              BBBBBBBBBBBB  BBBBBBBBBBBBB
//                        BBBBBBB

// return TRUE if we have osd in the specified rectangular area:
int __FASTCALL__ vo_osd_check_range_update(int x1,int y1,int x2,int y2){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	if(obj->flags&OSDFLAG_VISIBLE){
	    if(	(obj->bbox.x1<=x2 && obj->bbox.x2>=x1) &&
		(obj->bbox.y1<=y2 && obj->bbox.y2>=y1) ) return 1;
	}
	obj=obj->next;
    }
    return 0;
}
