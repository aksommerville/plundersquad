/* ps_widget_inputcfg.c
 * Interactive configuration for one input device.
 * Children:
 *  [0] namelabel
 *  [1] inputstatus
 *  [2] message
 *  [3] resetbutton
 *  [4] menu
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_map.h"
#include "input/ps_input_premap.h"
#include "input/ps_input_maptm.h"
#include "input/ps_input_config.h"
#include "input/ps_input_icfg.h"
#include "os/ps_clockassist.h"
#include "util/ps_enums.h"

#define PS_INPUTCFG_TEXT_MARGIN 5
#define PS_INPUTCFG_MAP_COUNT 7
#define PS_INPUTCFG_NAME_LENGTH_LIMIT 14

static int ps_inputcfg_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_inputcfg_map {
  int srcbtnid;
  int srcdirection;
  uint16_t dstbtnid;
};

struct ps_widget_inputcfg {
  struct ps_widget hdr;
  struct ps_input_device *device;
  int watchid;
  int64_t suppress_input_time; // Cheap hack to ignore events for a short interval.

  struct ps_input_icfg *icfg; // If not null, we are resetting.

  int menu_open;
  
};

#define WIDGET ((struct ps_widget_inputcfg*)widget)

/* Delete.
 */

static void _ps_inputcfg_del(struct ps_widget *widget) {
  if (WIDGET->device) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
    ps_input_device_del(WIDGET->device);
  }
  ps_input_icfg_del(WIDGET->icfg);
}

/* Initialize.
 */

static int _ps_inputcfg_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0xeeeeeeff;
  WIDGET->watchid=-1;

  if (!(child=ps_widget_label_spawn(widget,"",0))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_inputstatus))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_textblock))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Reset",5,ps_callback(ps_inputcfg_cb_begin_reset,0,widget)))) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_inputcfg_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_inputcfg) return -1;
  if (WIDGET->menu_open) {
    if (widget->childc!=5) return -1;
  } else {
    if (widget->childc!=4) return -1;
  }
  return 0;
}

static struct ps_widget *ps_inputcfg_get_namelabel(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_inputcfg_get_inputstatus(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_inputcfg_get_message(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_inputcfg_get_resetbutton(const struct ps_widget *widget) { return widget->childv[3]; }

static struct ps_widget *ps_inputcfg_get_menu(const struct ps_widget *widget) {
  if (WIDGET->menu_open) return widget->childv[4];
  return 0;
}

/* Menu widget.
 */

static int ps_inputcfg_open_menu(struct ps_widget *widget) {
  if (WIDGET->menu_open) return 0;
  
  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  if (!menu) return -1;

  menu->bgrgba=0xa0a0c0e0;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_inputcfg_cb_menu,0,widget))<0) return -1;
  
  if (!ps_widget_menu_spawn_label(menu,"Back",4)) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Reset",5)) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Done",4)) return -1;

  WIDGET->menu_open=1;
  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

static int ps_inputcfg_close_menu(struct ps_widget *widget) {
  if (!WIDGET->menu_open) return 0;
  if (ps_widget_remove_child(widget,ps_inputcfg_get_menu(widget))<0) return -1;
  WIDGET->menu_open=0;
  return 0;
}

/* Pack.
 */

static int _ps_inputcfg_pack(struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  struct ps_widget *namelabel=ps_inputcfg_get_namelabel(widget);
  struct ps_widget *inputstatus=ps_inputcfg_get_inputstatus(widget);
  struct ps_widget *message=ps_inputcfg_get_message(widget);
  struct ps_widget *resetbutton=ps_inputcfg_get_resetbutton(widget);
  struct ps_widget *menu=ps_inputcfg_get_menu(widget); // Can be null.
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,namelabel,widget->w,widget->h)<0) return -1;
  namelabel->x=0;
  namelabel->y=0;
  namelabel->w=widget->w;
  namelabel->h=chh;

  if (ps_widget_measure(&chw,&chh,inputstatus,widget->w,widget->h-namelabel->h-namelabel->y)<0) return -1;
  inputstatus->x=0;
  inputstatus->y=namelabel->y+namelabel->h;
  inputstatus->w=widget->w;
  inputstatus->h=chh;

  if (ps_widget_measure(&chw,&chh,resetbutton,widget->w,widget->h-inputstatus->h-inputstatus->y)<0) return -1;
  resetbutton->x=(widget->w>>1)-(chw>>1);
  resetbutton->y=inputstatus->y+inputstatus->h;
  resetbutton->w=chw;
  resetbutton->h=chh;

  if (ps_widget_measure(&chw,&chh,message,widget->w-(PS_INPUTCFG_TEXT_MARGIN<<1),widget->h-resetbutton->h-resetbutton->y-(PS_INPUTCFG_TEXT_MARGIN<<1))<0) return -1;
  message->x=PS_INPUTCFG_TEXT_MARGIN;
  message->y=resetbutton->y+resetbutton->h+PS_INPUTCFG_TEXT_MARGIN;
  message->w=widget->w-(PS_INPUTCFG_TEXT_MARGIN<<1);
  message->h=chh;

  int totalh=namelabel->h+inputstatus->h+resetbutton->h+message->h;
  int totalspace=widget->h-totalh;
  if (totalspace>=5) {
    int spacing=totalspace/5;
    namelabel->y=spacing;
    inputstatus->y=namelabel->y+namelabel->h+spacing;
    resetbutton->y=inputstatus->y+inputstatus->h+spacing;
    message->y=resetbutton->y+resetbutton->h+spacing;
  }

  if (menu) {
    menu->x=0;
    menu->y=0;
    menu->w=widget->w;
    menu->h=widget->h-menu->y;
  }
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_inputcfg={

  .name="inputcfg",
  .objlen=sizeof(struct ps_widget_inputcfg),

  .del=_ps_inputcfg_del,
  .init=_ps_inputcfg_init,

  .pack=_ps_inputcfg_pack,

};

/* Finish full reset.
 */

static int ps_inputcfg_commit_reset(struct ps_widget *widget) {
  int playerid=0;
  if (WIDGET->device&&WIDGET->device->map) playerid=WIDGET->device->map->plrid;
  if (ps_input_icfg_finish(WIDGET->icfg,playerid)<0) return -1;
  ps_input_icfg_del(WIDGET->icfg);
  WIDGET->icfg=0;
  if (ps_widget_inputstatus_set_manual(ps_inputcfg_get_inputstatus(widget),0)<0) return -1;
  WIDGET->suppress_input_time=ps_time_now()+100000; // 100 ms, which could be a lot shorter I think.
  if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),"",0)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Update UI during full reset.
 */

static int ps_inputcfg_update_reset(struct ps_widget *widget) {

  int btnid=ps_input_icfg_get_current_button(WIDGET->icfg);
  if (!btnid) return -1;
  const char *btnname=ps_plrbtn_repr(btnid);
  if (!btnname) return -1;

  char msg[256];
  int msgc;
  switch (ps_input_icfg_get_qualifier(WIDGET->icfg)) {
    case PS_ICFG_QUALIFIER_REPEAT: {
        msgc=snprintf(msg,sizeof(msg),"Press %s again",btnname);
      } break;
    case PS_ICFG_QUALIFIER_MISMATCH: {
        msgc=snprintf(msg,sizeof(msg),"Mismatch! Press %s",btnname);
      } break;
    default: {
        msgc=snprintf(msg,sizeof(msg),"Press %s",btnname);
      } break;
  }
  if ((msgc<1)||(msgc>=sizeof(msg))) return -1;

  if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),msg,msgc)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

/* Button callback.
 */

static int ps_inputcfg_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;
  //ps_log(GUI,TRACE,"%s %d=%d [%s]",__func__,btnid,value,mapped?"mapped":"raw");

  if (WIDGET->suppress_input_time) {
    if (ps_time_now()<WIDGET->suppress_input_time) return 0;
    WIDGET->suppress_input_time=0;
  }

  if (ps_widget_inputcfgpage_bump(widget)<0) return -1;

  /* Are we gathering buttons for reset? */
  if (WIDGET->icfg) {
    if (mapped) return 0;
    int committed=0;
    int err=ps_input_icfg_event(WIDGET->icfg,btnid,value);
    if (err<0) return -1;
    if (err) {
      if (ps_input_icfg_is_ready(WIDGET->icfg)) {
        if (ps_inputcfg_commit_reset(widget)<0) return -1;
        committed=1;
      }
    }
    if (!committed) {
      if (ps_inputcfg_update_reset(widget)<0) return -1;
    }

  /* Not resetting -- is the menu open? */
  } else if (WIDGET->menu_open) {
    struct ps_widget *menu=ps_inputcfg_get_menu(widget);
    if (value&&mapped) switch (btnid) {
      case PS_PLRBTN_UP: if (ps_widget_menu_change_selection(menu,-1)<0) return -1; break;
      case PS_PLRBTN_DOWN: if (ps_widget_menu_change_selection(menu,1)<0) return -1; break;
      case PS_PLRBTN_A: if (ps_widget_menu_activate(menu)<0) return -1; break;
      case PS_PLRBTN_START: if (ps_inputcfg_close_menu(widget)<0) return -1; break;
    }

  /* Menu not open -- should we open it? */
  } else {
    if (mapped&&(btnid==PS_PLRBTN_START)&&value) {
      if (ps_inputcfg_open_menu(widget)<0) return -1;
    }

  }
  
  return 0;
}

/* Setup.
 */
 
int ps_widget_inputcfg_setup(struct ps_widget *widget,struct ps_input_device *device) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  if (WIDGET->device) return -1; // Already initialized.
  if (!device) return -1;
  
  if (ps_input_device_ref(device)<0) return -1;
  ps_input_device_del(WIDGET->device);
  WIDGET->device=device;
  if (ps_input_premap_build_for_device(device)<0) return -1;

  if ((WIDGET->watchid=ps_input_device_watch_buttons(device,ps_inputcfg_cb_button,0,widget))<0) return -1;

  int namec=device->namec;
  if (namec>PS_INPUTCFG_NAME_LENGTH_LIMIT) namec=PS_INPUTCFG_NAME_LENGTH_LIMIT;
  if (ps_widget_label_set_text(ps_inputcfg_get_namelabel(widget),device->name,namec)<0) return -1;
  
  if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),"",0)<0) return -1;
  if (ps_widget_inputstatus_set_device(ps_inputcfg_get_inputstatus(widget),device)<0) return -1;
  
  return 0;
}

/* Get device.
 */

struct ps_input_device *ps_widget_inputcfg_get_device(const struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return 0;
  return WIDGET->device;
}

/* Begin full reset.
 */
 
int ps_inputcfg_cb_begin_reset(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;

  ps_input_icfg_del(WIDGET->icfg);
  if (!(WIDGET->icfg=ps_input_icfg_new(WIDGET->device))) return -1;

  if (ps_inputcfg_update_reset(widget)<0) return -1;
  
  return 0;
}

/* Set keyboard shortcut name for reset button.
 */
 
int ps_widget_inputcfg_set_reset_shortcut_name(struct ps_widget *widget,const char *name) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  char text[32];
  int textc=snprintf(text,sizeof(text),"Reset [%s]",name);
  if ((textc<0)||(textc>sizeof(text))) textc=0;
  if (ps_widget_button_set_text(ps_inputcfg_get_resetbutton(widget),text,textc)<0) return -1;
  return 0;
}

/* Dismiss page containing me.
 */

static int ps_inputcfg_dismiss_page(struct ps_widget *widget) {
  struct ps_widget *page=widget;
  while (page&&(page->type!=&ps_widget_type_inputcfgpage)) page=page->parent;
  if (!page) return 0;
  return ps_widget_kill(page);
}

/* Menu "Reset" -- enter reset mode and dismiss menu.
 */

static int ps_inputcfg_set_all_from_menu(struct ps_widget *widget) {
  if (ps_inputcfg_close_menu(widget)<0) return -1;
  if (ps_inputcfg_cb_begin_reset(0,widget)<0) return -1;
  return 0;
}

/* Callback from menu.
 */
 
static int ps_inputcfg_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_inputcfg_close_menu(widget);
    case 1: return ps_inputcfg_set_all_from_menu(widget);
    case 2: return ps_inputcfg_dismiss_page(widget);
  }
  return 0;
}
