/* ps_widget_heropanel.c
 * Interactive panel for one input device, under assemblepage.
 * Directly contained by heropacker.
 * We draw the fancy border ourselves, and track the interaction state.
 * We have three children at all times:
 *  [0] devicelabel (top)
 *  [1] messagelabel (bottom)
 *  [2] content (middle, variable type)
 *
 * There is a linear state flow: AVAILABLE,CHOOSING,READY
 * Also a MENU state which may be entered at any stage of the linear flow, by pressing START.
 * Also a CONFIGURING state which is only required once for unconfigured devices, before CHOOSING or MENU.
 *
 */

#include "ps.h"
#include "gui/ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/menus/ps_menus.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_icfg.h"
#include "input/ps_input_button.h"
#include "input/ps_input_premap.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "os/ps_clockassist.h"
#include "util/ps_enums.h"

#define PS_HEROPANEL_DEVICE_NAME_LIMIT 12

#define PS_HEROPANEL_DEVICELABEL_COLOR  0xffffffff
#define PS_HEROPANEL_MESSAGELABEL_COLOR 0xffffffff

#define PS_HEROPANEL_BGCOLOR_TIME 60
#define PS_HEROPANEL_BGCOLOR_ERROR      0xa02040ff
#define PS_HEROPANEL_BGCOLOR_IDLE       0x00000000
#define PS_HEROPANEL_BGCOLOR_PENDING    0x484038ff
#define PS_HEROPANEL_BGCOLOR_READY      0x004000ff

#define PS_HEROPANEL_BACKGROUND_MARGIN 6
#define PS_HEROPANEL_CONTENT_MARGIN 12
#define PS_HEROPANEL_FRAME_TSID 1
#define PS_HEROPANEL_FRAME_TILEID_CORNER 0x70
#define PS_HEROPANEL_FRAME_TILEID_VERT 0x71
#define PS_HEROPANEL_FRAME_TILEID_HORZ 0x72

static int ps_heropanel_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata);

static int ps_heropanel_menucb_dismiss(struct ps_widget *button,void *userdata);
static int ps_heropanel_menucb_audio(struct ps_widget *button,void *userdata);
static int ps_heropanel_menucb_input(struct ps_widget *button,void *userdata);
static int ps_heropanel_menucb_quit(struct ps_widget *button,void *userdata);

static int ps_heropanel_cb_heroselect(struct ps_widget *heroselect,void *userdata);

static int ps_heropanel_update_icfg(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_heropanel {
  struct ps_widget hdr;
  struct ps_input_device *device;
  int watchid;
  struct ps_input_icfg *icfg;
  int clicked_in; // Nonzero if we've gotten some input off (device).
  int plrdefid,palette; // Default -1. If >=0, CHOOSING state is complete.
  int plrdefid_soft,palette_soft; // Suggested values, delivered live by heroselect, and used to restore.
  struct akgl_vtx_mintile *minvtxv;
  int minvtxa;
  int active;
};

#define WIDGET ((struct ps_widget_heropanel*)widget)

/* Delete.
 */

static void _ps_heropanel_del(struct ps_widget *widget) {
  if (WIDGET->watchid>=0) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
  }
  ps_input_device_del(WIDGET->device);
  ps_input_icfg_del(WIDGET->icfg);
  if (WIDGET->minvtxv) free(WIDGET->minvtxv);
}

/* Initialize.
 */

static int _ps_heropanel_init(struct ps_widget *widget) {

  WIDGET->watchid=-1;
  WIDGET->plrdefid=-1;
  WIDGET->palette=-1;
  WIDGET->plrdefid_soft=-1;
  WIDGET->palette_soft=0;

  struct ps_widget *child;
  if (!(child=ps_widget_label_spawn(widget,"",0))) return -1;
  child->fgrgba=PS_HEROPANEL_DEVICELABEL_COLOR;
  if (!(child=ps_widget_label_spawn(widget,"",0))) return -1;
  child->fgrgba=PS_HEROPANEL_MESSAGELABEL_COLOR;
  if (!(child=ps_widget_label_spawn(widget,"",0))) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_heropanel_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_heropanel) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_heropanel_get_devicelabel(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_heropanel_get_messagelabel(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_heropanel_get_content(const struct ps_widget *widget) { return widget->childv[2]; }

/* Grow vertex buffer.
 */

static int ps_heropanel_require_minvtxv(struct ps_widget *widget,int c) {
  if (c<=WIDGET->minvtxa) return 0;
  if (c>INT_MAX/sizeof(struct akgl_vtx_mintile)) return -1;
  void *nv=realloc(WIDGET->minvtxv,sizeof(struct akgl_vtx_mintile)*c);
  if (!nv) return -1;
  WIDGET->minvtxv=nv;
  WIDGET->minvtxa=c;
  return 0;
}

/* Draw background.
 */

static int ps_heropanel_draw_background(struct ps_widget *widget,int x,int y,int w,int h) {
  if (ps_video_draw_rect(x,y,w,h,widget->bgrgba)<0) return -1;
  return 0;
}

/* Draw frame.
 */

static int ps_heropanel_draw_frame(struct ps_widget *widget,int parentx,int parenty,int x,int y,int w,int h) {
  int vtxc_lr=(h+(PS_TILESIZE>>1))/PS_TILESIZE; // sic floor div; the corner boxes will occlude one tile.
  int vtxc_tb=(w+(PS_TILESIZE>>1))/PS_TILESIZE; // Add half of tile size because the corner tiles extend beyond (x,y,w,h)
  int vtxc=((vtxc_lr+vtxc_tb)<<1)+4;
  if (ps_heropanel_require_minvtxv(widget,vtxc)<0) return -1;
  struct akgl_vtx_mintile *vtx=WIDGET->minvtxv;
  int i,p;

  p=y+(h>>1)-((vtxc_lr*PS_TILESIZE)>>1)+(PS_TILESIZE>>1);
  for (i=vtxc_lr;i-->0;vtx+=2,p+=PS_TILESIZE) {
    vtx[0].x=x;
    vtx[0].y=p;
    vtx[0].tileid=PS_HEROPANEL_FRAME_TILEID_VERT;
    vtx[1].x=x+w;
    vtx[1].y=p;
    vtx[1].tileid=PS_HEROPANEL_FRAME_TILEID_VERT;
  }

  p=x+(w>>1)-((vtxc_tb*PS_TILESIZE)>>1)+(PS_TILESIZE>>1);
  for (i=vtxc_tb;i-->0;vtx+=2,p+=PS_TILESIZE) {
    vtx[0].x=p;
    vtx[0].y=y;
    vtx[0].tileid=PS_HEROPANEL_FRAME_TILEID_HORZ;
    vtx[1].x=p;
    vtx[1].y=y+h;
    vtx[1].tileid=PS_HEROPANEL_FRAME_TILEID_HORZ;
  }

  vtx->x=x;
  vtx->y=y;
  vtx->tileid=PS_HEROPANEL_FRAME_TILEID_CORNER;
  vtx++;
  vtx->x=x+w;
  vtx->y=y;
  vtx->tileid=PS_HEROPANEL_FRAME_TILEID_CORNER;
  vtx++;
  vtx->x=x;
  vtx->y=y+h;
  vtx->tileid=PS_HEROPANEL_FRAME_TILEID_CORNER;
  vtx++;
  vtx->x=x+w;
  vtx->y=y+h;
  vtx->tileid=PS_HEROPANEL_FRAME_TILEID_CORNER;
  vtx++;

  if (ps_video_draw_mintile(WIDGET->minvtxv,vtxc,PS_HEROPANEL_FRAME_TSID)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_heropanel_draw(struct ps_widget *widget,int parentx,int parenty) {
  int x=parentx+widget->x+PS_HEROPANEL_BACKGROUND_MARGIN;
  int y=parenty+widget->y+PS_HEROPANEL_BACKGROUND_MARGIN;
  int w=widget->w-(PS_HEROPANEL_BACKGROUND_MARGIN<<1);
  int h=widget->h-(PS_HEROPANEL_BACKGROUND_MARGIN<<1);
  if (ps_heropanel_draw_background(widget,x,y,w,h)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  if ((w>=PS_HEROPANEL_CONTENT_MARGIN<<1)&&(h>=PS_HEROPANEL_CONTENT_MARGIN<<1)) {
    if (ps_heropanel_draw_frame(widget,parentx,parenty,x,y,w,h)<0) return -1;
  }
  return 0;
}

/* Pack.
 */

static int _ps_heropanel_pack(struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  struct ps_widget *devicelabel=ps_heropanel_get_devicelabel(widget);
  struct ps_widget *messagelabel=ps_heropanel_get_messagelabel(widget);
  struct ps_widget *content=ps_heropanel_get_content(widget);

  int x=PS_HEROPANEL_CONTENT_MARGIN;
  int y=PS_HEROPANEL_CONTENT_MARGIN;
  int w=widget->w-(PS_HEROPANEL_CONTENT_MARGIN<<1);
  int h=widget->h-(PS_HEROPANEL_CONTENT_MARGIN<<1);
  if (w<0) {
    x=widget->w>>1;
    w=0;
  }
  if (h<0) {
    y=widget->h>>1;
    h=0;
  }
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,devicelabel,w,h)<0) return -1;
  devicelabel->x=x;
  devicelabel->y=y;
  devicelabel->w=w;
  devicelabel->h=chh;

  if (ps_widget_measure(&chw,&chh,messagelabel,w,h-devicelabel->h)<0) return -1;
  messagelabel->x=x;
  messagelabel->y=y+h-chh;
  messagelabel->w=w;
  messagelabel->h=chh;

  content->x=x;
  content->y=devicelabel->y+devicelabel->h;
  content->w=w;
  content->h=messagelabel->y-devicelabel->h-devicelabel->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_heropanel={

  .name="heropanel",
  .objlen=sizeof(struct ps_widget_heropanel),

  .del=_ps_heropanel_del,
  .init=_ps_heropanel_init,

  .draw=_ps_heropanel_draw,
  .pack=_ps_heropanel_pack,

};

/* Check the current state and react accordingly (eg changing background color).
 * This is called any time the state could have changed.
 * It is not knowable what the prior state was.
 */

static int ps_heropanel_react_to_state(struct ps_widget *widget) {
  uint32_t bgrgba=widget->bgrgba;
  int state=ps_heropanel_get_state(widget);
  if (state<0) {
    bgrgba=PS_HEROPANEL_BGCOLOR_ERROR;
  } else if (ps_heropanel_state_is_ready(state)) {
    bgrgba=PS_HEROPANEL_BGCOLOR_READY;
  } else if (ps_heropanel_state_is_pending(state)) {
    bgrgba=PS_HEROPANEL_BGCOLOR_PENDING;
  } else {
    bgrgba=PS_HEROPANEL_BGCOLOR_IDLE;
  }
  if (bgrgba!=widget->bgrgba) {
    struct ps_gui *gui=ps_widget_get_gui(widget);
    if (gui) {
      if (ps_gui_animate_property(gui,widget,PS_WIDGET_PROPERTY_bgrgba,bgrgba,PS_HEROPANEL_BGCOLOR_TIME)<0) return -1;
    } else {
      widget->bgrgba=bgrgba;
    }
  }
  return 0;
}

/* Get state.
 */
 
int ps_heropanel_get_state(const struct ps_widget *widget) {

  /* If we are not a heropanel, state is -1. */
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  struct ps_widget *content=ps_heropanel_get_content(widget);

  /* If the menu is open, state is MENU. */
  if (content->type==&ps_widget_type_menu) return PS_HEROPANEL_STATE_MENU;

  /* If input configuration is in progress, state is CONFIGURING. */
  if (WIDGET->icfg) return PS_HEROPANEL_STATE_CONFIGURING;

  /* If we have plrdefid and palette, state is READY. */
  if ((WIDGET->plrdefid>=0)&&(WIDGET->palette>=0)) {
    return PS_HEROPANEL_STATE_READY;
  }

  /* If our child is a heroselect, state is CHOOSING. */
  if (content->type==&ps_widget_type_heroselect) return PS_HEROPANEL_STATE_CHOOSING;

  /* If a device is attached, state is AVAILABLE. */
  if (WIDGET->device) return PS_HEROPANEL_STATE_AVAILABLE;

  /* Finally, any other state is INIT. */
  return PS_HEROPANEL_STATE_INIT;
}

/* State properties.
 */
 
int ps_heropanel_state_is_pending(int state) {
  if (state<0) return 0;
  if (state==PS_HEROPANEL_STATE_READY) return 0;
  if (state==PS_HEROPANEL_STATE_INIT) return 0;
  if (state==PS_HEROPANEL_STATE_AVAILABLE) return 0;
  return 1;
}

int ps_heropanel_state_is_ready(int state) {
  if (state==PS_HEROPANEL_STATE_READY) return 1;
  return 0;
}

/* Drop whatever content we have and create new content for AVAILABLE state.
 */

static int ps_heropanel_rebuild_AVAILABLE(struct ps_widget *widget) {

  struct ps_widget *content=ps_widget_spawn_replacement(widget,2,&ps_widget_type_label);
  if (!content) return -1;
  if (ps_widget_label_set_text(content,"Click in!",-1)<0) return -1;

  struct ps_widget *messagelabel=ps_heropanel_get_messagelabel(widget);
  if (ps_widget_label_set_text(messagelabel,"",0)<0) return -1;
  
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Drop whatever content we have and create new content for CHOOSING state.
 */

static int ps_heropanel_rebuild_CHOOSING(struct ps_widget *widget) {

  if (widget->childv[2]->type!=&ps_widget_type_heroselect) {
    struct ps_widget *content=ps_widget_spawn_replacement(widget,2,&ps_widget_type_heroselect);
    if (!content) return -1;
    if (WIDGET->plrdefid_soft<0) {
      WIDGET->plrdefid_soft=ps_widget_heropacker_select_plrdefid(widget->parent);
    }
    if (ps_widget_heroselect_set_plrdefid(content,WIDGET->plrdefid_soft)<0) return -1;
    if (ps_widget_heroselect_set_palette(content,WIDGET->palette_soft)<0) return -1;
    if (ps_widget_heroselect_set_callback(content,ps_callback(ps_heropanel_cb_heroselect,0,widget))<0) return -1;
  }
  
  struct ps_widget *messagelabel=ps_heropanel_get_messagelabel(widget);
  if (ps_widget_label_set_text(messagelabel,"",0)<0) return -1;
  
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Drop whatever content we have and create new content for READY state.
 */

static int ps_heropanel_rebuild_READY(struct ps_widget *widget) {

  if (widget->childv[2]->type!=&ps_widget_type_heroselect) {
    struct ps_widget *content=ps_widget_spawn_replacement(widget,2,&ps_widget_type_heroselect);
    if (!content) return -1;
    if (ps_widget_heroselect_set_plrdefid(content,WIDGET->plrdefid)<0) return -1;
    if (ps_widget_heroselect_set_palette(content,WIDGET->palette)<0) return -1;
  }

  struct ps_widget *messagelabel=ps_heropanel_get_messagelabel(widget);
  if (ps_widget_label_set_text(messagelabel,"Ready",-1)<0) return -1;
  
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Drop content and reset for input configuration.
 * This also creates and resets (WIDGET->icfg).
 */

static int ps_heropanel_rebuild_icfg(struct ps_widget *widget) {

  ps_input_icfg_del(WIDGET->icfg);
  if (!(WIDGET->icfg=ps_input_icfg_new(WIDGET->device))) return -1;

  struct ps_widget *content=ps_widget_spawn_replacement(widget,2,&ps_widget_type_textblock);
  if (!content) return -1;
  content->fgrgba=0xffff00ff;
  if (ps_widget_textblock_set_alignment(content,0,0)<0) return -1;

  if (ps_heropanel_update_icfg(widget)<0) return -1;

  return 0;
}

/* Enter menu.
 */

static int ps_heropanel_enter_menu(struct ps_widget *widget) {
  struct ps_widget *menu=ps_widget_spawn_replacement(widget,2,&ps_widget_type_menu);
  if (!menu) return -1;

  struct ps_widget *item;
  if (!(item=ps_widget_menu_spawn_button(menu,"Back",-1,ps_callback(ps_heropanel_menucb_dismiss,0,widget)))) return -1;
  if (!(item=ps_widget_menu_spawn_button(menu,"Audio",-1,ps_callback(ps_heropanel_menucb_audio,0,widget)))) return -1;
  if (!(item=ps_widget_menu_spawn_button(menu,"Input",-1,ps_callback(ps_heropanel_menucb_input,0,widget)))) return -1;
  if (!(item=ps_widget_menu_spawn_button(menu,"Quit",-1,ps_callback(ps_heropanel_menucb_quit,0,widget)))) return -1;
  
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Dismiss menu.
 */

static int ps_heropanel_dismiss_menu(struct ps_widget *widget) {
  if (!WIDGET->clicked_in) {
    if (ps_heropanel_rebuild_AVAILABLE(widget)<0) return -1;
  } else if (WIDGET->plrdefid<0) {
    if (ps_heropanel_rebuild_CHOOSING(widget)<0) return -1;
  } else {
    if (ps_heropanel_rebuild_READY(widget)<0) return -1;
  }
  return 0;
}

/* Click in.
 */

static int ps_heropanel_click_in(struct ps_widget *widget) {
  WIDGET->clicked_in=1;
  if (WIDGET->device->map) {
    if (ps_heropanel_rebuild_CHOOSING(widget)<0) return -1;
  } else {
    if (ps_heropanel_rebuild_icfg(widget)<0) return -1;
  }
  return 0;
}

/* Cancel click-in. Retreat from CHOOSING to AVAILABLE.
 */

static int ps_heropanel_cancel_click_in(struct ps_widget *widget) {
  WIDGET->clicked_in=0;
  if (ps_heropanel_rebuild_AVAILABLE(widget)<0) return -1;
  return 0;
}

/* Commit hero choice. Advance from CHOOSING to READY.
 */

static int ps_heropanel_commit_choice(struct ps_widget *widget) {
  struct ps_widget *heroselect=ps_heropanel_get_content(widget);
  if ((WIDGET->plrdefid=ps_widget_heroselect_get_plrdefid(heroselect))<0) return -1;
  if ((WIDGET->palette=ps_widget_heroselect_get_palette(heroselect))<0) return -1;
  if (ps_heropanel_rebuild_READY(widget)<0) return -1;
  return 0;
}

/* Uncommit hero choice. Retreat from READY to CHOOSING.
 */

static int ps_heropanel_uncommit_choice(struct ps_widget *widget) {
  WIDGET->plrdefid=-1;
  WIDGET->palette=-1;
  if (ps_heropanel_rebuild_CHOOSING(widget)<0) return -1;
  return 0;
}

/* Finalize or abort icfg.
 * Advance to CONFIGURING to CHOOSING or AVAILABLE.
 */

static int ps_heropanel_finalize_icfg(struct ps_widget *widget) {
  if (ps_input_icfg_is_ready(WIDGET->icfg)) {
    if (ps_input_icfg_finish(WIDGET->icfg,0)<0) return -1;
    if (ps_heropanel_rebuild_CHOOSING(widget)<0) return -1;
  } else {
    if (ps_heropanel_rebuild_AVAILABLE(widget)<0) return -1;
  }
  ps_input_icfg_del(WIDGET->icfg);
  WIDGET->icfg=0;
  return 0;
}

/* Update icfg, in CONFIGURING state.
 */

static int ps_heropanel_update_icfg(struct ps_widget *widget) {
  struct ps_widget *label=ps_heropanel_get_content(widget);
  if (!label||(label->type!=&ps_widget_type_textblock)) return -1;
  if (!WIDGET->icfg) return -1;

  int btnid=ps_input_icfg_get_current_button(WIDGET->icfg);
  int qualifier=ps_input_icfg_get_qualifier(WIDGET->icfg);

  char buf[64];
  int bufc=0;
  switch (qualifier) {
    case PS_ICFG_QUALIFIER_COLLECT: {
        bufc=snprintf(buf,sizeof(buf),"Press %s",ps_plrbtn_repr(btnid));
      } break;
    case PS_ICFG_QUALIFIER_REPEAT: {
        bufc=snprintf(buf,sizeof(buf),"Press %s again",ps_plrbtn_repr(btnid));
      } break;
    case PS_ICFG_QUALIFIER_MISMATCH: {
        bufc=snprintf(buf,sizeof(buf),"Fault!\nPress %s",ps_plrbtn_repr(btnid));
      } break;
    default: { // Something wrong with icfg, abort.
        if (ps_heropanel_finalize_icfg(widget)<0) return -1;
        return 0;
      }
  }
  if ((bufc<1)||(bufc>=sizeof(buf))) {
    return -1;
  }
  if (ps_widget_textblock_set_text(label,buf,bufc)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Test event for click in.
 */

static int ps_heropanel_event_is_click_in(const struct ps_widget *widget,const struct ps_input_device *device,int btnid,int value,int mapped) {
  if (!value) return 0; // Value must be nonzero.
  if (mapped) switch (btnid) {
    case PS_PLRBTN_A: return 1;
    case PS_PLRBTN_B: return 1;
    case PS_PLRBTN_START: return 1; // START will enter the menu, but if that logic is ever removed it will be a click-in button.
  } else {
    // Raw events are only considered if the device has no map.
    if (device->map) return 0;
    if (!device->premap) return 0;
    int p=ps_input_premap_search(device->premap,btnid);
    if (p<0) return 0;
    const struct ps_input_btncfg *btncfg=device->premap->btncfgv+p;
    if (btncfg->lo) return 0; // We only want button and button-like things.
    if (value<btncfg->hi>>1) return 0; // For analogue buttons, use a 50% threshold.
    return 1; // OK, looks like he's pressing a button.
  }
  return 0;
}

/* Input device callback.
 */
 
static int ps_heropanel_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;

  if (!WIDGET->active) return 0;

  int state=ps_heropanel_get_state(widget);

  //ps_log(GUI,DEBUG,"%s %s %d=%d, state=%d",__func__,mapped?"COOKED":"RAW",btnid,value,state);

  switch (state) {

    /* AVAILABLE: Pretty much any input event is a "click in". */
    case PS_HEROPANEL_STATE_AVAILABLE: {
        if (mapped&&value&&(btnid==PS_PLRBTN_START)) {
          if (ps_heropanel_enter_menu(widget)<0) return -1;
        } else if (ps_heropanel_event_is_click_in(widget,device,btnid,value,mapped)) {
          if (ps_heropanel_click_in(widget)<0) return -1;
        }
      } break;

    /* CHOOSING: Delegate most to heroselect. */
    case PS_HEROPANEL_STATE_CHOOSING: {
        if (!mapped) break;
        if (!value) break;
        switch (btnid) {
          case PS_PLRBTN_START: if (ps_heropanel_enter_menu(widget)<0) return -1; break;
          case PS_PLRBTN_A: if (ps_heropanel_commit_choice(widget)<0) return -1; break;
          case PS_PLRBTN_B: if (ps_heropanel_cancel_click_in(widget)<0) return -1; break;
          case PS_PLRBTN_LEFT: if (ps_widget_heroselect_change_plrdefid(ps_heropanel_get_content(widget),-1)<0) return -1; break;
          case PS_PLRBTN_RIGHT: if (ps_widget_heroselect_change_plrdefid(ps_heropanel_get_content(widget),1)<0) return -1; break;
          case PS_PLRBTN_UP: if (ps_widget_heroselect_change_palette(ps_heropanel_get_content(widget),-1)<0) return -1; break;
          case PS_PLRBTN_DOWN: if (ps_widget_heroselect_change_palette(ps_heropanel_get_content(widget),1)<0) return -1; break;
        }
      } break;

    /* READY: Cancel or enter menu. */
    case PS_HEROPANEL_STATE_READY: {
        if (!mapped) break;
        if (!value) break;
        switch (btnid) {
          case PS_PLRBTN_START: if (ps_heropanel_enter_menu(widget)<0) return -1; break;
          case PS_PLRBTN_B: if (ps_heropanel_uncommit_choice(widget)<0) return -1; break;
        }
      } break;

    /* MENU: Delegate most to menu. */
    case PS_HEROPANEL_STATE_MENU: {
        if (!mapped) break;
        if (!value) break;
        switch (btnid) {
          case PS_PLRBTN_START: if (ps_heropanel_dismiss_menu(widget)<0) return -1; break;
          case PS_PLRBTN_B: if (ps_heropanel_dismiss_menu(widget)<0) return -1; break;
          case PS_PLRBTN_A: if (ps_widget_menu_activate(ps_heropanel_get_content(widget))<0) return -1; break;
          case PS_PLRBTN_LEFT: if (ps_widget_menu_adjust_selection(ps_heropanel_get_content(widget),-1)<0) return -1; break;
          case PS_PLRBTN_RIGHT: if (ps_widget_menu_adjust_selection(ps_heropanel_get_content(widget),1)<0) return -1; break;
          case PS_PLRBTN_UP: if (ps_widget_menu_change_selection(ps_heropanel_get_content(widget),-1)<0) return -1; break;
          case PS_PLRBTN_DOWN: if (ps_widget_menu_change_selection(ps_heropanel_get_content(widget),1)<0) return -1; break;
        }
      } break;

    /* CONFIGURING: Filter events through (icfg). */
    case PS_HEROPANEL_STATE_CONFIGURING: {
        int err=ps_input_icfg_event(WIDGET->icfg,btnid,value);
        if (err<0) return -1;
        if (!err) {
          if (ps_heropanel_update_icfg(widget)<0) return -1;
        } else if (ps_input_icfg_is_ready(WIDGET->icfg)) {
          if (ps_heropanel_finalize_icfg(widget)<0) return -1;
        } else {
          if (ps_heropanel_update_icfg(widget)<0) return -1;
        }
      } break;

  }
  return 0;
}

/* Menu: Dismiss.
 */

static int ps_heropanel_menucb_dismiss(struct ps_widget *button,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_heropanel_dismiss_menu(widget)<0) return -1;
  return 0;
}

/* Menu: Input.
 */

static int ps_heropanel_menucb_input(struct ps_widget *button,void *userdata) {
  struct ps_widget *widget=userdata;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_inputcfg(gui)<0) return -1;
  return 0;
}

/* Menu: Audio.
 */

static int ps_heropanel_menucb_audio(struct ps_widget *button,void *userdata) {
  struct ps_widget *widget=userdata;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_audiocfg(gui)<0) return -1;
  return 0;
}

/* Menu: Quit.
 */

static int ps_heropanel_menucb_quit(struct ps_widget *button,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

/* Callback from heroselect.
 */

static int ps_heropanel_cb_heroselect(struct ps_widget *heroselect,void *userdata) {
  struct ps_widget *widget=userdata;
  WIDGET->plrdefid_soft=ps_widget_heroselect_get_plrdefid(heroselect);
  WIDGET->palette_soft=ps_widget_heroselect_get_palette(heroselect);
  return 0;
}

/* Input device.
 */
 
int ps_widget_heropanel_set_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;

  /* Retain the device, replace the old one. */
  if (WIDGET->watchid>=0) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
    WIDGET->watchid=-1;
  }
  if (device&&(ps_input_device_ref(device)<0)) return -1;
  ps_input_device_del(WIDGET->device);
  WIDGET->device=device;
  if (device) {
    if ((WIDGET->watchid=ps_input_device_watch_buttons(WIDGET->device,ps_heropanel_cb_button,0,widget))<0) return -1;
  }

  /* Replace content of namelabel. */
  if (device) {
    const char *name=device->name;
    int namec=device->namec;
    if (!name||(namec<1)) {
      name="Device";
      namec=6;
    } else if (namec>PS_HEROPANEL_DEVICE_NAME_LIMIT) {
      namec=PS_HEROPANEL_DEVICE_NAME_LIMIT;
      while (namec&&((unsigned char)name[namec-1]<=0x20)) namec--;
    }
    if (ps_widget_label_set_text(ps_heropanel_get_devicelabel(widget),name,namec)<0) return -1;
  } else {
    if (ps_widget_label_set_text(ps_heropanel_get_devicelabel(widget),"",0)<0) return -1;
  }

  /* If a device is present, rebuild for AVAILABLE state. */
  if (device) {
    if (ps_heropanel_rebuild_AVAILABLE(widget)<0) return -1;
  }

  /* Update background color if necessary. */
  if (ps_heropanel_react_to_state(widget)<0) return -1;

  return 0;
}

struct ps_input_device *ps_widget_heropanel_get_device(const struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return 0;
  return WIDGET->device;
}

/* Simple accessors.
 */

int ps_widget_heropanel_get_plrdefid(const struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  if (WIDGET->plrdefid>=0) return WIDGET->plrdefid;
  return WIDGET->plrdefid_soft;
}

int ps_widget_heropanel_get_palette(const struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  if (WIDGET->palette>=0) return WIDGET->palette;
  return WIDGET->palette_soft;
}

int ps_widget_heropanel_is_pending(const struct ps_widget *widget){
  return ps_heropanel_state_is_pending(ps_heropanel_get_state(widget));
}

int ps_widget_heropanel_is_ready(const struct ps_widget *widget) {
  return ps_heropanel_state_is_ready(ps_heropanel_get_state(widget));
}

int ps_widget_heropanel_activate(struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  WIDGET->active=1;
  return 0;
}

int ps_widget_heropanel_deactivate(struct ps_widget *widget) {
  if (ps_heropanel_obj_validate(widget)<0) return -1;
  WIDGET->active=0;
  return 0;
}
