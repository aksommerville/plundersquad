/* ps_widget_scrolllist.c
 * TODO OpenGL scissor? I'm assuming it will be possible. Must add to akgl.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_SCROLLLIST_SELECTION_COLOR 0xff8000ff

/* Object definition.
 */

struct ps_scrolllist_entry {
  int y; // Absolute position, irrespective of scroll.
  int w,h; // Preferred size.
};

struct ps_widget_scrolllist {
  struct ps_widget hdr;
  int contenth; // Total height of content, reset at measure.
  int pixelp; // Scroll position in imaginary pixels.
  int vischildp; // Index of first visible child.
  int vischildc; // Count of visible children.
  struct ps_scrolllist_entry *entryv; // Child preferred sizes, recalculated at each measure().
  int entryc,entrya;
  int selp; // Index of selected child or -1
  int mousex,mousey; // We unwisely manage the mouse ourselves.
  struct ps_callback cb_select;
};

#define WIDGET ((struct ps_widget_scrolllist*)widget)

/* Delete.
 */

static void _ps_scrolllist_del(struct ps_widget *widget) {
  if (WIDGET->entryv) free(WIDGET->entryv);
  ps_callback_cleanup(&WIDGET->cb_select);
}

/* Initialize.
 */

static int _ps_scrolllist_init(struct ps_widget *widget) {
  WIDGET->selp=-1;
  return 0;
}

/* General properties.
 */

static int _ps_scrolllist_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
    case PS_WIDGET_SCROLLLIST_PROPERTY_scroll: *v=WIDGET->pixelp; return 0;
  }
  return -1;
}

static int _ps_scrolllist_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
    case PS_WIDGET_SCROLLLIST_PROPERTY_scroll: return ps_widget_scrolllist_set_scroll_position(widget,v);
  }
  return -1;
}

static int _ps_scrolllist_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
    case PS_WIDGET_SCROLLLIST_PROPERTY_scroll: return PS_WIDGET_PROPERTY_TYPE_HOTINTEGER;
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int ps_scrolllist_draw_children(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (child->w<1) continue;
    if (WIDGET->cb_select.fn&&(i==WIDGET->selp)) {
      if (ps_video_draw_rect(parentx+child->x,parenty+child->y,child->w,child->h,PS_SCROLLLIST_SELECTION_COLOR)<0) return -1;
    }
    if (ps_widget_draw(child,parentx,parenty)<0) return -1;
  }
  return 0;
}

static int _ps_scrolllist_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  akgl_scissor(parentx+widget->x,parenty+widget->y,widget->w,widget->h);
  int err=ps_scrolllist_draw_children(widget,parentx,parenty);
  akgl_scissor_none();
  return err;
}

/* Measure.
 */

static int _ps_scrolllist_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=*h=0;

  if (WIDGET->entrya<widget->childc) {
    void *nv=realloc(WIDGET->entryv,sizeof(struct ps_scrolllist_entry)*widget->childc);
    if (!nv) return -1;
    WIDGET->entryv=nv;
    WIDGET->entrya=widget->childc;
  }
  WIDGET->entryc=0;

  int y=0,i=0;
  WIDGET->contenth=0;
  struct ps_scrolllist_entry *entry=WIDGET->entryv;
  for (;i<widget->childc;i++,entry++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
    entry->y=y;
    entry->w=chw;
    entry->h=chh;
    if (chw>*w) *w=chw;
    WIDGET->contenth+=chh;
    y+=chh;
  }

  WIDGET->entryc=widget->childc;
  *h=WIDGET->contenth;
  return 0;
}

/* Pack.
 */

static int _ps_scrolllist_pack(struct ps_widget *widget) {

  if (WIDGET->entryc!=widget->childc) {
    int w,h;
    if (_ps_scrolllist_measure(&w,&h,widget,widget->w,widget->h)<0) return -1;
  }

  int i=0; 
  const struct ps_scrolllist_entry *entry=WIDGET->entryv;
  for (;i<widget->childc;i++,entry++) {
    struct ps_widget *child=widget->childv[i];
    child->x=0;
    child->y=entry->y-WIDGET->pixelp;
    child->w=widget->w;
    child->h=entry->h;
    if ((child->y>=widget->h)||(child->y+child->h<=0)) {
      child->w=child->h=0;
    }
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Widget at point.
 */

static int ps_scrolllist_child_index_from_vertical(const struct ps_widget *widget,int y) {
  if (y<0) return -1;
  if (y>=widget->h) return -1;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (y<child->y) continue;
    if (y>=child->y+child->h) continue;
    return i;
  }
  return -1;
}

/* Set selection.
 */

static int ps_scrolllist_set_selection(struct ps_widget *widget,int childp) {
  if ((childp<0)||(childp>=widget->childc)) childp=-1;
  if (WIDGET->selp==childp) return 0;
  if (!WIDGET->cb_select.fn) return -1;

  WIDGET->selp=childp;

  if (ps_callback_call(&WIDGET->cb_select,widget)<0) return -1;
  
  return 0;
}

/* Primitive input events.
 */

static int _ps_scrolllist_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  return 0;
}

static int _ps_scrolllist_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (WIDGET->cb_select.fn) {
    if ((btnid==1)&&(value==1)) {
      int nselp=ps_scrolllist_child_index_from_vertical(widget,WIDGET->mousey);
      if (ps_scrolllist_set_selection(widget,nselp)<0) return -1;
    }
  }
  return 0;
}

static int _ps_scrolllist_mousewheel(struct ps_widget *widget,int dx,int dy) {
  if (ps_widget_scrolllist_adjust(widget,dy)<0) return -1;
  return 0;
}

static int _ps_scrolllist_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_scrolllist_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_scrolllist_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrolllist_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrolllist_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrolllist_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrolllist_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_scrolllist_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrolllist_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_scrolllist={

  .name="scrolllist",
  .objlen=sizeof(struct ps_widget_scrolllist),

  .del=_ps_scrolllist_del,
  .init=_ps_scrolllist_init,

  .get_property=_ps_scrolllist_get_property,
  .set_property=_ps_scrolllist_set_property,
  .get_property_type=_ps_scrolllist_get_property_type,

  .draw=_ps_scrolllist_draw,
  .measure=_ps_scrolllist_measure,
  .pack=_ps_scrolllist_pack,

  .mousemotion=_ps_scrolllist_mousemotion,
  .mousebutton=_ps_scrolllist_mousebutton,
  .mousewheel=_ps_scrolllist_mousewheel,
  .key=_ps_scrolllist_key,
  .userinput=_ps_scrolllist_userinput,

  .mouseenter=_ps_scrolllist_mouseenter,
  .mouseexit=_ps_scrolllist_mouseexit,
  .activate=_ps_scrolllist_activate,
  .cancel=_ps_scrolllist_cancel,
  .adjust=_ps_scrolllist_adjust,
  .focus=_ps_scrolllist_focus,
  .unfocus=_ps_scrolllist_unfocus,

};

/* Scroll position.
 */
 
int ps_widget_scrolllist_get_scroll_position(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  return WIDGET->pixelp;
}

int ps_widget_scrolllist_set_scroll_position(struct ps_widget *widget,int p) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  int limit=WIDGET->contenth-widget->h;
  if (p>limit) p=limit;
  if (p<0) p=0;
  if (p==WIDGET->pixelp) return 0;
  
  WIDGET->pixelp=p;

  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

int ps_widget_scrolllist_adjust(struct ps_widget *widget,int dy) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  return ps_widget_scrolllist_set_scroll_position(widget,WIDGET->pixelp+dy);
}

/* Add label.
 */
 
struct ps_widget *ps_widget_scrolllist_add_label(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return 0;
  struct ps_widget *label=ps_widget_spawn(widget,&ps_widget_type_label);
  if (!label) return 0;
  if (ps_widget_label_set_text(label,src,srcc)<0) return 0;
  return label;
}

/* Enable selection.
 */
 
int ps_widget_scrolllist_enable_selection(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  ps_callback_cleanup(&WIDGET->cb_select);
  WIDGET->cb_select=cb;
  if (!cb.fn) {
    WIDGET->selp=-1;
  }
  return 0;
}

/* Get selection.
 */

int ps_widget_scrolllist_get_selection(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  return WIDGET->selp;
}

int ps_widget_scrolllist_set_selection(struct ps_widget *widget,int p) {
  if (!widget||(widget->type!=&ps_widget_type_scrolllist)) return -1;
  return ps_scrolllist_set_selection(widget,p);
}
