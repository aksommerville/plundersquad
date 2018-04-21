/* ps_widget_heroselect.c
 * Single panel in assemble page, associated with one input device.
 * Children are volatile, rebuilt on phase changes.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "input/ps_input_device.h"
#include "input/ps_input_button.h"
#include "input/ps_input_premap.h"
#include "input/ps_input_map.h"
#include "input/ps_input_maptm.h"
#include "input/ps_input_config.h"
#include "input/ps_input_icfg.h"
#include "input/ps_input.h"
#include "video/ps_video.h"
#include "os/ps_clockassist.h"
#include "game/ps_sound_effects.h"

#define PS_HEROSELECT_PHASE_WELCOME   1 /* Not clicked in. */
#define PS_HEROSELECT_PHASE_QUERY     2 /* Select hero and colors. */
#define PS_HEROSELECT_PHASE_READY     3 /* Committed, waiting for other players. */
#define PS_HEROSELECT_PHASE_CONFIG    4 /* Configuring device map. */

#define PS_HEROSELECT_NAME_LIMIT 16

#define PS_HEROSELECT_INPUT_DELAY 50000

#define PS_HEROSELECT_SPRDEF_ID 1

int ps_heroselect_rebuild_children_for_phase(struct ps_widget *widget,int phase);

/* Object definition.
 */

struct ps_widget_heroselect {
  struct ps_widget hdr;
  struct ps_input_device *device;
  int device_watchid;
  int phase;
  int64_t input_delay; // If nonzero, discard input events until after this time.
  int plrdefid;
  int palette;

  struct ps_input_icfg *icfg;
};

#define WIDGET ((struct ps_widget_heroselect*)widget)

/* Delete.
 */

static void _ps_heroselect_del(struct ps_widget *widget) {
  if (WIDGET->device_watchid>=0) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->device_watchid);
  }
  ps_input_device_del(WIDGET->device);
  ps_input_icfg_del(WIDGET->icfg);
}

/* Initialize.
 */

static int _ps_heroselect_init(struct ps_widget *widget) {

  WIDGET->device_watchid=-1;
  WIDGET->input_delay=ps_time_now()+PS_HEROSELECT_INPUT_DELAY;
  WIDGET->plrdefid=1;
  WIDGET->palette=0;

  if (ps_heroselect_rebuild_children_for_phase(widget,PS_HEROSELECT_PHASE_WELCOME)<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_heroselect_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_heroselect) return -1;
  return 0;
}

/* Update device label.
 */

static int ps_heroselect_update_device_label(struct ps_widget *widget) {
  if (widget->childc<1) return -1;
  struct ps_widget *label=widget->childv[0];
  if (WIDGET->device&&WIDGET->device->namec) {
    int namec=WIDGET->device->namec;
    if (namec>PS_HEROSELECT_NAME_LIMIT) namec=PS_HEROSELECT_NAME_LIMIT;
    if (ps_widget_label_set_text(label,WIDGET->device->name,namec)<0) return -1;
  } else {
    if (ps_widget_label_set_text(label,"???",3)<0) return -1;
  }
  return 0;
}

/* Build child widgets.
 * Caller should pack after.
 */

static int ps_heroselect_spawn_device_label(struct ps_widget *widget) {
  struct ps_widget *label=ps_widget_spawn(widget,&ps_widget_type_label);
  if (!label) return -1;
  label->fgrgba=0xffff00ff;
  if (ps_heroselect_update_device_label(widget)<0) return -1;
  return 0;
}

static int ps_heroselect_spawn_sprite(struct ps_widget *widget) {
  struct ps_widget *sprite=ps_widget_spawn(widget,&ps_widget_type_sprite);
  if (!sprite) return -1;
  if (ps_widget_sprite_load_sprdef(sprite,PS_HEROSELECT_SPRDEF_ID)<0) return -1;
  
  if (ps_widget_sprite_set_plrdefid(sprite,WIDGET->plrdefid)<0) return -1;
  if (ps_widget_sprite_set_palette(sprite,WIDGET->palette)<0) return -1;

  // Setting plrdefid and palette may modify our choices to ensure uniqueness.
  // It is important that we read them back immediately.
  WIDGET->plrdefid=ps_widget_sprite_get_plrdefid(sprite);
  WIDGET->palette=ps_widget_sprite_get_palette(sprite);

  return 0;
}
 
int ps_heroselect_rebuild_children_for_phase(struct ps_widget *widget,int phase) {
  struct ps_widget *child;
  WIDGET->phase=phase;
  if (ps_widget_remove_all_children(widget)<0) return -1;
  switch (phase) {

    case PS_HEROSELECT_PHASE_WELCOME: {
        if (ps_heroselect_spawn_device_label(widget)<0) return -1;
        if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
        if (ps_widget_label_set_text(child,"Click in!",-1)<0) return -1;
        child->fgrgba=0xffffffff;
      } break;

    case PS_HEROSELECT_PHASE_QUERY: {
        if (ps_heroselect_spawn_device_label(widget)<0) return -1;
        if (ps_heroselect_spawn_sprite(widget)<0) return -1;
      } break;

    case PS_HEROSELECT_PHASE_READY: {
        if (ps_heroselect_spawn_device_label(widget)<0) return -1;
        if (ps_heroselect_spawn_sprite(widget)<0) return -1;
        if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
        if (ps_widget_label_set_text(child,"Ready",5)<0) return -1;
        child->fgrgba=0xffa0c0ff;
      } break;

    case PS_HEROSELECT_PHASE_CONFIG: {
        if (ps_heroselect_spawn_device_label(widget)<0) return -1;
        if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
        child->fgrgba=0xa0ff60ff;
        int btnid=ps_input_icfg_get_current_button(WIDGET->icfg);
        const char *btnname=ps_plrbtn_repr(btnid);
        char message[128];
        int messagec;
        switch (ps_input_icfg_get_qualifier(WIDGET->icfg)) {
          case PS_ICFG_QUALIFIER_REPEAT: messagec=snprintf(message,sizeof(message),"Press %s again",btnname); break;
          case PS_ICFG_QUALIFIER_MISMATCH: child->fgrgba=0xffa0a0ff; messagec=snprintf(message,sizeof(message),"Error! Press %s",btnname); break;
          default: messagec=snprintf(message,sizeof(message),"Press %s",btnname); break;
        }
        if ((messagec<0)||(messagec>sizeof(message))) messagec=0;
        if (ps_widget_label_set_text(child,message,messagec)<0) return -1;
      } break;

  }
  return 0;
}

/* General properties.
 */

static int _ps_heroselect_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_heroselect_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_heroselect_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int _ps_heroselect_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_video_draw_rect(parentx+widget->x+1,parenty+widget->y+1,widget->w-2,widget->h-2,0x80808080)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_heroselect_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 * We have different constitution depending on the phase.
 * But luckily, all three phases fit the same basic mold.
 */

static int _ps_heroselect_pack(struct ps_widget *widget) {
  int chw,chh,i;
  struct ps_widget *child;

  /* First child is the device label. Take the full width, at top. */
  if (widget->childc>=1) {
    child=widget->childv[0];
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    child->x=0;
    child->y=0;
    child->w=widget->w;
    child->h=chh;
  }

  /* Second child is click-in message or sprite. Either way, it gets centered. */
  if (widget->childc>=2) {
    child=widget->childv[1];
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    child->x=(widget->w>>1)-(chw>>1);
    child->y=(widget->h>>1)-(chh>>1);
    child->w=chw;
    child->h=chh;
  }

  /* Third child if present is the footer. Full width at bottom. */
  if (widget->childc>=3) {
    child=widget->childv[2];
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    child->x=0;
    child->y=widget->h-chh;
    child->w=widget->w;
    child->h=chh;
  }
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_heroselect_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_heroselect_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_heroselect_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_heroselect_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_heroselect_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_heroselect_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_heroselect_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_heroselect_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_heroselect_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_heroselect_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_heroselect_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_heroselect_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_heroselect={

  .name="heroselect",
  .objlen=sizeof(struct ps_widget_heroselect),

  .del=_ps_heroselect_del,
  .init=_ps_heroselect_init,

  .get_property=_ps_heroselect_get_property,
  .set_property=_ps_heroselect_set_property,
  .get_property_type=_ps_heroselect_get_property_type,

  .draw=_ps_heroselect_draw,
  .measure=_ps_heroselect_measure,
  .pack=_ps_heroselect_pack,

  .mousemotion=_ps_heroselect_mousemotion,
  .mousebutton=_ps_heroselect_mousebutton,
  .mousewheel=_ps_heroselect_mousewheel,
  .key=_ps_heroselect_key,
  .userinput=_ps_heroselect_userinput,

  .mouseenter=_ps_heroselect_mouseenter,
  .mouseexit=_ps_heroselect_mouseexit,
  .activate=_ps_heroselect_activate,
  .cancel=_ps_heroselect_cancel,
  .adjust=_ps_heroselect_adjust,
  .focus=_ps_heroselect_focus,
  .unfocus=_ps_heroselect_unfocus,

};

/* Finalize input mapping.
 */
 
static int ps_heroselect_finalize_mapping(struct ps_widget *widget) {
  int playerid=0;
  if (WIDGET->device&&WIDGET->device->map) playerid=WIDGET->device->map->plrid;
  if (ps_input_icfg_finish(WIDGET->icfg,playerid)<0) return -1;
  ps_input_icfg_del(WIDGET->icfg);
  WIDGET->icfg=0;
  if (ps_heroselect_rebuild_children_for_phase(widget,PS_HEROSELECT_PHASE_QUERY)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  WIDGET->input_delay=ps_time_now()+PS_HEROSELECT_INPUT_DELAY;
  return 0;
}

/* Update UI for input mapping.
 */
 
static int ps_heroselect_update_ui_for_input_config(struct ps_widget *widget) {
  if (ps_heroselect_rebuild_children_for_phase(widget,PS_HEROSELECT_PHASE_CONFIG)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Receive input event for unmapped device.
 */

static int ps_heroselect_input_config_event(struct ps_widget *widget,int btnid,int value) {
  //ps_log(GUI,TRACE,"%s %d=%d",__func__,btnid,value);
  int err;

  if (WIDGET->phase==PS_HEROSELECT_PHASE_WELCOME) {
    return ps_heroselect_update_ui_for_input_config(widget);
  }

  err=ps_input_icfg_event(WIDGET->icfg,btnid,value);
  if (err<0) return -1;
  int committed=0;
  if (err) {
    if (ps_input_icfg_is_ready(WIDGET->icfg)) {
      if (ps_heroselect_finalize_mapping(widget)<0) return -1;
      committed=1;
    }
  }
  if (!committed) {
    if (ps_heroselect_update_ui_for_input_config(widget)<0) return -1;
  }
  
  return 0;
}

/* Change selection in QUERY phase.
 */

static int ps_heroselect_modify(struct ps_widget *widget,int dx,int dy) {
  if (WIDGET->phase!=PS_HEROSELECT_PHASE_QUERY) return 0;
  if (widget->childc>=2) {
    struct ps_widget *sprite=widget->childv[1];
    if (dx) {
      if (ps_widget_sprite_modify_plrdefid(sprite,dx)<0) return -1;
      WIDGET->plrdefid=ps_widget_sprite_get_plrdefid(sprite);
      WIDGET->palette=ps_widget_sprite_get_palette(sprite); // Changing plrdefid can also change palette
    }
    if (dy) {
      if (ps_widget_sprite_modify_palette(sprite,dy)<0) return -1;
      WIDGET->palette=ps_widget_sprite_get_palette(sprite);
    }
  }
  return 0;
}

/* Advance or retreat phase.
 */

static int ps_heroselect_advance(struct ps_widget *widget) {
  int nextphase;
  switch (WIDGET->phase) {
    case PS_HEROSELECT_PHASE_WELCOME: nextphase=PS_HEROSELECT_PHASE_QUERY; break;
    case PS_HEROSELECT_PHASE_QUERY: nextphase=PS_HEROSELECT_PHASE_READY; break;
    default: return 0;
  }
  if (ps_heroselect_rebuild_children_for_phase(widget,nextphase)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  WIDGET->input_delay=ps_time_now()+PS_HEROSELECT_INPUT_DELAY;
  return 0;
}

static int ps_heroselect_retreat(struct ps_widget *widget) {
  int nextphase;
  switch (WIDGET->phase) {
    case PS_HEROSELECT_PHASE_QUERY: nextphase=PS_HEROSELECT_PHASE_WELCOME; break;
    case PS_HEROSELECT_PHASE_READY: nextphase=PS_HEROSELECT_PHASE_QUERY; break;
    default: return 0;
  }
  if (ps_heroselect_rebuild_children_for_phase(widget,nextphase)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  WIDGET->input_delay=ps_time_now()+PS_HEROSELECT_INPUT_DELAY;
  return 0;
}

/* Receive input from device.
 */

static int ps_heroselect_cb_input(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;
  //ps_log(GUI,TRACE,"%s %d=%d [%d]",__func__,btnid,value,mapped);

  /* After a major event, we discard input for a little while. */
  if (WIDGET->input_delay>0) {
    int64_t now=ps_time_now();
    if (now>WIDGET->input_delay) {
      WIDGET->input_delay=0;
    } else {
      return 0;
    }
  }

  /* When the device has no map, we are configuring the device. */
  if (!device->map&&device->premap) {
    if (mapped) return 0;
    if (!WIDGET->icfg) {
      if (!(WIDGET->icfg=ps_input_icfg_new(WIDGET->device))) return -1;
    }
    return ps_heroselect_input_config_event(widget,btnid,value);
  }

  /* We only care about presses, not releases. */
  if (!value) return 0;

  /* Mapped events can trigger modify, advance, or retreat. */
  if (mapped) switch (btnid) {
    case PS_PLRBTN_UP: return ps_heroselect_modify(widget,0,-1);
    case PS_PLRBTN_DOWN: return ps_heroselect_modify(widget,0,1);
    case PS_PLRBTN_LEFT: return ps_heroselect_modify(widget,-1,0);
    case PS_PLRBTN_RIGHT: return ps_heroselect_modify(widget,1,0);
    case PS_PLRBTN_A: return ps_heroselect_advance(widget);
    case PS_PLRBTN_B: return ps_heroselect_retreat(widget);
    case PS_PLRBTN_START: return ps_heroselect_advance(widget);
  }
  
  return 0;
}

/* Accessors.
 */
 
int ps_widget_heroselect_set_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  if (device&&(ps_input_device_ref(device)<0)) return -1;

  if (WIDGET->device_watchid>=0) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->device_watchid);
    WIDGET->device_watchid=-1;
  }
  
  ps_input_device_del(WIDGET->device);
  WIDGET->device=device;

  if (ps_heroselect_update_device_label(widget)<0) return -1;

  if (device) {
    if ((WIDGET->device_watchid=ps_input_device_watch_buttons(
      device,ps_heroselect_cb_input,0,widget
    ))<0) return -1;
  }
  
  return 0;
}

struct ps_input_device *ps_widget_heroselect_get_device(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return 0;
  return WIDGET->device;
}

int ps_widget_heroselect_set_plrdefid(struct ps_widget *widget,int plrdefid) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  WIDGET->plrdefid=plrdefid;
  return 0;
}

int ps_widget_heroselect_get_plrdefid(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  return WIDGET->plrdefid;
}

int ps_widget_heroselect_set_palette(struct ps_widget *widget,int palette) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  WIDGET->palette=palette;
  return 0;
}

int ps_widget_heroselect_get_palette(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  return WIDGET->palette;
}

/* State queries.
 */
 
int ps_widget_heroselect_is_pending(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return 0;
  switch (WIDGET->phase) {
    case PS_HEROSELECT_PHASE_QUERY:
    case PS_HEROSELECT_PHASE_CONFIG:
      return 1;
  }
  return 0;
}

int ps_widget_heroselect_is_ready(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return 0;
  return (WIDGET->phase==PS_HEROSELECT_PHASE_READY)?1:0;
}
