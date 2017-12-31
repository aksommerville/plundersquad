/* ps_widget_rgbslider.c
 * Horizontal slider for color editing, one channel.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

/* Object definition.
 */

struct ps_widget_rgbslider {
  struct ps_widget hdr;
  uint32_t rgba;
  int chid;
  struct ps_callback cb;
  int mousex,mousey;
};

#define WIDGET ((struct ps_widget_rgbslider*)widget)

/* Delete.
 */

static void _ps_rgbslider_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_rgbslider_init(struct ps_widget *widget) {
  widget->accept_mouse_focus=1;
  return 0;
}

/* Draw.
 */

static int _ps_rgbslider_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if ((widget->w<2)||(widget->h<2)) return 0;
  
  uint32_t rgba_left=WIDGET->rgba;
  uint32_t rgba_right=WIDGET->rgba;
  int thumbx=0x80;
  switch (WIDGET->chid) {
    case 0: rgba_left&=0x00ffffff; rgba_right|=0xff000000; thumbx=WIDGET->rgba>>24; break;
    case 1: rgba_left&=0xff00ffff; rgba_right|=0x00ff0000; thumbx=(WIDGET->rgba>>16)&0xff; break;
    case 2: rgba_left&=0xffff00ff; rgba_right|=0x0000ff00; thumbx=(WIDGET->rgba>>8)&0xff; break;
  }
  if (ps_video_draw_horz_gradient(parentx+widget->x,parenty+widget->y,widget->w,widget->h>>1,rgba_left,rgba_right)<0) return -1;

  thumbx=parentx+widget->x+(thumbx*widget->w)/255;
  struct akgl_vtx_raw vtxv[4]={
    {thumbx,parenty+widget->y+(widget->h>>1),0,0,0,255},
    {thumbx-3,parenty+widget->y+widget->h-1,0,0,0,255},
    {thumbx+3,parenty+widget->y+widget->h-1,0,0,0,255},
    {thumbx,parenty+widget->y+(widget->h>>1),0,0,0,255},
  };
  if (ps_video_draw_line_strip(vtxv,4)<0) return -1;
  
  return 0;
}

/* Receive input.
 */

static int _ps_rgbslider_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  return 0;
}

static int _ps_rgbslider_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (value&&(btnid==1)) {
    int v=(WIDGET->mousex*255)/widget->w;
    if (v<0) v=0;
    else if (v>255) v=255;
    switch (WIDGET->chid) {
      case 0: WIDGET->rgba=(WIDGET->rgba&0x00ffffff)|(v<<24); break;
      case 1: WIDGET->rgba=(WIDGET->rgba&0xff00ffff)|(v<<16); break;
      case 2: WIDGET->rgba=(WIDGET->rgba&0xffff00ff)|(v<<8); break;
    }
    if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_rgbslider={

  .name="rgbslider",
  .objlen=sizeof(struct ps_widget_rgbslider),

  .del=_ps_rgbslider_del,
  .init=_ps_rgbslider_init,

  .draw=_ps_rgbslider_draw,

  .mousemotion=_ps_rgbslider_mousemotion,
  .mousebutton=_ps_rgbslider_mousebutton,

};

/* Accessors.
 */
 
int ps_widget_rgbslider_set_channel(struct ps_widget *widget,int chid) {
  if (!widget||(widget->type!=&ps_widget_type_rgbslider)) return -1;
  WIDGET->chid=chid;
  return 0;
}

int ps_widget_rgbslider_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_rgbslider)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

int ps_widget_rgbslider_set_rgba(struct ps_widget *widget,uint32_t rgba) {
  if (!widget||(widget->type!=&ps_widget_type_rgbslider)) return -1;
  WIDGET->rgba=rgba;
  return 0;
}

uint32_t ps_widget_rgbslider_get_rgba(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_rgbslider)) return 0;
  return WIDGET->rgba;
}
