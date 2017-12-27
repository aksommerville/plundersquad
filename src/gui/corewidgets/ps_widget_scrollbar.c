/* ps_widget_scrollbar.c
 * General-purpose scrollbar.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"
#include "util/ps_geometry.h"

#define PS_SCROLLBAR_WIDTH 6
#define PS_SCROLLBAR_HIGHLIGHT_COLOR 0xffff0040

/* Object definition.
 */

struct ps_widget_scrollbar {
  struct ps_widget hdr;
  int orientation;
  int value;
  int limit;
  int visible_size;
  struct ps_callback cb;
  int mousestate; // 0=none, 1=pageup, 2=thumb, 3=pagedown
  int mousex,mousey;
};

#define WIDGET ((struct ps_widget_scrollbar*)widget)

/* Delete.
 */

static void _ps_scrollbar_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_scrollbar_init(struct ps_widget *widget) {

  widget->bgrgba=0x808080ff;
  widget->fgrgba=0xc0c0c0ff;
  widget->accept_mouse_focus=1;
  widget->accept_mouse_wheel=1;

  WIDGET->orientation=PS_AXIS_VERT;
  WIDGET->mousex=-1;
  WIDGET->mousey=-1;

  return 0;
}

/* Translate between logical and visible coordinates.
 */

static int ps_scrollbar_calculate_thumb(int *p,int *c,const struct ps_widget *widget) {

  /* How much space do we have to work with? */
  int limit=0;
  switch (WIDGET->orientation) {
    case PS_AXIS_HORZ: limit=widget->w; break;
    case PS_AXIS_VERT: limit=widget->h; break;
  }
  if (limit<1) {
    *p=*c=0;
    return 0; 
  }

  /* If logical size is invalid, the whole thing is covered. */
  if (WIDGET->limit<1) {
    *p=0;
    *c=limit;
    return 0;
  }

  /* Calculate thumb size first.
   * This is proportionate to the visible size, but never less than square.
   * Cover all if we are too small.
   */
  *c=(WIDGET->visible_size*limit)/WIDGET->limit;
  if (*c<PS_SCROLLBAR_WIDTH) *c=PS_SCROLLBAR_WIDTH;
  if (*c>=limit) {
    *p=0;
    *c=limit;
    return 0;
  }

  /* Calculate thumb position.
   * Clamp to visible range.
   */
  *p=(WIDGET->value*limit)/WIDGET->limit;
  if (*p<0) *p=0;
  else if (*p+*c>limit) *p=limit-(*c);

  return 0;
}

/* General properties.
 */

static int _ps_scrollbar_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_scrollbar_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_scrollbar_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int _ps_scrollbar_draw(struct ps_widget *widget,int parentx,int parenty) {
  int thumbp,thumbc;
  if (ps_scrollbar_calculate_thumb(&thumbp,&thumbc,widget)<0) return -1;
  parentx+=widget->x;
  parenty+=widget->y;
  switch (WIDGET->orientation) {
  
    case PS_AXIS_HORZ: {
        if (ps_video_draw_rect(parentx,parenty,thumbp,widget->h,widget->bgrgba)<0) return -1;
        if (ps_video_draw_rect(parentx+thumbp,parenty,thumbc,widget->h,widget->fgrgba)<0) return -1;
        if (ps_video_draw_rect(parentx+thumbp+thumbc,parenty,widget->w-thumbc-thumbp,widget->h,widget->bgrgba)<0) return -1;
        switch (WIDGET->mousestate) {
          case 1: if (ps_video_draw_rect(parentx,parenty,thumbp,widget->h,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
          case 2: if (ps_video_draw_rect(parentx+thumbp,parenty,thumbc,widget->h,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
          case 3: if (ps_video_draw_rect(parentx+thumbp+thumbc,parenty,widget->w-thumbc-thumbp,widget->h,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
        }
      } break;
      
    case PS_AXIS_VERT: {
        if (ps_video_draw_rect(parentx,parenty,widget->w,thumbp,widget->bgrgba)<0) return -1;
        if (ps_video_draw_rect(parentx,parenty+thumbp,widget->w,thumbc,widget->fgrgba)<0) return -1;
        if (ps_video_draw_rect(parentx,parenty+thumbp+thumbc,widget->w,widget->h-thumbc-thumbp,widget->bgrgba)<0) return -1;
        switch (WIDGET->mousestate) {
          case 1: if (ps_video_draw_rect(parentx,parenty,widget->w,thumbp,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
          case 2: if (ps_video_draw_rect(parentx,parenty+thumbp,widget->w,thumbc,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
          case 3: if (ps_video_draw_rect(parentx,parenty+thumbp+thumbc,widget->w,widget->h-thumbc-thumbp,PS_SCROLLBAR_HIGHLIGHT_COLOR)<0) return -1; break;
        }
      } break;
      
  }
  return 0;
}

/* Measure.
 */

static int _ps_scrollbar_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  switch (WIDGET->orientation) {
    case PS_AXIS_HORZ: *h=PS_SCROLLBAR_WIDTH; break;
    case PS_AXIS_VERT: *w=PS_SCROLLBAR_WIDTH; break;
  }
  return 0;
}

/* Given a point in me, what part is it?
 */

static int ps_scrollbar_mousestate_for_position(const struct ps_widget *widget,int x,int y) {
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=widget->w) return 0;
  if (y>=widget->h) return 0;
  int thumbp,thumbc;
  if (ps_scrollbar_calculate_thumb(&thumbp,&thumbc,widget)<0) return 0;
  switch (WIDGET->orientation) {
    case PS_AXIS_HORZ: {
        if (x<thumbp) return 1;
        if (x<thumbp+thumbc) return 2;
        return 3;
      }
    case PS_AXIS_VERT: {
        if (y<thumbp) return 1;
        if (y<thumbp+thumbc) return 2;
        return 3;
      }
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_scrollbar_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  WIDGET->mousestate=ps_scrollbar_mousestate_for_position(widget,x,y);
  return 0;
}

static int _ps_scrollbar_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_scrollbar_mousewheel(struct ps_widget *widget,int dx,int dy) {

  int d=0;
  switch (WIDGET->orientation) {
    case PS_AXIS_HORZ: d=dx; break;
    case PS_AXIS_VERT: d=dy; break;
  }
  if (!d) return 0;

  if (ps_widget_scrollbar_adjust(widget,d)<0) return -1;
  
  return 0;
}

static int _ps_scrollbar_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_scrollbar_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_scrollbar_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrollbar_mouseexit(struct ps_widget *widget) {
  WIDGET->mousestate=0;
  WIDGET->mousex=-1;
  WIDGET->mousey=-1;
  return 0;
}

static int _ps_scrollbar_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrollbar_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrollbar_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_scrollbar_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_scrollbar_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_scrollbar={

  .name="scrollbar",
  .objlen=sizeof(struct ps_widget_scrollbar),

  .del=_ps_scrollbar_del,
  .init=_ps_scrollbar_init,

  .get_property=_ps_scrollbar_get_property,
  .set_property=_ps_scrollbar_set_property,
  .get_property_type=_ps_scrollbar_get_property_type,

  .draw=_ps_scrollbar_draw,
  .measure=_ps_scrollbar_measure,

  .mousemotion=_ps_scrollbar_mousemotion,
  .mousebutton=_ps_scrollbar_mousebutton,
  .mousewheel=_ps_scrollbar_mousewheel,
  .key=_ps_scrollbar_key,
  .userinput=_ps_scrollbar_userinput,

  .mouseenter=_ps_scrollbar_mouseenter,
  .mouseexit=_ps_scrollbar_mouseexit,
  .activate=_ps_scrollbar_activate,
  .cancel=_ps_scrollbar_cancel,
  .adjust=_ps_scrollbar_adjust,
  .focus=_ps_scrollbar_focus,
  .unfocus=_ps_scrollbar_unfocus,

};

/* Accessors.
 */
 
int ps_widget_scrollbar_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}
 
int ps_widget_scrollbar_set_orientation(struct ps_widget *widget,int axis) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  switch (axis) {
    case PS_AXIS_HORZ:
    case PS_AXIS_VERT:
      break;
    default: return -1;
  }
  WIDGET->orientation=axis;
  return 0;
}

int ps_widget_scrollbar_set_value(struct ps_widget *widget,int value) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  if (value>WIDGET->limit-WIDGET->visible_size) value=WIDGET->limit-WIDGET->visible_size;
  if (value<0) value=0;
  WIDGET->value=value;
  return 0;
}
  
int ps_widget_scrollbar_get_value(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  return WIDGET->value;
}

int ps_widget_scrollbar_set_limit(struct ps_widget *widget,int limit) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  if (limit<0) return -1;
  WIDGET->limit=limit;
  return 0;
}

int ps_widget_scrollbar_get_limit(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  return WIDGET->limit;
}

int ps_widget_scrollbar_set_visible_size(struct ps_widget *widget,int visible_size) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  if (visible_size<0) return -1;
  WIDGET->visible_size=visible_size;
  return 0;
}

int ps_widget_scrollbar_get_visible_size(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  return WIDGET->visible_size;
}

/* Adjust by direct value.
 */
 
int ps_widget_scrollbar_adjust(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_scrollbar)) return -1;
  int nv=WIDGET->value+d;
  if (nv>WIDGET->limit-WIDGET->visible_size) nv=WIDGET->limit-WIDGET->visible_size;
  if (nv<0) nv=0;
  if (nv==WIDGET->value) return 0;
  WIDGET->value=nv;
  WIDGET->mousestate=ps_scrollbar_mousestate_for_position(widget,WIDGET->mousex,WIDGET->mousey);
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  return 0;
}
