/* ps_widget_toggle.c
 * A button that stays pressed, for setting boolean properties.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_TOGGLE_MARGIN 5

/* Object definition.
 */

struct ps_widget_toggle {
  struct ps_widget hdr;
  int value;
  int enable;
  int hover;
  int track;
  struct ps_callback cb;
  uint32_t activergba;
};

#define WIDGET ((struct ps_widget_toggle*)widget)

/* Delete.
 */

static void _ps_toggle_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_toggle_init(struct ps_widget *widget) {

  widget->bgrgba=0x808080ff;
  widget->fgrgba=0x000000ff;
  WIDGET->activergba=0x507050ff;

  WIDGET->value=0;
  WIDGET->enable=1;

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

/* Draw frame.
 * I'll try to get slick about this and draw the border and bevel in a single line strip.
 */

static int ps_toggle_draw_frame(struct ps_widget *widget,int parentx,int parenty) {

  parentx+=widget->x;
  parenty+=widget->y;
  int x0=parentx,x1=parentx+1,x2=parentx+widget->w-1,x3=parentx+widget->w;
  int y0=parenty,y1=parenty+1,y2=parenty+widget->h-1,y3=parenty+widget->h;

  uint8_t or=widget->fgrgba>>24;
  uint8_t og=widget->fgrgba>>16;
  uint8_t ob=widget->fgrgba>>8;
  uint8_t oa=widget->fgrgba;
  uint32_t rgbanw,rgbase;
  if (WIDGET->value) {
    rgbanw=ps_rgba_darken(WIDGET->activergba);
    rgbase=ps_rgba_lighten(WIDGET->activergba);
  } else {
    rgbanw=ps_rgba_lighten(widget->bgrgba);
    rgbase=ps_rgba_darken(widget->bgrgba);
  }
  uint8_t nwr=rgbanw>>24;
  uint8_t nwg=rgbanw>>16;
  uint8_t nwb=rgbanw>>8;
  uint8_t nwa=rgbanw;
  uint8_t ser=rgbase>>24;
  uint8_t seg=rgbase>>16;
  uint8_t seb=rgbase>>8;
  uint8_t sea=rgbase;

  const int vtxc=11; // SWo, NWo, NEo, SEo, SWo2, SWi, NWi, NEi, NEi2, SEi, SWi
  struct akgl_vtx_raw vtxv[vtxc]={
    {x0,y3,or,og,ob,oa},
    {x0,y0,or,og,ob,oa},
    {x3,y0,or,og,ob,oa},
    {x3,y3,or,og,ob,oa},
    {x0,y3,or,og,ob,oa},
    {x1,y2,nwr,nwg,nwb,nwa},
    {x1,y1,nwr,nwg,nwb,nwa},
    {x2,y1,nwr,nwg,nwb,nwa},
    {x2,y1,ser,seg,seb,sea},
    {x2,y2,ser,seg,seb,sea},
    {x1,y2,ser,seg,seb,sea},
  };

  if (ps_video_draw_line_strip(vtxv,vtxc)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_toggle_draw(struct ps_widget *widget,int parentx,int parenty) {

  if (WIDGET->value) {
    if (ps_video_draw_rect(parentx+widget->x,parenty+widget->y,widget->w,widget->h,WIDGET->activergba)<0) return -1;
  } else {
    if (ps_video_draw_rect(parentx+widget->x,parenty+widget->y,widget->w,widget->h,widget->bgrgba)<0) return -1;
  }
  
  if (ps_toggle_draw_frame(widget,parentx,parenty)<0) return -1;

  if (WIDGET->value) {
    parentx+=1;
    parenty+=1;
  }
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  
  return 0;
}

/* Measure.
 */

static int _ps_toggle_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (widget->childc>=1) {
    if (ps_widget_measure(w,h,widget->childv[0],maxw,maxh)<0) return -1;
    (*w)+=PS_TOGGLE_MARGIN<<1;
    (*h)+=PS_TOGGLE_MARGIN<<1;
  } else {
    *w=PS_TOGGLE_MARGIN<<1;
    *h=PS_TOGGLE_MARGIN<<1;
  }
  return 0;
}

/* Pack.
 */

static int _ps_toggle_pack(struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  struct ps_widget *child=widget->childv[0];

  // If we are too small, let the child take my entire size.
  int margintolerance=PS_TOGGLE_MARGIN*3;
  if ((widget->w<margintolerance)||(widget->h<margintolerance)) {
    child->x=0;
    child->y=0;
    child->w=widget->w;
    child->h=widget->h;
  } else {
    child->x=PS_TOGGLE_MARGIN;
    child->y=PS_TOGGLE_MARGIN;
    child->w=widget->w-(PS_TOGGLE_MARGIN<<1);
    child->h=widget->h-(PS_TOGGLE_MARGIN<<1);
  }

  if (ps_widget_pack(child)<0) return -1;
  return 0;
}

/* Input events.
 */

static int _ps_toggle_activate(struct ps_widget *widget) {
  if (!WIDGET->enable) return 0;
  WIDGET->value=(WIDGET->value?0:1);
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  return 0;
}

static int _ps_toggle_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (!WIDGET->enable) return 0;
  if (btnid==1) {
    if (value) {
      WIDGET->track=1;
    } else {
      WIDGET->track=0;
    }
  }
  return 0;
}

static int _ps_toggle_mouseenter(struct ps_widget *widget) {
  if (!WIDGET->enable) return 0;
  WIDGET->hover=1;
  return 0;
}

static int _ps_toggle_mouseexit(struct ps_widget *widget) {
  if (!WIDGET->enable) return 0;
  WIDGET->hover=0;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_toggle={

  .name="toggle",
  .objlen=sizeof(struct ps_widget_toggle),

  .del=_ps_toggle_del,
  .init=_ps_toggle_init,

  .draw=_ps_toggle_draw,
  .measure=_ps_toggle_measure,
  .pack=_ps_toggle_pack,

  .mousebutton=_ps_toggle_mousebutton,
  .mouseenter=_ps_toggle_mouseenter,
  .mouseexit=_ps_toggle_mouseexit,
  .activate=_ps_toggle_activate,

};

/* Accessors.
 */
 
int ps_widget_toggle_set_value(struct ps_widget *widget,int value) {
  if (!widget||(widget->type!=&ps_widget_type_toggle)) return -1;
  WIDGET->value=(value?1:0);
  return 0;
}

int ps_widget_toggle_get_value(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_toggle)) return 0;
  return WIDGET->value;
}

int ps_widget_toggle_set_enable(struct ps_widget *widget,int enable) {
  if (!widget||(widget->type!=&ps_widget_type_toggle)) return -1;
  WIDGET->enable=(enable?1:0);
  if (!WIDGET->enable) {
    WIDGET->hover=0;
    WIDGET->track=0;
  }
  return 0;
}

int ps_widget_toggle_get_enable(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_toggle)) return 0;
  return WIDGET->enable;
}

int ps_widget_toggle_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_toggle)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

/* Setup child widget.
 */

static struct ps_widget *ps_toggle_require_child(struct ps_widget *widget,const struct ps_widget_type *type) {
  if (widget->childc>=1) {
    struct ps_widget *child=widget->childv[0];
    if (child->type==type) return child;
    ps_widget_remove_all_children(widget);
  }
  struct ps_widget *child=ps_widget_spawn(widget,type);
  return child;
}

struct ps_widget *ps_widget_toggle_set_text(struct ps_widget *widget,const char *src,int srcc) {
  struct ps_widget *label=ps_toggle_require_child(widget,&ps_widget_type_label);
  if (!label) return 0;
  if (ps_widget_label_set_text(label,src,srcc)<0) return 0;
  return label;
}

struct ps_widget *ps_widget_toggle_set_icon(struct ps_widget *widget,uint16_t tileid) {
  struct ps_widget *icon=ps_toggle_require_child(widget,&ps_widget_type_icon);
  if (!icon) return 0;
  if (ps_widget_icon_set_tile(icon,tileid)<0) return 0;
  return icon;
}
