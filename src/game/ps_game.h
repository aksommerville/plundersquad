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
struct ps_stats;
struct ps_path;
struct ps_bloodhound_activator;
struct ps_statusreport;
struct ps_dragoncharger;
struct ps_res_trdef;
struct ps_summoner;
struct ps_switchboard;
struct ps_gamelog;
struct ps_score_store;

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
#define PS_SPRGRP_BARRIER         13 /* Can interact with switches. */
#define PS_SPRGRP_TELEPORT        14 /* Can travel through teleporter. */
#define PS_SPRGRP_HEROONLYHACK    15 /* For game's internal use, please ignore. */
#define PS_SPRGRP_SWORDAWARE      16 /* Reacts to sword swipe (and only sword). */
#define PS_SPRGRP_COUNT           17

/* Non-persistent grid change (internal use; don't worry about it)
 */
struct ps_game_npgc { 
  uint8_t col,row;
  uint8_t tileid,physics,shape;
};

struct ps_game {

  struct ps_scenario *scenario;
  struct ps_player *playerv[PS_PLAYER_LIMIT];
  int playerc;
  int difficulty;
  int length;
  int treasurev[PS_TREASURE_LIMIT];
  int treasurec;
  struct ps_stats *stats;
  struct ps_bloodhound_activator *bloodhound_activator;
  struct ps_statusreport *statusreport; // optional
  struct ps_dragoncharger *dragoncharger;
  struct ps_summoner *summoner;
  struct ps_switchboard *switchboard;
  //struct ps_gamelog *gamelog;
  struct ps_score_store *score_store;
  
  struct ps_sprgrp grpv[PS_SPRGRP_COUNT];
  struct ps_grid *grid; // WEAK
  int gridx,gridy; // Grid's position in world.
  struct ps_physics *physics;
  int inhibit_screen_switch; // Nonzero when we first move to a neighbor grid. Heroes reset it.
  int suppress_switch_effects; // Nonzero during screen change. Switches don't cause visual effects.

  struct ps_game_renderer *renderer;

// Signals to owner that GUI activity is necessary:
  const struct ps_res_trdef *got_treasure;
  int finished;
  int paused;

  struct ps_game_npgc *npgcv;
  int npgcc,npgca;

  int input_watchid;
  
};

/* Game lifecycle and configuration.
 *****************************************************************************/

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

/* For a test scenario, follow the same process but use this instead of ps_game_generate().
 * We will generate a scenario with one explicit region and only the blueprint IDs you provide.
 * There is some hacky logic to permit test games with no treasure; they will run until aborted.
 */
int _ps_game_generate_test(struct ps_game *game,int regionid,int blueprintid,...);
#define ps_game_generate_test(game,regionid,...) _ps_game_generate_test(game,regionid,##__VA_ARGS__,-1)

/* Another test option.
 * Generate a scenario that uses every blueprint, for exhaustive testing.
 */
int ps_game_generate_all_blueprints_test(struct ps_game *game,int blueprintidlo,int blueprintidhi,int regionid);

/* Broad controls of game state during play.
 *****************************************************************************/

/* With a scenario generated, call this to reset the transient state and begin play.
 */
int ps_game_restart(struct ps_game *game);

/* During play, call this to forcibly return to the first screen.
 * This is an option in the pause menu, for use when players end up in an impossible place.
 */
int ps_game_return_to_start_screen(struct ps_game *game);

/* Change to a particular screen.
 * (mode) influences transition effect and whether hero sprites are preserved.
 * This is mostly for internal use.
 */
int ps_game_change_screen(struct ps_game *game,int x,int y,int mode);
#define PS_CHANGE_SCREEN_MODE_RESET      1 /* Hard reset. */
#define PS_CHANGE_SCREEN_MODE_NEIGHBOR   2 /* Slide effect, and move heroes to the other side. */
#define PS_CHANGE_SCREEN_MODE_WARP       3 /* Spawn fresh hero sprites. */

int ps_game_update(struct ps_game *game);

/* Pause game and cause our owner to load the pause menu.
 */
int ps_game_pause(struct ps_game *game,int pause);

/* High-level game events and troubleshooting support.
 *****************************************************************************/

int ps_game_collect_treasure(struct ps_game *game,struct ps_sprite *collector,int treasureid);
int ps_game_get_treasure_state(const struct ps_game *game,int treasureid); // 0=uncollected, 1=collected
int ps_game_count_collected_treasures(const struct ps_game *game);

/* Record statistics.
 */
int ps_game_report_kill(struct ps_game *game,struct ps_sprite *assailant,struct ps_sprite *victim);
int ps_game_report_switch(struct ps_game *game,struct ps_sprite *presser);
void ps_game_dump_stats(const struct ps_game *game);

/* These can be nice for troubleshooting.
 * In particular, I'm adding them because I want to see the blueprint id for the current grid.
 */
struct ps_screen *ps_game_get_screen_for_current_grid(const struct ps_game *game);
int ps_game_get_current_blueprint_id(const struct ps_game *game);
int ps_game_force_next_screen(struct ps_game *game,int dx,int dy);

/* ps_game_decorate_monster_death() creates fireworks and a prize, plays a sound effect, and checks the deathgate.
 * This is the default behavior for killing any sprite, if it is fragile and doesn't implement hurt().
 * You can call it manually, eg if your monster has HP or some other hurt condition.
 * That doesn't kill the victim or report any statistics (duh, we don't have enough information).
 */
int ps_game_decorate_monster_death(struct ps_game *game,int x,int y);
int ps_game_create_fireworks(struct ps_game *game,int x,int y);
int ps_game_create_prize(struct ps_game *game,int x,int y);
int ps_game_create_splash(struct ps_game *game,int x,int y);
int ps_game_check_deathgate(struct ps_game *game);

// For test/debug builds, restore all heroes to life.
int ps_game_heal_all_heroes(struct ps_game *game);

/* Make a bloodhound sprite which will run onscreen and tell the players which way to go.
 * There is an object 'bloodhound_activator' devoted to this; one doesn't normally call this function.
 */
int ps_game_summon_bloodhound(struct ps_game *game);

// Helpers for the bloodhound. See generalized form below.
int ps_game_get_direction_to_nearest_treasure(const struct ps_game *game);
int ps_game_measure_distance_between_screens(int *distance,int *direction,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy);

// Wipe (path) and replace with the shortest path linking (srcx,srcy) to (dstx,dsty) based on the screens' door flags.
int ps_game_compose_world_path(struct ps_path *path,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy);

/* Wipe (path) and replace with a path from (srcx,srcy) to (dstx,dsty) in the given grid.
 * If successful, the returned path will not contain any cells in (impassable).
 * The algorithm is fast and reliable but does not necessarily produce optimal results (there may be a shorter path).
 */
int ps_game_compose_grid_path(struct ps_path *path,const struct ps_grid *grid,int dstx,int dsty,int srcx,int srcy,uint16_t impassable);

/* Return a rectangle containing (x,y) where all cells have the same physics.
 * Precise behavior is undefined if the region is not rectangular.
 */
int ps_game_get_contiguous_physical_rect_in_grid(int *dstx,int *dsty,int *dstw,int *dsth,const struct ps_grid *grid,int x,int y);

/* The standard groups owned by ps_game can be expressed as a 32-bit mask.
 * Sprites can use this to modify and restore another sprite's groups.
 */
uint32_t ps_game_get_group_mask_for_sprite(const struct ps_game *game,const struct ps_sprite *spr);
int ps_game_set_group_mask_for_sprite(struct ps_game *game,struct ps_sprite *spr,uint32_t grpmask);

/* Set cell (col,row) in the current grid to the given values.
 * Remember prior values and magically restore them when the grid unloads.
 * Use anything OOB (eg -1) to keep that field the same.
 */
int ps_game_apply_nonpersistent_grid_change(struct ps_game *game,int col,int row,int tileid,int physics,int shape);
int ps_game_reverse_nonpersistent_grid_change(struct ps_game *game,int col,int row);

/* Barriers can be part of the grid or managed by a sprite; we take care of the details.
 * Generally, a barrier starts closed and you should call this with (open==1) to change it.
 * (barrierid) must be positive; we quietly ignore anything <1.
 */
int ps_game_get_switch(const struct ps_game *game,int switchid);
int ps_game_set_switch(struct ps_game *game,int switchid,int value);

/* Create or destroy a sprite pointing at this hero, for when he is offscreen.
 */
int ps_game_add_indicator_for_hero(struct ps_game *game,struct ps_sprite *hero);
int ps_game_remove_indicator_for_hero(struct ps_game *game,struct ps_sprite *hero);

/* Mark all treasures except one as collected, and warp to the screen of the last treasure.
 * Nice for troubleshooting the game over menu.
 */
int ps_game_advance_to_finish(struct ps_game *game);

/* Set the 'invincible' of every player with attached input.
 * Use this after swapping inputs to indicate which are now enabled.
 * Don't use this in production builds!
 */
int ps_game_highlight_enabled_players(struct ps_game *game);

/* Select a cell at random that matches the given physics mask.
 * eg (1<<PS_BLUEPRINT_CELL_VACANT).
 */
int ps_game_find_random_cell_with_physics(int *col,int *row,const struct ps_game *game,uint16_t mask);

/* Support for ps_sprite_chestkeeper.
 * When he dies, he calls this to create the real treasure chest.
 */
int ps_game_recreate_treasure_chest(struct ps_game *game,int x,int y);

int ps_game_save_to_score_store(struct ps_game *game);

/* For a monster at (x,y), return a reasonable random direction for it to move.
 * We attempt to point towards passable cells.
 */
int ps_game_select_random_travel_vector(
  double *dx,double *dy,const struct ps_game *game,double x,double y,double speed,uint16_t impassable
);

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
