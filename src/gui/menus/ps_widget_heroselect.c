/* ps_widget_heroselect.c
 * Single panel in assemble page, associated with one input device.
 * Children are volatile, rebuilt on phase changes.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "video/ps_video.h"
#include "os/ps_clockassist.h"
#include "game/ps_sound_effects.h"
#include "util/ps_enums.h"
#include "akgl/akgl.h"

#define PS_HEROSELECT_SPRDEF_ID 1

#define PS_HEROSELECT_TOO_MANY_PLAYERS_WARNING_TTL 120

/* Object definition.
 */

struct ps_widget_heroselect {
  struct ps_widget hdr;
  int plrdefid;
  int palette;
  int64_t too_many_players_warning; // Timestamp when warning was issued.
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_heroselect*)widget)

/* Delete.
 */

static void _ps_heroselect_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_heroselect_init(struct ps_widget *widget) {

  WIDGET->plrdefid=1;
  WIDGET->palette=0;
  
  struct ps_widget *sprite=ps_widget_spawn(widget,&ps_widget_type_sprite);
  if (!sprite) return -1;
  if (ps_widget_sprite_load_sprdef(sprite,PS_HEROSELECT_SPRDEF_ID)<0) return -1;
  if (ps_widget_sprite_set_plrdefid(sprite,WIDGET->plrdefid)<0) return -1;
  if (ps_widget_sprite_set_palette(sprite,WIDGET->palette)<0) return -1;
  WIDGET->plrdefid=ps_widget_sprite_get_plrdefid(sprite); // May have been modified by sprite widget.
  WIDGET->palette=ps_widget_sprite_get_palette(sprite); // "


  return 0;
}

/* Structural helpers.
 */

static int ps_heroselect_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_heroselect) return -1;
  if (widget->childc!=1) return -1;
  return 0;
}

static struct ps_widget *ps_heroselect_get_sprite(const struct ps_widget *widget) { return widget->childv[0]; }

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_heroselect={

  .name="heroselect",
  .objlen=sizeof(struct ps_widget_heroselect),

  .del=_ps_heroselect_del,
  .init=_ps_heroselect_init,

};

/* Accessors.
 */

static int ps_heroselect_modify(struct ps_widget *widget,int dx,int dy) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  struct ps_widget *sprite=ps_heroselect_get_sprite(widget);
  if (dx) {
    if (ps_widget_sprite_modify_plrdefid(sprite,dx)<0) return -1;
    WIDGET->plrdefid=ps_widget_sprite_get_plrdefid(sprite);
    WIDGET->palette=ps_widget_sprite_get_palette(sprite); // Changing plrdefid can also change palette
  }
  if (dy) {
    if (ps_widget_sprite_modify_palette(sprite,dy)<0) return -1;
    WIDGET->palette=ps_widget_sprite_get_palette(sprite);
  }
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  return 0;
}

int ps_widget_heroselect_set_plrdefid(struct ps_widget *widget,int plrdefid) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  struct ps_widget *sprite=ps_heroselect_get_sprite(widget);
  if (ps_widget_sprite_set_plrdefid(sprite,plrdefid)<0) return -1;
  WIDGET->plrdefid=ps_widget_sprite_get_plrdefid(sprite);
  WIDGET->palette=ps_widget_sprite_get_palette(sprite);
  return 0;
}

int ps_widget_heroselect_get_plrdefid(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  return WIDGET->plrdefid;
}

int ps_widget_heroselect_set_palette(struct ps_widget *widget,int palette) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  struct ps_widget *sprite=ps_heroselect_get_sprite(widget);
  if (ps_widget_sprite_set_palette(sprite,palette)<0) return -1;
  WIDGET->palette=ps_widget_sprite_get_palette(sprite);
  return 0;
}

int ps_widget_heroselect_get_palette(const struct ps_widget *widget) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  return WIDGET->palette;
}

int ps_widget_heroselect_change_plrdefid(struct ps_widget *widget,int d) {
  return ps_heroselect_modify(widget,d,0);
}

int ps_widget_heroselect_change_palette(struct ps_widget *widget,int d) {
  return ps_heroselect_modify(widget,0,d);
}

int ps_widget_heroselect_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (ps_heroselect_obj_validate(widget)<0) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}
