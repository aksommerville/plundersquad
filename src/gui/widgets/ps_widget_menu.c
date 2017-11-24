#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"
#include "game/ps_sound_effects.h"

/* Widget definition.
 *
 * childv[0]: Cursor
 * childv[1]: Packer
 */

struct ps_widget_menu {
  struct ps_widget hdr;
  int cursorp;
};

#define WIDGET ((struct ps_widget_menu*)widget)

/* Delete.
 */

static void _ps_menu_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_menu_init(struct ps_widget *widget) {

  struct ps_widget *cursor=ps_widget_spawn(widget,&ps_widget_type_label);
  if (!cursor) return -1;
  cursor->bgrgba=0x0060a0ff;

  struct ps_widget *packer=ps_widget_spawn(widget,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,0)<0) return -1;

  return 0;
}

/* Measure.
 */

static int _ps_menu_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (widget->childc!=2) return -1;
  return ps_widget_measure(w,h,widget->childv[1],maxw,maxh);
}

/* Reposition cursor.
 */

static int ps_menu_reposition_cursor(struct ps_widget *widget,int animate) {
  struct ps_widget *cursor=widget->childv[0];
  int optionc=widget->childv[1]->childc;
  if ((WIDGET->cursorp<0)||(WIDGET->cursorp>=optionc)) {
    cursor->w=0;
    cursor->h=0;
  } else {
  
    struct ps_widget *option=widget->childv[1]->childv[WIDGET->cursorp];
    int dstx=option->x-2;
    int dsty=option->y;
    int dstw=option->w+4;
    int dsth=option->h;
    
    if (animate) {
      struct ps_gui *gui=ps_widget_get_gui(widget);
      if (!gui) goto _immediate_;
      if (ps_gui_transition_property(gui,cursor,PS_GUI_PROPERTY_x,dstx,5)<0) return -1;
      if (ps_gui_transition_property(gui,cursor,PS_GUI_PROPERTY_y,dsty,5)<0) return -1;
      if (ps_gui_transition_property(gui,cursor,PS_GUI_PROPERTY_w,dstw,5)<0) return -1;
      if (ps_gui_transition_property(gui,cursor,PS_GUI_PROPERTY_h,dsth,5)<0) return -1;
    } else {
     _immediate_:
      cursor->x=dstx; // Kosher even though they have different parents,
      cursor->y=dsty; // because the packer has exactly the same bounds as the menu.
      cursor->w=dstw;
      cursor->h=dsth;
    }
  }
  return 0;
}

/* Pack.
 */

static int _ps_menu_pack(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;
  
  struct ps_widget *packer=widget->childv[1];
  packer->x=packer->y=0;
  packer->w=widget->w;
  packer->h=widget->h;
  if (ps_widget_pack(packer)<0) return -1;

  if (ps_menu_reposition_cursor(widget,0)<0) return -1;

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
};

/* Add option.
 */

static int ps_menu_cb_click(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_menu)) return -1;
  if (widget->childc<2) return -1;

  /* Find the label among our packer's children, select it, and fire submit to the containing page. */
  int i=0;
  for (;i<widget->childv[1]->childc;i++) {
    struct ps_widget *child=widget->childv[1]->childv[i];
    if (child==label) {
      WIDGET->cursorp=i;
      if (ps_menu_reposition_cursor(widget,0)<0) return -1;
      if (ps_gui_activate_cursor(ps_widget_get_gui(widget))<0) return -1;
      return 0;
    }
  }

  return 0;
}
 
struct ps_widget *ps_widget_menu_add_option(struct ps_widget *widget,const char *text,int textc) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return 0;
  if (widget->childc!=2) return 0;
  struct ps_widget *label=ps_widget_spawn(widget->childv[1],&ps_widget_type_label);
  if (!label) return 0;
  if (ps_widget_label_set_text(label,text,textc)<0) return 0;
  
  // Passing self weakly as userdata to the child. 
  // This is a risk if the label could outsurvive us, but I don't think that will ever happen.
  // If we were to retain ourself here, there would be a retention loop and memory leak.
  if (ps_widget_label_set_click_cb(label,ps_menu_cb_click,widget,0)<0) return 0;
  
  return label;
}

/* Add slider.
 */
 
struct ps_widget *ps_widget_menu_add_slider(struct ps_widget *widget,const char *text,int textc,int lo,int hi) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return 0;
  if (widget->childc!=2) return 0;
  struct ps_widget *slider=ps_widget_spawn(widget->childv[1],&ps_widget_type_slider);
  if (!slider) return 0;
  if (ps_widget_slider_setup(slider,text,textc,lo,hi)<0) return 0;
  return slider;
}

/* Move cursor.
 */
 
int ps_widget_menu_move_cursor(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return -1;
  if (widget->childc!=2) return -1;
  int optionc=widget->childv[1]->childc;
  if (optionc<=1) {
    WIDGET->cursorp=0;
  } else {
    PS_SFX_MENU_MOVE
    d%=optionc;
    int np=WIDGET->cursorp+d;
    if (np<0) np+=optionc;
    else if (np>=optionc) np-=optionc;
    if (WIDGET->cursorp==np) return 0;
    WIDGET->cursorp=np;
  }
  if (ps_menu_reposition_cursor(widget,1)<0) return -1;
  return 0;
}

/* Adjust selected value.
 */

int ps_widget_menu_adjust_selection(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return -1;
  if (widget->childc!=2) return -1;
  if ((WIDGET->cursorp<0)||(WIDGET->cursorp>=widget->childv[1]->childc)) return 0;
  struct ps_widget *selection=widget->childv[1]->childv[WIDGET->cursorp];
  
  if (selection->type==&ps_widget_type_slider) {
    PS_SFX_MENU_ADJUST
    if (ps_widget_slider_adjust(selection,d)<0) return -1;
    return 0;
  }

  return 0;
}

/* Get cursor.
 */
 
int ps_widget_menu_get_cursor(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return -1;
  return WIDGET->cursorp;
}

/* Get slider value.
 */
 
int ps_widget_menu_get_slider(const struct ps_widget *widget,int p) {
  if (!widget||(widget->type!=&ps_widget_type_menu)) return 0;
  if (widget->childc!=2) return 0;
  if ((p<0)||(p>=widget->childv[1]->childc)) return 0;
  struct ps_widget *slider=widget->childv[1]->childv[p];
  return ps_widget_slider_get_value(slider);
}
