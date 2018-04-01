#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "util/ps_callback.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* Object definition.
 */

struct ps_widget_button {
  struct ps_widget hdr;
  int bevel_width;
  int border_width;
  int pressed;
  int hover;
  int track;
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_button*)widget)

/* Delete.
 */

static void _ps_button_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_button_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;
  widget->fgrgba=0x000000ff;
  WIDGET->bevel_width=3; // Always renders 1 pixel, but you can set higher to establish a margin.
  WIDGET->border_width=1;
  widget->accept_keyboard_focus=0;

  return 0;
}

/* Quick color modifications.
 */

static uint32_t ps_rgba_darken(uint32_t src) {
  int d=0x40;
  uint8_t r=src>>24,g=src>>16,b=src>>8,a=src;
  if (r<d) r=0; else r-=d;
  if (g<d) g=0; else g-=d;
  if (b<d) b=0; else b-=d;
  return (r<<24)|(g<<16)|(b<<8)|a;
}

static uint32_t ps_rgba_lighten(uint32_t src) {
  int d=0x40;
  uint8_t r=src>>24,g=src>>16,b=src>>8,a=src;
  if (r>0xff-d) r=0xff; else r+=d;
  if (g>0xff-d) g=0xff; else g+=d;
  if (b>0xff-d) b=0xff; else b+=d;
  return (r<<24)|(g<<16)|(b<<8)|a;
}

/* Draw.
 */
 
static int ps_button_draw_bevel(struct ps_widget *widget,int parentx,int parenty) {
  uint32_t rgba_nw,rgba_se;
  if (WIDGET->pressed) {
    rgba_nw=ps_rgba_darken(widget->bgrgba);
    rgba_se=ps_rgba_lighten(widget->bgrgba);
  } else {
    rgba_nw=ps_rgba_lighten(widget->bgrgba);
    rgba_se=ps_rgba_darken(widget->bgrgba);
  }
  
  struct akgl_vtx_raw vtxv[3];
  parentx+=widget->x;
  parenty+=widget->y;

  vtxv[0].x=parentx+WIDGET->border_width;
  vtxv[0].y=parenty+widget->h-WIDGET->border_width;
  vtxv[1].x=vtxv[0].x;
  vtxv[1].y=parenty+WIDGET->border_width;
  vtxv[2].x=parentx+widget->w-WIDGET->border_width;
  vtxv[2].y=vtxv[1].y;
  vtxv[0].r=vtxv[1].r=vtxv[2].r=rgba_nw>>24;
  vtxv[0].g=vtxv[1].g=vtxv[2].g=rgba_nw>>16;
  vtxv[0].b=vtxv[1].b=vtxv[2].b=rgba_nw>>8;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=rgba_nw;
  if (ps_video_draw_line_strip(vtxv,3)<0) return -1;

  vtxv[0].x=parentx+widget->w-WIDGET->border_width;
  vtxv[0].y=parenty+WIDGET->border_width;
  vtxv[1].x=vtxv[0].x;
  vtxv[1].y=parenty+widget->h-WIDGET->border_width;
  vtxv[2].x=parentx+WIDGET->border_width;
  vtxv[2].y=vtxv[1].y;
  vtxv[0].r=vtxv[1].r=vtxv[2].r=rgba_se>>24;
  vtxv[0].g=vtxv[1].g=vtxv[2].g=rgba_se>>16;
  vtxv[0].b=vtxv[1].b=vtxv[2].b=rgba_se>>8;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=rgba_se;
  if (ps_video_draw_line_strip(vtxv,3)<0) return -1;

  return 0;
}

static int ps_button_draw_border(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  uint8_t r=widget->fgrgba>>24;
  uint8_t g=widget->fgrgba>>16;
  uint8_t b=widget->fgrgba>>8;
  uint8_t a=widget->fgrgba;
  
  if (WIDGET->hover) {
    if (b>=0x80) b=0xff;
    else b+=0x80;
  }

  struct akgl_vtx_raw vtxv[5]={
    {parentx,parenty,r,g,b,a},
    {parentx+widget->w,parenty,r,g,b,a},
    {parentx+widget->w,parenty+widget->h,r,g,b,a},
    {parentx,parenty+widget->h,r,g,b,a},
    {parentx,parenty,r,g,b,a},
  };
  if (ps_video_draw_line_strip(vtxv,5)<0) return -1;
  return 0;
}

static int _ps_button_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (WIDGET->bevel_width>0) {
    if (ps_button_draw_bevel(widget,parentx,parenty)<0) return -1;
  }
  if (WIDGET->border_width>0) {
    if (ps_button_draw_border(widget,parentx,parenty)<0) return -1;
  }

  /* If we are pressed, cheat the children's positions down and right by 1 pixel. */
  if (WIDGET->pressed) {
    parentx+=1;
    parenty+=1;
  }
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  
  return 0;
}

/* Measure.
 */

static int _ps_button_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=0;
  *h=0;

  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
    if (chw>*w) *w=chw;
    (*h)+=chh;
  }
  
  (*w)+=WIDGET->border_width<<1;
  (*h)+=WIDGET->border_width<<1;
  (*w)+=WIDGET->bevel_width<<1;
  (*h)+=WIDGET->bevel_width<<1;
  return 0;
}

/* Pack.
 */

static int _ps_button_pack(struct ps_widget *widget) {
  int y=WIDGET->border_width+WIDGET->bevel_width;
  int x=y;
  int w=widget->w-(WIDGET->border_width<<1)-(WIDGET->bevel_width<<1);
  int h=widget->h-WIDGET->border_width-WIDGET->bevel_width;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,w,h-y)<0) return -1;
    child->x=x;
    child->y=y;
    child->w=w;
    child->h=chh;
    y+=chh;
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_button_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_button_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    if (value) {
      WIDGET->track=1;
      WIDGET->pressed=1;
    } else {
      WIDGET->track=0;
      WIDGET->pressed=0;
    }
  }
  return 0;
}

static int _ps_button_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_button_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_button_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_button_mouseenter(struct ps_widget *widget) {
  WIDGET->hover=1;
  if (WIDGET->track) WIDGET->pressed=1;
  return 0;
}

static int _ps_button_mouseexit(struct ps_widget *widget) {
  WIDGET->hover=0;
  WIDGET->pressed=0;
  return 0;
}

static int _ps_button_activate(struct ps_widget *widget) {
  return ps_callback_call(&WIDGET->cb,widget);
}

static int _ps_button_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_button_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_button_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_button_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_button={

  .name="button",
  .objlen=sizeof(struct ps_widget_button),

  .del=_ps_button_del,
  .init=_ps_button_init,

  .draw=_ps_button_draw,
  .measure=_ps_button_measure,
  .pack=_ps_button_pack,

  .mousemotion=_ps_button_mousemotion,
  .mousebutton=_ps_button_mousebutton,
  .mousewheel=_ps_button_mousewheel,
  .key=_ps_button_key,
  .userinput=_ps_button_userinput,

  .mouseenter=_ps_button_mouseenter,
  .mouseexit=_ps_button_mouseexit,
  .activate=_ps_button_activate,
  .cancel=_ps_button_cancel,
  .adjust=_ps_button_adjust,
  .focus=_ps_button_focus,
  .unfocus=_ps_button_unfocus,

};

/* Children by type.
 */

static struct ps_widget *ps_button_require_icon(struct ps_widget *widget) {
  if ((widget->childc>=1)&&(widget->childv[0]->type==&ps_widget_type_icon)) return widget->childv[0];
  struct ps_widget *icon=ps_widget_new(&ps_widget_type_icon);
  if (!icon) return 0;
  if (ps_widget_insert_child(widget,0,icon)<0) {
    ps_widget_del(icon);
    return 0;
  }
  ps_widget_del(icon);
  return icon;
}

static struct ps_widget *ps_button_require_label(struct ps_widget *widget) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (child->type==&ps_widget_type_label) return child;
  }
  struct ps_widget *label=ps_widget_new(&ps_widget_type_label);
  if (!label) return 0;
  if (ps_widget_add_child(widget,label)<0) {
    ps_widget_del(label);
    return 0;
  }
  ps_widget_del(label);
  return label;
}

/* Accessors.
 */
 
int ps_widget_button_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

int ps_widget_button_set_margins(struct ps_widget *widget,int bevel_width,int border_width) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return -1;
  if ((bevel_width<0)||(border_width<0)) return -1;
  WIDGET->bevel_width=bevel_width;
  WIDGET->border_width=border_width;
  return 0;
}

int ps_widget_button_set_icon(struct ps_widget *widget,uint16_t tileid) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return -1;
  struct ps_widget *icon=ps_button_require_icon(widget);
  if (ps_widget_icon_set_tile(icon,tileid)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

int ps_widget_button_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return -1;
  struct ps_widget *label=ps_button_require_label(widget);
  if (ps_widget_label_set_text(label,src,srcc)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

const char *ps_widget_button_get_text(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return 0;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (child->type==&ps_widget_type_label) {
      return ps_widget_label_get_text(child);
    }
  }
  return 0;
}

/* Convenience ctor.
 */
 
struct ps_widget *ps_widget_button_spawn(
  struct ps_widget *parent,
  uint16_t tileid,
  const char *text,int textc,
  struct ps_callback cb
) {
  struct ps_widget *widget=ps_widget_spawn(parent,&ps_widget_type_button);
  if (!widget) return 0;
  
  if (tileid) {
    struct ps_widget *child=ps_widget_spawn(widget,&ps_widget_type_icon);
    if (!child) return 0;
    if (ps_widget_icon_set_tile(child,tileid)<0) return 0;
  }

  if (text) {
    if (textc<0) { textc=0; while (text[textc]) textc++; }
    if (textc) {
      struct ps_widget *child=ps_widget_spawn(widget,&ps_widget_type_label);
      if (!child) return 0;
      if (ps_widget_label_set_text(child,text,textc)<0) return 0;
    }
  }

  WIDGET->cb=cb;

  return widget;
}

/* Set text color.
 */
 
int ps_widget_button_set_text_color(struct ps_widget *widget,uint32_t rgba) {
  if (!widget||(widget->type!=&ps_widget_type_button)) return -1;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (child->type==&ps_widget_type_label) {
      child->fgrgba=rgba;
    }
  }
  return 0;
}
