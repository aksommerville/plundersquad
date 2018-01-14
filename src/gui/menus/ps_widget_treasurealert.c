/* ps_widget_treasurealert.c
 * Splash to show the user the treasure he just picked up.
 * Children:
 *   [0] message
 *   [1] texture
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "res/ps_restype.h"
#include "input/ps_input.h"

/* Object definition.
 */

struct ps_widget_treasurealert {
  struct ps_widget hdr;
  const struct ps_res_trdef *trdef;
};

#define WIDGET ((struct ps_widget_treasurealert*)widget)

/* Delete.
 */

static void _ps_treasurealert_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_treasurealert_init(struct ps_widget *widget) {

  widget->bgrgba=0x00000000;

  struct ps_widget *child;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
  child->fgrgba=0xffff00ff;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_texture))) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_treasurealert_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_treasurealert_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_treasurealert_pack(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;
  struct ps_widget *message=widget->childv[0];
  struct ps_widget *texture=widget->childv[1];
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,message,widget->w,widget->h)<0) return -1;
  message->x=(widget->w>>1)-(chw>>1);
  message->w=chw;
  message->h=chh;

  texture->w=128;
  texture->h=128;
  texture->x=(widget->w>>1)-(texture->w>>1);

  int children_height=message->h+texture->h;
  int vertmargin=(widget->h-children_height)/3;
  message->y=vertmargin;
  texture->y=message->y+message->h+vertmargin;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_treasurealert_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_treasurealert_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_treasurealert_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_treasurealert_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_treasurealert_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_treasurealert_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (value) switch (btnid) {
    case PS_PLRBTN_A:
    case PS_PLRBTN_B:
    case PS_PLRBTN_START: {
        if (ps_widget_kill(widget)<0) return -1;
        if (ps_input_suppress_player_actions(30)<0) return -1;
      } break;
  }
  return 0;
}

/* Digested input events.
 */

static int _ps_treasurealert_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_treasurealert_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_treasurealert_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_treasurealert_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_treasurealert_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_treasurealert_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_treasurealert_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_treasurealert={

  .name="treasurealert",
  .objlen=sizeof(struct ps_widget_treasurealert),

  .del=_ps_treasurealert_del,
  .init=_ps_treasurealert_init,

  .draw=_ps_treasurealert_draw,
  .measure=_ps_treasurealert_measure,
  .pack=_ps_treasurealert_pack,

  .update=_ps_treasurealert_update,
  .mousemotion=_ps_treasurealert_mousemotion,
  .mousebutton=_ps_treasurealert_mousebutton,
  .mousewheel=_ps_treasurealert_mousewheel,
  .key=_ps_treasurealert_key,
  .userinput=_ps_treasurealert_userinput,

  .mouseenter=_ps_treasurealert_mouseenter,
  .mouseexit=_ps_treasurealert_mouseexit,
  .activate=_ps_treasurealert_activate,
  .cancel=_ps_treasurealert_cancel,
  .adjust=_ps_treasurealert_adjust,
  .focus=_ps_treasurealert_focus,
  .unfocus=_ps_treasurealert_unfocus,

};

/* Setup.
 */
 
int ps_widget_treasurealert_setup(struct ps_widget *widget,const struct ps_res_trdef *trdef) {
  if (!widget||(widget->type!=&ps_widget_type_treasurealert)) return -1;
  WIDGET->trdef=trdef;
  if (ps_gui_animate_property(ps_widget_get_gui(widget),widget,PS_WIDGET_PROPERTY_bgrgba,0x000000c0,60)<0) return -1;

  if (WIDGET->trdef) {
  
    char message[256];
    int messagec=snprintf(message,sizeof(message),"You found %.*s!",WIDGET->trdef->namec,WIDGET->trdef->name);
    if ((messagec<0)||(messagec>=sizeof(message))||!WIDGET->trdef->namec) {
      messagec=snprintf(message,sizeof(message),"You found a treasure!");
    }
    if (ps_widget_label_set_text(widget->childv[0],message,messagec)<0) return -1;

    if (WIDGET->trdef->image) {
      if (ps_widget_texture_set_texture(widget->childv[1],WIDGET->trdef->image->texture)<0) return -1;
    }
    
  }
  
  return 0;
}
