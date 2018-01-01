/* ps_game.h
 * This object represents the entire logical model of a running game.
 */

#ifndef PS_GAME_H
#define PS_GAME_H

#include "ps_sprite.h"

struct ps_scenario;
struct ps_player;
struct ps_grid;
struct ps_video_layer;
struct ps_physics;
struct ps_input_device;

/* Global sprite groups. */
#define PS_SPRGRP_KEEPALIVE        0 /* All active sprites belong to this group. */
#define PS_SPRGRP_DEATHROW         1 /* Kill all these after each update. */
#define PS_SPRGRP_VISIBLE          2 /* Draw these each frame. */
#define PS_SPRGRP_UPDATE           3 /* Update these each frame. */
#define PS_SPRGRP_PHYSICS          4 /* Everything that participates in physics. */
#define PS_SPRGRP_HERO             5 /* Sprites controlled directly by a player. */
#define PS_SPRGRP_HAZARD           6 /* Sprites that can inflict damage. */
#define PS_SPRGRP_HEROHAZARD       7 /* Can inflict damage to heroes only. */
#define PS_SPRGRP_FRAGILE          8 /* Sprites that can suffer damage. */
#define PS_SPRGRP_TREASURE         9 /* Anything the heroes can pick up. */
#define PS_SPRGRP_LATCH           10 /* Can be picked up by hookshot. */
#define PS_SPRGRP_SOLID           11 /* Sprites that PHYSICS sprites can't pass through. */
#define PS_SPRGRP_PRIZE           12 /* Hearts for the heroes to pick up. */
#define PS_SPRGRP_COUNT           13

struct ps_game {

  struct ps_scenario *scenario;
  struct ps_player *playerv[PS_PLAYER_LIMIT];
  int playerc;
  int difficulty;
  int length;
  int treasurev[PS_TREASURE_LIMIT];
  int treasurec;
  int playtime; // Count of updates since reset.
  
  struct ps_sprgrp grpv[PS_SPRGRP_COUNT];
  struct ps_grid *grid; // WEAK
  int gridx,gridy; // Grid's position in world.
  struct ps_physics *physics;

  struct ps_video_layer *layer_scene;

  int finished; // Signal to owner that you should drop this and return to the menus.
  int paused;
  
};

struct ps_game *ps_game_new();
void ps_game_del(struct ps_game *game);

/* The setup process.
 * First set the player count.
 * Then set skills for each player.
 * Then set difficulty and length. (or before players, whatever).
 * Finally, ps_game_generate() to commit config and generate the scenario.
 */
int ps_game_set_player_count(struct ps_game *game,int playerc);
int ps_game_configure_player(struct ps_game *game,int playerid,int plrdefid,int palette,struct ps_input_device *device);
int ps_game_set_difficulty(struct ps_game *game,int difficulty);
int ps_game_set_length(struct ps_game *game,int length);
int ps_game_generate(struct ps_game *game);

/* XXX These functions were used by earlier setup UI, but probably not needed anymore.
 */
int ps_game_eliminate_player(struct ps_game *game,int playerid); // Removes a player and drops the playerid of any above it; IDs must be contiguous
int ps_game_set_player_definition(struct ps_game *game,int playerid,int plrdefid);
int ps_game_adjust_player_definition(struct ps_game *game,int playerid,int d);
int ps_game_adjust_player_palette(struct ps_game *game,int playerid,int d);

/* For a test scenario, follow the same process but use this instead of ps_game_generate().
 * We will generate a scenario with one explicit region and only the blueprint IDs you provide.
 */
int _ps_game_generate_test(struct ps_game *game,int regionid,int blueprintid,...);
#define ps_game_generate_test(game,regionid,...) _ps_game_generate_test(game,regionid,##__VA_ARGS__,-1)

/* With a scenario generated, call this to reset the transient state and begin play.
 */
int ps_game_restart(struct ps_game *game);

/* During play, call this to forcibly return to the first screen.
 * This is an option in the pause menu, for use when players end up in an impossible place.
 */
int ps_game_return_to_start_screen(struct ps_game *game);

int ps_game_update(struct ps_game *game);

int ps_game_toggle_pause(struct ps_game *game);

int ps_game_collect_treasure(struct ps_game *game,struct ps_sprite *collector,int treasureid);
int ps_game_get_treasure_state(const struct ps_game *game,int treasureid); // 0=uncollected, 1=collected
int ps_game_count_collected_treasures(const struct ps_game *game);

/* These can be nice for troubleshooting.
 * In particular, I'm adding them because I want to see the blueprint id for the current grid.
 */
struct ps_screen *ps_game_get_screen_for_current_grid(const struct ps_game *game);
int ps_game_get_current_blueprint_id(const struct ps_game *game);
int ps_game_force_next_screen(struct ps_game *game,int dx,int dy);

int ps_game_create_fireworks(struct ps_game *game,int x,int y);
int ps_game_create_prize(struct ps_game *game,int x,int y);
int ps_game_check_deathgate(struct ps_game *game);

// For test/debug builds, restore all heroes to life.
int ps_game_heal_all_heroes(struct ps_game *game);

/* ===== Serial Format =====
 *  0000   8 Signature: "\0PLSQD\n\xff"
 *  0008   4 Game Serial Version: 1
 *  000c   4 Reserved for resources version or checksum.
 *  0010   1 Player count.
 *  0011   1 Difficulty.
 *  0012   1 Length.
 *  0013   1 unused
 *  0014   4 Treasure state, bitmap. treasurev[0]==0x00000001 etc
 *  0018   4 Play time.
 *  001c   2 Selected grid (x,y). (XXX not used; we restart at the home screen)
 *  001e ... Players:
 *    0000   2 plrdef id
 *    0002   1 Palette
 *    0003   1 unused
 *    0004
 *  ....   4 Scenario compressed size.
 *  ...4   4 Scenario uncompressed size.
 *  ...8 ... Scenario.
 */
int ps_game_encode(void *dstpp,const struct ps_game *game);
int ps_game_decode(struct ps_game *game,const void *src,int srcc);

#endif
