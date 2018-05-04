/* ps_sprite_hero.h
 */

#ifndef PS_SPRITE_HERO_H
#define PS_SPRITE_HERO_H

#include "game/ps_sprite.h"

/* The default state is zero: living, standing still, nothing much happening.
 */
#define PS_HERO_STATE_GHOST           0x00000001 /* Dead, ghost. */
#define PS_HERO_STATE_PUMPKIN         0x00000002 /* Being carried by someone else's hookshot. */
#define PS_HERO_STATE_FERRY           0x00000004 /* Riding a turtle. */
#define PS_HERO_STATE_OFFSCREEN       0x00000008 /* Invisible, waiting off one edge of the screen. */
#define PS_HERO_STATE_WALK            0x00000010 /* Walking or trying to. */
#define PS_HERO_STATE_FLY             0x00000020 /* Flying, as a bat. */
#define PS_HERO_STATE_SWORD           0x00000040 /* Swinging my sword. */
#define PS_HERO_STATE_HURT            0x00000080 /* Invincible due to damage (max HP is one, so this doesn't actually happen) */
#define PS_HERO_STATE_HEAL            0x00000100 /* Invincible due to recent healing. */
#define PS_HERO_STATE_HIGHLIGHT       0x00000200 /* Displaying other transient highlight. (probably just for debug) */
#define PS_HERO_STATE_STOPINPUT       0x00000400 /* Ignore all input until the source goes zero. */
#define PS_HERO_STATE_HOOKSHOT        0x00000800 /* Using hookshot. */

#define PS_HERO_HURT_TIME 60
#define PS_HERO_HEAL_TIME 85
#define PS_HERO_HIGHLIGHT_TIME 60
#define PS_HERO_DEFAULT_HP 1

struct ps_player;

struct ps_sprite_hero {
  struct ps_sprite hdr;

  /* Superficial appearance properties. */
  int blinktime; // Counts down
  int walktime; // Counts up, reset to zero when walk status changes
  int hurttime; // Counts down, impervious while nonzero.
  int healtime; // Counts down after healing.
  int highlighttime; // Counts down during general-purpose highlight.
  uint8_t dragoncharge; // Highlight when ghost charges into dragonhood.

  struct ps_player *player;
  uint16_t input;
  int ignore_input;
  int reexamine_dpad; // Set during input processing to force dpad state to zero, to reexamine next time.
  int indx,indy; // Requested movement, ie joystick position. (-1,0,1)
  int facedir; // PS_DIRECTION_*, which direction to render
  int hp;

  /* Actions state. */
  int fly_counter;
  int defer_fly_end;

  /* Late update: Trying to coalesce a bunch of important properties into one uniform "state" mask.
   * It's fine to read state directly but modify it ony with the provided accessors.
   */
  uint32_t state;
  int lethal; // Only relevant in PS_HERO_STATE_GHOST. Am I in PS_SPRGRP_HEROHAZARD?
  
};

int ps_hero_set_player(struct ps_sprite *spr,struct ps_player *player);
int ps_hero_get_playerid(const struct ps_sprite *spr);

int ps_hero_action_begin(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_action_end(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_action_continue(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_begin(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_end(struct ps_sprite *spr,struct ps_game *game);
int ps_hero_auxaction_continue(struct ps_sprite *spr,struct ps_game *game);

int ps_hero_get_principal_action(const struct ps_sprite *spr);
int ps_hero_get_auxiliary_action(const struct ps_sprite *spr);

/* Heroes by default accept input from the core.
 * Calling this sends explicit input state, and also permanently disables reading from the input core.
 * assemblepage uses this.
 */
int ps_hero_accept_fake_input(struct ps_sprite *spr,uint16_t input,struct ps_game *game);

int ps_hero_set_dragon_charge(struct ps_sprite *spr,int p,int c);

/* Hero state changes return:
 *   -1 for real errors
 *    0 if the state change is not possible due to current state or whatever
 *    1 if redundant
 *    2 if changed
 * You must provide a single valid state flag at a time.
 * It is common for state changes to have heavy side effects, eg adding HEAL state brings it back to life.
 */
int ps_hero_add_state(struct ps_sprite *spr,uint32_t incoming,struct ps_game *game);
int ps_hero_remove_state(struct ps_sprite *spr,uint32_t outgoing,struct ps_game *game);

/* State queries.
 * Return 0 or 1 (no errors), answer the given question by examining state.
 */
int ps_hero_stateq_can_change_direction(const struct ps_sprite *spr);
int ps_hero_stateq_can_walk(const struct ps_sprite *spr);

// Hurt hero even if he is invincible.
int ps_hero_force_kill(struct ps_sprite *spr,struct ps_game *game);

/* Actions mostly correspond to skills. */
#define PS_HERO_ACTION_NONE         0
#define PS_HERO_ACTION_SWORD        1
#define PS_HERO_ACTION_ARROW        2
#define PS_HERO_ACTION_HOOKSHOT     3
#define PS_HERO_ACTION_FLAME        4
#define PS_HERO_ACTION_HEAL         5
#define PS_HERO_ACTION_BOMB         6
#define PS_HERO_ACTION_FLY          7
#define PS_HERO_ACTION_MARTYR       8

#endif
