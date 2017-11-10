#include "ps.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_sprite.h"
#include "game/sprites/ps_sprite_hero.h"
#include "res/ps_resmgr.h"
#include "video/ps_video.h"

#define PS_HEROSETUP_DEFAULT_DEVICE_NAME "(unknown device)"
#define PS_HEROSETUP_DEVICE_NAME_LIMIT 20
#define PS_HEROSETUP_CLICK_IN_MESSAGE "Press button"

#define PS_HEROSETUP_BACKGROUND_COLOR_INIT    0x607080c0
#define PS_HEROSETUP_BACKGROUND_COLOR_EDIT    0x806040c0
#define PS_HEROSETUP_BACKGROUND_COLOR_READY   0x00000000
#define PS_HEROSETUP_TEXT_COLOR               0xffffe0ff

/* Widget definition.
 */

struct ps_widget_herosetup {
  struct ps_widget hdr;
  struct ps_input_device *device;
  int watchid;
  int phase;
  int playerid;
  struct ps_sprgrp *grp;
};

#define WIDGET ((struct ps_widget_herosetup*)widget)

/* Delete.
 */

static void _ps_herosetup_del(struct ps_widget *widget) {
  ps_sprgrp_kill(WIDGET->grp);
  ps_sprgrp_del(WIDGET->grp);
  ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
  ps_input_device_del(WIDGET->device);
}

/* Initialize.
 */

static int _ps_herosetup_init(struct ps_widget *widget) {

  WIDGET->phase=PS_HEROSETUP_PHASE_INIT;

  widget->bgrgba=PS_HEROSETUP_BACKGROUND_COLOR_INIT;

  struct ps_widget *label=ps_widget_spawn_label(widget,PS_HEROSETUP_DEFAULT_DEVICE_NAME,sizeof(PS_HEROSETUP_DEFAULT_DEVICE_NAME)-1,PS_HEROSETUP_TEXT_COLOR);
  if (!label) return -1;

  if (!(WIDGET->grp=ps_sprgrp_new())) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_herosetup_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  

  if (WIDGET->grp->sprc>0) {
    struct ps_sprite *spr=WIDGET->grp->sprv[0];
    spr->x=x0+widget->x+(widget->w>>1);
    spr->y=y0+widget->y+(widget->h>>1);
    if (ps_video_draw_sprites(WIDGET->grp)<0) return -1;
  }
  
  return 0;
}

/* Measure.
 */

static int _ps_herosetup_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack for INIT phase.
 * Expect one child: the click-in message
 */

static int ps_herosetup_pack_init(struct ps_widget *widget,int dstx,int dsty,int dstw,int dsth,struct ps_widget **childv,int childc) {
  if (childc!=1) return -1;

  childv[0]->x=dstx;
  childv[0]->y=dsty;
  childv[0]->w=dstw;
  childv[0]->h=dsth;
  if (ps_widget_pack(childv[0])<0) return -1;
  
  return 0;
}

/* Pack for EDIT phase.
 */

static int ps_herosetup_pack_edit(struct ps_widget *widget,int dstx,int dsty,int dstw,int dsth,struct ps_widget **childv,int childc) {
  return 0;
}

/* Pack for READY phase.
 */

static int ps_herosetup_pack_ready(struct ps_widget *widget,int dstx,int dsty,int dstw,int dsth,struct ps_widget **childv,int childc) {
  return 0;
}

/* Pack, shared and dispatch.
 */

static int _ps_herosetup_pack(struct ps_widget *widget) {
  if (widget->childc<1) return 0;

  /* childv[0] is always the device label. It gets centered at the very top. */
  int labelh;
  if (ps_widget_measure(0,&labelh,widget->childv[0],widget->w,widget->h)<0) return -1;
  widget->childv[0]->x=0;
  widget->childv[0]->y=0;
  widget->childv[0]->w=widget->w;
  widget->childv[0]->h=labelh;
  if (ps_widget_pack(widget->childv[0])<0) return -1;

  /* Determine the remaining space. */
  int dstx=0;
  int dsty=labelh;
  int dstw=widget->w;
  int dsth=widget->h-labelh;
  struct ps_widget **childv=widget->childv+1;
  int childc=widget->childc-1;

  /* Dispatch based on flow phase. */
  switch (WIDGET->phase) {
    case PS_HEROSETUP_PHASE_INIT: return ps_herosetup_pack_init(widget,dstx,dsty,dstw,dsth,childv,childc);
    case PS_HEROSETUP_PHASE_EDIT: return ps_herosetup_pack_edit(widget,dstx,dsty,dstw,dsth,childv,childc);
    case PS_HEROSETUP_PHASE_READY: return ps_herosetup_pack_ready(widget,dstx,dsty,dstw,dsth,childv,childc);
  }

  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_herosetup={
  .name="herosetup",
  .objlen=sizeof(struct ps_widget_herosetup),
  .del=_ps_herosetup_del,
  .init=_ps_herosetup_init,
  .draw=_ps_herosetup_draw,
  .measure=_ps_herosetup_measure,
  .pack=_ps_herosetup_pack,
};

/* Button pressed on device.
 */

static int ps_herosetup_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;

  // Discard raw events. Maybe we'll use these in the future, for input configuration. Maybe not here though.
  if (!mapped) return 0;

  // We are dumb: Just pass events to our parent, and let them figure out what to do with it.
  if (ps_widget_heroselect_receive_player_input(widget->parent,widget,btnid,value)<0) return -1;
  
  return 0;
}

/* Accessors.
 */
 
int ps_widget_herosetup_set_input_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  if (WIDGET->device==device) return 0;

  /* Drop the previous device. */
  ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
  ps_input_device_del(WIDGET->device);
  WIDGET->device=0;
  if (!device) return 0;

  /* Store the new device. */
  if (device&&(ps_input_device_ref(device)<0)) return -1;
  WIDGET->device=device;

  /* Watch buttons. */
  if ((WIDGET->watchid=ps_input_device_watch_buttons(device,ps_herosetup_cb_button,0,widget))<0) return -1;

  /* Rewrite title label. */
  if (widget->childc>=1) {
    struct ps_widget *label=widget->childv[0];
    if (device->namec<1) {
      if (ps_widget_label_set_text(label,PS_HEROSETUP_DEFAULT_DEVICE_NAME,-1)<0) return -1;
    } else {
      int namec=device->namec;
      if (namec>PS_HEROSETUP_DEVICE_NAME_LIMIT) namec=PS_HEROSETUP_DEVICE_NAME_LIMIT;
      if (ps_widget_label_set_text(label,device->name,namec)<0) return -1;
    }
  }
  
  return 0;
}

struct ps_input_device *ps_widget_herosetup_get_input_device(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return 0;
  return WIDGET->device;
}

int ps_widget_herosetup_set_playerid(struct ps_widget *widget,int playerid) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  if ((playerid<0)||(playerid>PS_PLAYER_LIMIT)) return -1;
  WIDGET->playerid=playerid;
  return 0;
}

int ps_widget_herosetup_get_playerid(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return 0;
  return WIDGET->playerid;
}

int ps_widget_herosetup_get_phase(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  return WIDGET->phase;
}

/* Drop all children except the first.
 */

static int ps_herosetup_drop_phase_children(struct ps_widget *widget) {
  while (widget->childc>1) {
    if (ps_widget_remove_child(widget,widget->childv[widget->childc-1])<0) return -1;
  }
  return 0;
}

/* Enter INIT phase.
 * NB: INIT is the default phase, and this function is *not* called initially.
 */

static int ps_herosetup_enter_init(struct ps_widget *widget) {
  ps_log(GUI,TRACE,"%s %p",__func__,widget);
  struct ps_gui *gui=ps_widget_get_gui(widget);
  
  if (ps_widget_heroselect_free_player(widget->parent,WIDGET->playerid)<0) return -1;
  if (ps_herosetup_drop_phase_children(widget)<0) return -1;
  WIDGET->playerid=0;
  WIDGET->phase=PS_HEROSETUP_PHASE_INIT;

  widget->bgrgba=PS_HEROSETUP_BACKGROUND_COLOR_INIT;

  struct ps_widget *label=ps_widget_spawn_label(widget,PS_HEROSETUP_CLICK_IN_MESSAGE,-1,PS_HEROSETUP_TEXT_COLOR);
  if (!label) return -1;
  if (ps_gui_animate_property(gui,label,PS_GUI_PROPERTY_fgrgba,PS_HEROSETUP_TEXT_COLOR,PS_HEROSETUP_TEXT_COLOR&0xffffff00,40)<0) return -1;

  ps_sprgrp_kill(WIDGET->grp);

  if (ps_widget_pack(widget)<0) return -1;
  
  return 0;
}

/* Enter EDIT phase.
 */

static int ps_herosetup_enter_edit(struct ps_widget *widget) {
  ps_log(GUI,TRACE,"%s %p",__func__,widget);
  
  int playerid=ps_widget_heroselect_allocate_player(widget->parent);
  if (playerid<1) return -1;
  if (ps_herosetup_drop_phase_children(widget)<0) return -1;
  WIDGET->playerid=playerid;
  WIDGET->phase=PS_HEROSETUP_PHASE_EDIT;

  widget->bgrgba=PS_HEROSETUP_BACKGROUND_COLOR_EDIT;

  ps_sprgrp_kill(WIDGET->grp);

  if (ps_widget_pack(widget)<0) return -1;

  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (!game) return -1;
  struct ps_player *player=game->playerv[WIDGET->playerid-1];
  //if (ps_game_set_player_definition(game,WIDGET->playerid,1)<0) return -1;
  if (ps_widget_herosetup_refresh_player(widget,player)<0) return -1;
  
  return 0;
}

/* Enter READY phase.
 */

static int ps_herosetup_enter_ready(struct ps_widget *widget) {
  ps_log(GUI,TRACE,"%s %p %d",__func__,widget,WIDGET->playerid);
  
  if (ps_herosetup_drop_phase_children(widget)<0) return -1;
  WIDGET->phase=PS_HEROSETUP_PHASE_READY;

  widget->bgrgba=PS_HEROSETUP_BACKGROUND_COLOR_READY;

  ps_sprgrp_kill(WIDGET->grp);

  if (ps_widget_pack(widget)<0) return -1;

  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (!game) return -1;
  struct ps_player *player=game->playerv[WIDGET->playerid-1];
  if (ps_widget_herosetup_refresh_player(widget,player)<0) return -1;
  
  return 0;
}

/* Advance phase.
 */

int ps_widget_herosetup_advance_phase(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  switch (WIDGET->phase) {
    case PS_HEROSETUP_PHASE_INIT: return ps_herosetup_enter_edit(widget);
    case PS_HEROSETUP_PHASE_EDIT: return ps_herosetup_enter_ready(widget);
  }
  return 0;
}

/* Retreat phase.
 */

int ps_widget_herosetup_retreat_phase(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  switch (WIDGET->phase) {
    case PS_HEROSETUP_PHASE_INIT: return ps_herosetup_enter_init(widget);
    case PS_HEROSETUP_PHASE_EDIT: return ps_herosetup_enter_init(widget);
    case PS_HEROSETUP_PHASE_READY: return ps_herosetup_enter_edit(widget);
  }
  return 0;
}

/* Refresh player, eg after palette or definition changed.
 */
 
int ps_widget_herosetup_refresh_player(struct ps_widget *widget,struct ps_player *player) {
  if (!widget||(widget->type!=&ps_widget_type_herosetup)) return -1;
  if (WIDGET->phase<PS_HEROSETUP_PHASE_EDIT) return -1;
  if (!player) return -1;
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));

  ps_log(GUI,DEBUG,"%s %p:%d",__func__,player->plrdef,player->palette);

  ps_sprgrp_kill(WIDGET->grp);

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,1);
  if (!sprdef) return -1;
  int argv[]={WIDGET->playerid};
  struct ps_sprite *sprite=ps_sprdef_instantiate(game,sprdef,argv,1,100,100);//TODO position
  if (!sprite) return -1;
  if (ps_hero_set_player(sprite,player)<0) return -1;
  if (ps_sprgrp_add_sprite(WIDGET->grp,sprite)<0) return -1;

  return 0;
}
