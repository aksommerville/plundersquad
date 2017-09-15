/* ps_sprite_hero.h
 */

#ifndef PS_SPRITE_HERO_H
#define PS_SPRITE_HERO_H

#include "game/ps_sprite.h"

struct ps_player;

struct ps_sprite_hero {
  struct ps_sprite hdr;

  /* Superficial appearance properties. */
  uint32_t rgba_body; // Clothing
  uint32_t rgba_head; // Hair
  int blinktime; // Counts down
  int walktime; // Counts up, reset to zero when walk status changes
  int hurttime; // Counts down, impervious while nonzero.
  int healtime; // Counts down after healing.

  struct ps_player *player;
  uint16_t input;
  int reexamine_dpad; // Set during input processing to force dpad state to zero, to reexamine next time.
  int indx,indy; // Requested movement, ie joystick position. (-1,0,1)
  int facedir; // PS_DIRECTION_*, which direction to render
  int hp;

  /* Actions state. */
  int walk_in_progress;
  int sword_in_progress;
  int hookshot_in_progress;
  int flame_in_progress;
  int flame_counter;
  int fly_in_progress;
  int fly_counter;
  int carry_in_progress;
  
};

int ps_hero_set_player(struct ps_sprite *spr,struct ps_player *player);

int ps_hero_become_ghost(struct ps_game *game,struct ps_sprite *spr);
int ps_hero_become_living(struct ps_game *game,struct ps_sprite *spr);
int ps_hero_heal(struct ps_sprite *spr,struct ps_game *game);

int ps_hero_action_begin(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_action_end(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_action_continue(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_begin(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_end(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_continue(struct ps_sprite *spr,struct ps_game *game);

int ps_hero_get_principal_action(const struct ps_sprite *spr);
int ps_hero_get_auxiliary_action(const struct ps_sprite *spr);

int ps_hero_abort_hookshot(struct ps_sprite *spr,struct ps_game *game);

/* Actions mostly correspond to skills. */
#define PS_HERO_ACTION_NONE         0
#define PS_HERO_ACTION_SWORD        1
#define PS_HERO_ACTION_ARROW        2
#define PS_HERO_ACTION_HOOKSHOT     3
#define PS_HERO_ACTION_FLAME        4
#define PS_HERO_ACTION_HEAL         5
#define PS_HERO_ACTION_CARRY        6
#define PS_HERO_ACTION_FLY          7
#define PS_HERO_ACTION_MARTYR       8
#define PS_HERO_ACTION_FROG         9

#define PS_HERO_FLAMES_RAMP_UP_TIME   120
#define PS_HERO_FLAMES_ORBIT_TIME      60
#define PS_HERO_FLAMES_ORBIT_DISTANCE PS_TILESIZE
#define PS_HERO_FLAMES_MARGIN 4.0 /* Physical bounds beyond orbit */

#endif
