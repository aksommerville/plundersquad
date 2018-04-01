/* ps_widget_menu.c
 * Children:
 *  [0] thumb
 *  [1] packer
 */

#include "ps.h"
#include "../ps_widget.h"
#include "../ps_gui.h"
#include "ps_corewidgets.h"
#include "util/ps_geometry.h"
#include "game/ps_sound_effects.h"
#include "input/ps_input_button.h"

#define PS_MENU_ANIMATE_TIME 10

static int ps_menu_set_thumb_position(struct ps_widget *widget,int animate);

/* Object definition.
 */

struct ps_widget_menu {
  struct ps_widget hdr;
  int optionp;
  struct ps_callback cb;
  int mousex,mousey;
};

#define WIDGET ((struct ps_widget_menu*)widget)

/* Delete.
 */

static void _ps_menu_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_menu_init(struct ps_widget *widget) {

  struct ps_widget *thumb=ps_widget_spawn(widget,&ps_widget_type_blotter);
  if (!thumb) return -1;
  thumb->bgrgba=0xffff00ff;

  struct ps_widget *packer=ps_widget_spawn(widget,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_set_axis(packer,PS_AXIS_VERT)<0) return -1;
  if (ps_widget_packer_set_alignment(packer,PS_ALIGN_CENTER,PS_ALIGN_FILL)<0) return -1;

  return 0;
}

/* Measure, pack: Mostly defer to the packer.
 */

static int _ps_menu_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (widget->childc!=2) return -1;
  return ps_widget_measure(w,h,widget->childv[1],maxw,maxh);
}

static int _ps_menu_pack(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;
  
  struct ps_widget *packer=widget->childv[1];
  packer->x=0;
  packer->y=0;
  packer->w=widget->w;
  packer->h=widget->h;

  if (ps_widget_pack(packer)<0) return -1;
  if (ps_menu_set_thumb_position(widget,0)<0) return -1;
  return 0;
}

/* Get option index from vertical position.
 */

static int ps_menu_get_index_by_vertical(const struct ps_widget *widget,int y) {
  if (!widget||(widget->childc<2)) return -1;
  struct ps_widget *packer=widget->childv[1];
  int i=packer->childc; while (i-->0) {
    struct ps_widget *option=packer->childv[i];
    if (y<option->y) continue;
    if (y>=option->y+option->h) continue;
    return i;
  }
  return -1;
}

/* Primitive input events.
 */

static int _ps_menu_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  return 0;
}

static int _ps_menu_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if ((btnid==1)&&(value==1)) {
    int childp=ps_menu_get_index_by_vertical(widget,WIDGET->mousey);
    if (childp>=0) {
      if (ps_menu_set_thumb_position(widget,1)<0) return -1;
      WIDGET->optionp=childp;
    }
  }
  return 0;
}

static int _ps_menu_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_menu_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_menu_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (value) switch (btnid) {
    case PS_PLRBTN_UP: return ps_widget_menu_change_selection(widget,-1);
    case PS_PLRBTN_DOWN: return ps_widget_menu_change_selection(widget,1);
    case PS_PLRBTN_LEFT: return ps_widget_menu_adjust_selection(widget,-1);
    case PS_PLRBTN_RIGHT: return ps_widget_menu_adjust_selection(widget,1);
    case PS_PLRBTN_A: return ps_widget_menu_activate(widget);
    case PS_PLRBTN_START: return ps_widget_menu_activate(widget);
  }
  return 0;
}

/* Digested input events.
 */

static int _ps_menu_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_menu_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_menu_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_menu_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_menu_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_menu={

  .name="menu",
  .objlen=sizeof(struct ps_widget_menu),

  .del=_ps_menu_del,
  .init=_ps_menu_init,

  .measure=_ps_menu_measure,
  .pack=_ps_menu_pack,

  .mousemotion=_ps_menu_mousemotion,
  .mousebutton=_ps_menu_mousebutton,
  .mousewheel=_ps_menu_mousewheel,
  .key=_ps_menu_key,
  .userinput=_ps_menu_userinput,

  .mouseenter=_ps_menu_mouseenter,
  .mouseexit=_ps_menu_mouseexit,
  .activate=ps_widget_menu_activate,
  .cancel=_ps_menu_cancel,
  .adjust=ps_widget_menu_adjust_selection,
  .focus=_ps_menu_focus,
  .unfocus=_ps_menu_unfocus,

};

/* Set thumb position.
 */
 
static int ps_menu_set_thumb_position(struct ps_widget *widget,int animate) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  struct ps_widget *thumb=widget->childv[0];
  struct ps_widget *packer=widget->childv[1];
  int x,y,w,h;

  if ((WIDGET->optionp<0)||(WIDGET->optionp>=packer->childc)) {
    x=y=w=h=0;
  } else {
    struct ps_widget *selection=packer->childv[WIDGET->optionp];
    x=selection->x; // packer is always at (0,0).
    y=selection->y;
    w=selection->w;
    h=selection->h;
  }

  if (animate) {
    struct ps_gui *gui=ps_widget_get_gui(widget);
    if (!gui) goto _immediate_; // Maybe the menu is detached? Don't panic.
    if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_x,x,PS_MENU_ANIMATE_TIME)<0) return -1;
    if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_y,y,PS_MENU_ANIMATE_TIME)<0) return -1;
    if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_w,w,PS_MENU_ANIMATE_TIME)<0) return -1;
    if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_h,h,PS_MENU_ANIMATE_TIME)<0) return -1;
  } else {
   _immediate_:
    thumb->x=x;
    thumb->y=y;
    thumb->w=w;
    thumb->h=h;
    if (ps_widget_pack(thumb)<0) return -1; // Not actually necessary, but let's pretend we don't know that.
  }
  
  return 0;
}

/* Accessors.
 */
 
struct ps_widget *ps_widget_menu_get_thumb(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return 0;
  return widget->childv[0];
}

struct ps_widget *ps_widget_menu_get_packer(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return 0;
  return widget->childv[1];
}

int ps_widget_menu_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

/* Get selection.
 */
 
int ps_widget_menu_get_selected_index(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  return WIDGET->optionp;
}

struct ps_widget *ps_widget_menu_get_selected_widget(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return 0;
  struct ps_widget *packer=widget->childv[1];
  if ((WIDGET->optionp<0)||(WIDGET->optionp>=packer->childc)) return 0;
  return packer->childv[WIDGET->optionp];
}

/* Change selection.
 */
 
int ps_widget_menu_change_selection(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  struct ps_widget *packer=widget->childv[1];
  PS_SFX_MENU_MOVE
  if (packer->childc<=1) {
    WIDGET->optionp=0;
  } else {
    WIDGET->optionp+=d;
    if (WIDGET->optionp<0) WIDGET->optionp=packer->childc-1;
    else if (WIDGET->optionp>=packer->childc) WIDGET->optionp=0;
  }
  if (ps_menu_set_thumb_position(widget,1)<0) return -1;
  return 0;
}

/* Adjust selection.
 */
 
int ps_widget_menu_adjust_selection(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  struct ps_widget *packer=widget->childv[1];
  if ((WIDGET->optionp<0)||(WIDGET->optionp>=packer->childc)) return 0;
  struct ps_widget *selection=packer->childv[WIDGET->optionp];
  return ps_widget_adjust(selection,d);
}

/* Activate.
 */
 
int ps_widget_menu_activate(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return -1;
  struct ps_widget *packer=widget->childv[1];
  if ((WIDGET->optionp<0)||(WIDGET->optionp>=packer->childc)) return 0;
  struct ps_widget *selection=packer->childv[WIDGET->optionp];

  // Callback or activate, not both.
  if (WIDGET->cb.fn) {
    if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  } else {
    if (ps_widget_activate(selection)<0) return -1;
  }
  return 0;
}

/* Add menu item.
 */
 
struct ps_widget *ps_widget_menu_spawn_label(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return 0;
  struct ps_widget *label=ps_widget_spawn(widget->childv[1],&ps_widget_type_label);
  if (!label) return 0;
  if (ps_widget_label_set_text(label,src,srcc)<0) return 0;
  return label;
}

struct ps_widget *ps_widget_menu_spawn_button(struct ps_widget *widget,const char *src,int srcc,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_menu)||(widget->childc!=2)) return 0;
  struct ps_widget *button=ps_widget_spawn(widget->childv[1],&ps_widget_type_button);
  if (!button) return 0;
  button->bgrgba=0x00000000;
  if (ps_widget_button_set_text(button,src,srcc)<0) return 0;
  if (ps_widget_button_set_margins(button,0,0)<0) return 0;
  if (ps_widget_button_set_callback(button,cb)<0) return 0;
  return button;
}
