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
#include "ps_corewidgets.h"
#include "video/ps_video.h"

static struct ps_widget *ps_root_get_active_child(const struct ps_widget *widget);
static int ps_root_set_track_hover(struct ps_widget *widget,struct ps_widget *track);
static int ps_root_set_track_click(struct ps_widget *widget,struct ps_widget *track);
static int ps_root_set_track_keyboard(struct ps_widget *widget,struct ps_widget *track);
static struct ps_widget *ps_root_find_hover_widget(const struct ps_widget *widget,int x,int y);
static int ps_root_doublecheck_keyboard_focus(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_root {
  struct ps_widget hdr;
  struct ps_gui *gui; // WEAK
  struct ps_widget *track_hover;
  struct ps_widget *track_click;
  struct ps_widget *track_keyboard;
};

#define WIDGET ((struct ps_widget_root*)widget)

/* Delete.
 */

static void _ps_root_del(struct ps_widget *widget) {
  ps_widget_del(WIDGET->track_hover);
  ps_widget_del(WIDGET->track_click);
  ps_widget_del(WIDGET->track_keyboard);
}

/* Initialize.
 */

static int _ps_root_init(struct ps_widget *widget) {

  widget->bgrgba=0x400000ff;
  widget->fgrgba=0x000000c0;

  return 0;
}

/* Draw.
 */

static int _ps_root_draw(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;

  /* Background only? */
  if (widget->childc<1) {
    if (ps_video_draw_rect(parentx,parenty,widget->w,widget->h,widget->bgrgba)<0) return -1;

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

/* Mouse motion.
 */

static int _ps_root_mousemotion(struct ps_widget *widget,int x,int y) {

  /* If we are tracking a click, events go exclusively to that widget. */
  if (WIDGET->track_click) {
    if (ps_widget_contains_screen_point(WIDGET->track_click,x,y)) {
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
 */

static int _ps_root_mousebutton(struct ps_widget *widget,int btnid,int value) {

  /* Any button other than the principal, forward it directly to the hover widget. */
  if (btnid!=1) {
    if (!WIDGET->track_hover) return 0;
    if (ps_widget_mousebutton(WIDGET->track_hover,btnid,value)<0) return -1;
    return 0;
  }

  /* Releasing the principal button either activates or untracks the click-track widget. */
  if (!value) {
    if (!WIDGET->track_click) return 0;
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
    if (ps_widget_mousebutton(WIDGET->track_hover,btnid,value)<0) return -1;
    if (ps_root_set_track_click(widget,WIDGET->track_hover)<0) return -1;
    return 0;
  }

  return 0;
}

static int _ps_root_mousewheel(struct ps_widget *widget,int dx,int dy) {
  if (WIDGET->track_hover) {
    if (ps_widget_mousewheel(WIDGET->track_hover,dx,dy)<0) return -1;
  }
  return 0;
}

/* Keyboard.
 */

static int _ps_root_key(struct ps_widget *widget,int keycode,int codepoint,int value) {

  //TODO Tab key to change keyboard focus.

  /* If there is a keyboard focus, give it the event exclusively. */
  if (WIDGET->track_keyboard) {
    if (ps_widget_key(WIDGET->track_keyboard,keycode,codepoint,value)<0) return -1;
    return 0;
  }

  //TODO General keyboard events like activating buttons or cancelling dialogues.

  return 0;
}

/* Joystick.
 */

static int _ps_root_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  //TODO user input in GUI
  return 0;
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

static int ps_root_set_track_keyboard(struct ps_widget *widget,struct ps_widget *track) {
  if (track&&(ps_widget_ref(track)<0)) return -1;
  ps_widget_del(WIDGET->track_keyboard);
  WIDGET->track_keyboard=track;
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

/* Find a widget under the given one that accepts keyboard focus.
 */

static struct ps_widget *ps_root_find_keyboard_widget(struct ps_widget *widget) {
  if (widget->accept_keyboard_focus) return widget;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    struct ps_widget *found=ps_root_find_keyboard_widget(child);
    if (found) return found;
  }
  return 0;
}

/* Double-check keyboard focus.
 * We do this at pack, since that happens whenever a new modal is pushed.
 * By design, it should do no harm to call this redundantly.
 */
 
static int ps_root_doublecheck_keyboard_focus(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return -1;
  struct ps_widget *active=ps_root_get_active_child(widget);

  /* If we have a focus, ensure that it is part of the active modal. */
  if (WIDGET->track_keyboard) {
    if (ps_widget_is_ancestor(active,WIDGET->track_keyboard)) {
      return 0;
    }
    if (ps_widget_unfocus(WIDGET->track_keyboard)<0) return -1;
    if (ps_root_set_track_keyboard(widget,0)<0) return -1;
  }

  /* Find a focusable widget in the active modal. */
  if (!active) return 0;
  struct ps_widget *focus=ps_root_find_keyboard_widget(active);
  if (!focus) return 0;
  if (ps_widget_focus(focus)<0) return -1;
  if (ps_root_set_track_keyboard(widget,focus)<0) return -1;
  
  return 0;
}
