/* ps_widget_root.c
 * There is only ever one of these, and it is owned directly by the GUI object.
 * We maintain a stack of modal controller widgets.
 * We process raw input and deliver to the appropriate descendant widget.
 * bgrgba is the color when no widgets are loaded.
 * fgrgba is the blotter color, drawn just below the topmost child if more than one.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "../ps_gui.h"
#include "gui/ps_keyfocus.h"
#include "ps_corewidgets.h"
#include "video/ps_video.h"

static struct ps_widget *ps_root_get_active_child(const struct ps_widget *widget);
static int ps_root_set_track_hover(struct ps_widget *widget,struct ps_widget *track);
static int ps_root_set_track_click(struct ps_widget *widget,struct ps_widget *track);
static int ps_root_set_track_keyboard(struct ps_widget *widget,struct ps_widget *track);
static struct ps_widget *ps_root_find_hover_widget(const struct ps_widget *widget,int x,int y);
static struct ps_widget *ps_root_find_scroll_widget(const struct ps_widget *widget,int x,int y);
static int ps_root_doublecheck_keyboard_focus(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_root {
  struct ps_widget hdr;
  struct ps_gui *gui; // WEAK
  struct ps_widget *track_hover;
  struct ps_widget *track_click;
  int mousex,mousey;
  int dragdx,dragdy; // Position of cursor relative to track_click, if it is draggable.
  struct ps_keyfocus *keyfocus;
  int mod_shift;
};

#define WIDGET ((struct ps_widget_root*)widget)

/* Delete.
 */

static void _ps_root_del(struct ps_widget *widget) {
  ps_widget_del(WIDGET->track_hover);
  ps_widget_del(WIDGET->track_click);
  ps_keyfocus_del(WIDGET->keyfocus);
}

/* Initialize.
 */

static int _ps_root_init(struct ps_widget *widget) {

  widget->bgrgba=0x400000ff;
  widget->fgrgba=0x000000c0;

  if (!(WIDGET->keyfocus=ps_keyfocus_new())) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_root_draw(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;

  /* No children -- GUI is inactive and we must draw nothing. */
  if (widget->childc<1) {
    //if (ps_video_draw_rect(parentx,parenty,widget->w,widget->h,widget->bgrgba)<0) return -1;

  /* Single child only? */
  } else if (widget->childc==1) {
    if (ps_widget_draw(widget->childv[0],parentx,parenty)<0) return -1;

  /* Multiple children with blotter? */
  } else {
    int i=0; for (;i<widget->childc-1;i++) {
      if (ps_widget_draw(widget->childv[i],parentx,parenty)<0) return -1;
    }
    if (ps_video_draw_rect(parentx,parenty,widget->w,widget->h,widget->fgrgba)<0) return -1;
    if (ps_widget_draw(widget->childv[widget->childc-1],parentx,parenty)<0) return -1;
  }
  
  return 0;
}

/* Pack.
 * First child gets the whole screen, whether he wants it or not.
 * Subsequent children get their desired bounds, centered.
 */

static int _ps_root_pack(struct ps_widget *widget) {

  if (ps_root_doublecheck_keyboard_focus(widget)<0) return -1;

  if (widget->childc<1) return 0;
  
  struct ps_widget *child=widget->childv[0];
  child->x=0;
  child->y=0;
  child->w=widget->w;
  child->h=widget->h;
  if (ps_widget_pack(child)<0) return -1;

  int i=1; for (;i<widget->childc;i++) {
    child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    child->x=(widget->w>>1)-(chw>>1);
    child->y=(widget->h>>1)-(chh>>1);
    child->w=chw;
    child->h=chh;
    if (ps_widget_pack(child)<0) return -1;
  }

  return 0;
}

/* Update.
 */

static int _ps_root_update(struct ps_widget *widget) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (active) return ps_widget_update(active);
  return 0;
}

/* Drag the tracking widget.
 */

static int ps_root_drag_widget(struct ps_widget *widget,struct ps_widget *pumpkin,int x,int y) {
  int relx,rely;
  if (ps_widget_coords_from_screen(&relx,&rely,pumpkin->parent,x,y)<0) return -1;
  pumpkin->x=relx-WIDGET->dragdx;
  pumpkin->y=rely-WIDGET->dragdy;
  return 0;
}

/* Mouse motion.
 */

static int _ps_root_mousemotion(struct ps_widget *widget,int x,int y) {

  WIDGET->mousex=x;
  WIDGET->mousey=y;

  /* If we are tracking a click, events go exclusively to that widget. */
  if (WIDGET->track_click) {
  
    if (WIDGET->track_click->draggable) {
      if (ps_root_drag_widget(widget,WIDGET->track_click,x,y)<0) return -1;

    } else if (WIDGET->track_click->drag_verbatim) {
      if (ps_widget_coords_from_screen(&x,&y,WIDGET->track_click,x,y)<0) return -1;
      if (ps_widget_mousemotion(WIDGET->track_click,x,y)<0) return -1;

    } else if (ps_widget_contains_screen_point(WIDGET->track_click,x,y)) {
      if (WIDGET->track_click!=WIDGET->track_hover) {
        if (ps_root_set_track_hover(widget,WIDGET->track_click)<0) return -1;
        if (ps_widget_mouseenter(WIDGET->track_click)<0) return -1;
      }

    } else {
      if (WIDGET->track_hover) {
        if (ps_widget_mouseexit(WIDGET->track_hover)<0) return -1;
        if (ps_root_set_track_hover(widget,0)<0) return -1;
      }
    }
    return 0;
  }

  /* When not tracking a click, set track_hover to the topmost trackable widget directly under the pointer. */
  struct ps_widget *nhover=ps_root_find_hover_widget(widget,x,y);

  /* No change in the hover widget. Report motion to it. */
  if (nhover&&(nhover==WIDGET->track_hover)) {
    if (ps_widget_coords_from_screen(&x,&y,WIDGET->track_hover,x,y)<0) return -1;
    if (ps_widget_mousemotion(WIDGET->track_hover,x,y)<0) return -1;
    return 0;
  }

  /* If we had a hover widget before, drop it. */
  if (WIDGET->track_hover) {
    if (ps_widget_mouseexit(WIDGET->track_hover)<0) return -1;
    if (ps_root_set_track_hover(widget,0)<0) return -1;
  }

  /* If we're acquiring a new hover widget, fire mouseenter. */
  if (nhover) {
    if (ps_widget_mouseenter(nhover)<0) return -1;
    if (ps_root_set_track_hover(widget,nhover)<0) return -1;
  }

  return 0;
}

/* Mouse button.
 * Check aggressively for orphaned focus widgets. 
 * Might not be perfect, but we can avoid abvious errors.
 */

static int _ps_root_mousebutton(struct ps_widget *widget,int btnid,int value) {

  /* Remove stale track_hover and recheck if unset.
   */
  if (WIDGET->track_hover&&!ps_widget_is_ancestor(widget,WIDGET->track_hover)) {
    if (ps_root_set_track_hover(widget,0)<0) return -1;
  }
  if (!WIDGET->track_hover) {
    if (_ps_root_mousemotion(widget,WIDGET->mousex,WIDGET->mousey)<0) return -1;
  }

  /* Any button other than the principal, forward it directly to the hover widget. */
  if (btnid!=1) {
    if (!WIDGET->track_hover) return 0;
    if (ps_widget_mousebutton(WIDGET->track_hover,btnid,value)<0) return -1;
    return 0;
  }

  /* Releasing the principal button either activates or untracks the click-track widget. */
  if (!value) {
    if (!WIDGET->track_click) return 0;
    if (!ps_widget_is_ancestor(widget,WIDGET->track_click)) {
      if (ps_root_set_track_click(widget,0)<0) return -1;
      return 0;
    }
    if (WIDGET->track_click==WIDGET->track_hover) {
      if (ps_widget_activate(WIDGET->track_click)<0) return -1;
    } else {
      // We don't have an event for "not tracking anymore".
    }
    if (ps_widget_mousebutton(WIDGET->track_click,btnid,value)<0) return -1;
    if (ps_root_set_track_click(widget,0)<0) return -1;
    return 0;
  }

  /* Pressing the principal button begins tracking if we are hovering on something. */
  if (WIDGET->track_hover) {
    if (!ps_widget_is_ancestor(widget,WIDGET->track_hover)) {
      if (ps_root_set_track_hover(widget,0)<0) return -1;
    } else {
      if (ps_widget_mousebutton(WIDGET->track_hover,btnid,value)<0) return -1;
      if (ps_root_set_track_click(widget,WIDGET->track_hover)<0) return -1;
      if (ps_widget_coords_from_screen(&WIDGET->dragdx,&WIDGET->dragdy,WIDGET->track_hover,WIDGET->mousex,WIDGET->mousey)<0) return -1;
    }
    return 0;
  }

  return 0;
}

/* Mouse wheel.
 * Originally, these were delivered to the hover focus, but that is causing problems.
 * I think it makes more sense to separate "clickable" and "scrollable" focus.
 */

static int _ps_root_mousewheel(struct ps_widget *widget,int dx,int dy) {
  
  struct ps_widget *target=ps_root_find_scroll_widget(widget,WIDGET->mousex,WIDGET->mousey);
  if (!target) return 0;

  if (ps_widget_mousewheel(target,dx,dy)<0) return -1;

  /* Generate a dummy motion event in case the hover widget moved beneath us. */
  if (_ps_root_mousemotion(widget,WIDGET->mousex,WIDGET->mousey)<0) return -1;
  
  return 0;
}

/* Keyboard.
 */

static int _ps_root_key(struct ps_widget *widget,int keycode,int codepoint,int value) {

  /* Track shift key, but don't consume the event. */
  if (keycode==0x000700e1) {
    if (value) WIDGET->mod_shift=1;
    else WIDGET->mod_shift=0;
  }

  /* Tab, Shift-Tab: Move keyboard focus. */
  if (codepoint==0x09) {
    if (value) {
      if (WIDGET->mod_shift) {
        if (ps_keyfocus_retreat(WIDGET->keyfocus)<0) return -1;
      } else {
        if (ps_keyfocus_advance(WIDGET->keyfocus)<0) return -1;
      }
    }
    return 0;
  }

  /* Enter: Activate topmost child. */
  if ((codepoint==0x0a)||(codepoint==0x0d)) {
    if (value) {
      struct ps_widget *active=ps_root_get_active_child(widget);
      if (active) {
        if (ps_widget_activate(active)<0) return -1;
      }
    }
    return 0;
  }

  /* Escape: Cancel topmost child. */
  if (codepoint==0x1b) {
    if (value) {
      struct ps_widget *active=ps_root_get_active_child(widget);
      if (active) {
        if (ps_widget_cancel(active)<0) return -1;
      }
    }
    return 0;
  }

  /* If there is a keyboard focus, give it the event exclusively. */
  struct ps_widget *focus=ps_keyfocus_get_focus(WIDGET->keyfocus);
  if (focus) {
    if (ps_widget_key(focus,keycode,codepoint,value)<0) return -1;
  }

  return 0;
}

/* Joystick.
 */

static int _ps_root_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (!active) return 0;
  return ps_widget_userinput(active,plrid,btnid,value);
}

/* Digested input events.
 */

static int _ps_root_activate(struct ps_widget *widget) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (active) return ps_widget_activate(active);
  return 0;
}

static int _ps_root_cancel(struct ps_widget *widget) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (active) return ps_widget_cancel(active);
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_root={

  .name="root",
  .objlen=sizeof(struct ps_widget_root),

  .del=_ps_root_del,
  .init=_ps_root_init,

  .draw=_ps_root_draw,
  .pack=_ps_root_pack,

  .mousemotion=_ps_root_mousemotion,
  .mousebutton=_ps_root_mousebutton,
  .mousewheel=_ps_root_mousewheel,
  .key=_ps_root_key,
  .userinput=_ps_root_userinput,

  .activate=_ps_root_activate,
  .cancel=_ps_root_cancel,

};

/* Trivial accessors.
 */
 
struct ps_gui *ps_widget_root_get_gui(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return 0;
  return WIDGET->gui;
}

int ps_widget_root_set_gui(struct ps_widget *widget,struct ps_gui *gui) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return -1;
  WIDGET->gui=gui;
  return 0;
}

static struct ps_widget *ps_root_get_active_child(const struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  return widget->childv[widget->childc-1];
}

/* Set tracked widgets.
 */
 
static int ps_root_set_track_hover(struct ps_widget *widget,struct ps_widget *track) {
  if (track&&(ps_widget_ref(track)<0)) return -1;
  ps_widget_del(WIDGET->track_hover);
  WIDGET->track_hover=track;
  return 0;
}

static int ps_root_set_track_click(struct ps_widget *widget,struct ps_widget *track) {
  if (track&&(ps_widget_ref(track)<0)) return -1;
  ps_widget_del(WIDGET->track_click);
  WIDGET->track_click=track;
  return 0;
}

/* Find the hoverable widget under a given point.
 */

static struct ps_widget *ps_root_find_hover_widget_1(struct ps_widget *widget,int x,int y) {

  /* Adjust coordinates and abort if oob. */
  x-=widget->x;
  y-=widget->y;
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=widget->w) return 0;
  if (y>=widget->h) return 0;

  /* If any of my children return a result, use it. From end of list to beginning. */
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    struct ps_widget *found=ps_root_find_hover_widget_1(child,x,y);
    if (found) return found;
  }

  /* If I accept hover events, return me. */
  if (widget->accept_mouse_focus) return widget;

  return 0;
}
 
static struct ps_widget *ps_root_find_hover_widget(const struct ps_widget *widget,int x,int y) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (!active) return 0;
  return ps_root_find_hover_widget_1(active,x,y);
}

/* Find the scrollable widget under a given point.
 */

static struct ps_widget *ps_root_find_scroll_widget_1(struct ps_widget *widget,int x,int y) {

  /* Adjust coordinates and abort if oob. */
  x-=widget->x;
  y-=widget->y;
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=widget->w) return 0;
  if (y>=widget->h) return 0;

  /* If any of my children return a result, use it. From end of list to beginning. */
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    struct ps_widget *found=ps_root_find_scroll_widget_1(child,x,y);
    if (found) return found;
  }

  /* If I accept scroll events, return me. */
  if (widget->accept_mouse_wheel) return widget;

  return 0;
}
 
static struct ps_widget *ps_root_find_scroll_widget(const struct ps_widget *widget,int x,int y) {
  struct ps_widget *active=ps_root_get_active_child(widget);
  if (!active) return 0;
  return ps_root_find_scroll_widget_1(active,x,y);
}

/* Double-check keyboard focus.
 * We do this at pack, since that happens whenever a new modal is pushed.
 * By design, it should do no harm to call this redundantly.
 */
 
static int ps_root_doublecheck_keyboard_focus(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return -1;
  if (ps_keyfocus_refresh(WIDGET->keyfocus,ps_root_get_active_child(widget))<0) return -1;
  return 0;
}
