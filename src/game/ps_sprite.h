/* ps_sprite.h
 * A sprite is a moveable object on screen, with logic attached.
 * The game's model is composed principally of sprites and grids.
 */

#ifndef PS_SPRITE_H
#define PS_SPRITE_H

struct ps_sprite;
struct ps_sprgrp;
struct ps_sprtype;
struct ps_sprdef;
struct akgl_vtx_maxtile;
struct ps_game;
struct ps_fbox;
struct ps_circle;

#define PS_SPRITE_SHAPE_SQUARE      0
#define PS_SPRITE_SHAPE_CIRCLE      1

/* ps_sprite: Generic sprite interface.
 *****************************************************************************/

struct ps_sprite {
  const struct ps_sprtype *type; // Required, immutable.
  int refc;
  struct ps_sprgrp **grpv; // All references are mutual.
  int grpc,grpa;
  struct ps_sprdef *def; // Optional.

  double x,y;
  double radius;
  int shape;
  int collide_sprites;
  uint16_t impassable; // Mask of cell physics that I can't walk on.
  int switchid; // Sprites in BARRIER group can request a callback when a global switch changes.
  
  uint8_t tsid; // Sprites may have multiple tiles, but all from the same sheet.
  int layer; // Low layers are drawn first.

  // Rendering source; used only if (type->draw) not implemented.
  uint8_t tileid;
  uint8_t opacity;

  int phreconsider; // For transient use by physics.
  int collided_grid; // Reset each frame for physics sprites.

  struct ps_sprgrp *master; // Optional. Set when some other sprite is controlling this one.
  
};

struct ps_sprite *ps_sprite_new(const struct ps_sprtype *type);
void ps_sprite_del(struct ps_sprite *spr);
int ps_sprite_ref(struct ps_sprite *spr);

int ps_sprite_set_sprdef(struct ps_sprite *spr,struct ps_sprdef *def);

int ps_sprite_update(struct ps_sprite *spr,struct ps_game *game);
int ps_sprite_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr);

int ps_sprite_receive_damage(struct ps_game *game,struct ps_sprite *victim,struct ps_sprite *assailant);

int ps_sprites_collide(const struct ps_sprite *a,const struct ps_sprite *b);
int ps_sprite_collide_fbox(const struct ps_sprite *spr,const struct ps_fbox *fbox);
int ps_sprite_collide_circle(const struct ps_sprite *spr,const struct ps_circle *circle);

/* Setting a sprite's master also releases it from the existing master if present.
 */
int ps_sprite_set_master(struct ps_sprite *slave,struct ps_sprite *master,struct ps_game *game);
struct ps_sprite *ps_sprite_get_master(const struct ps_sprite *slave);
int ps_sprite_release_from_master(struct ps_sprite *slave,struct ps_game *game);

/* ps_sprgrp: Mutual collection of sprites.
 *****************************************************************************/

#define PS_SPRGRP_ORDER_ADDR       0 /* Order by address. Most efficient, and the default. */
#define PS_SPRGRP_ORDER_EXPLICIT   1 /* Preserve order in which sprites are added. */
#define PS_SPRGRP_ORDER_RENDER     2 /* Maintain rendering order, imperfectly. */

struct ps_sprgrp {
  int refc; // 0==immortal
  struct ps_sprite **sprv;
  int sprc,spra;
  int order; // Change order only while the group is empty.
  int sortd; // For PS_SPRGRP_ORDER_RENDER, toggle sorting order.
};

struct ps_sprgrp *ps_sprgrp_new();
void ps_sprgrp_cleanup(struct ps_sprgrp *grp);
void ps_sprgrp_del(struct ps_sprgrp *grp);
int ps_sprgrp_ref(struct ps_sprgrp *grp);

int ps_sprgrp_has_sprite(const struct ps_sprgrp *grp,const struct ps_sprite *spr);
int ps_sprgrp_add_sprite(struct ps_sprgrp *grp,struct ps_sprite *spr);
int ps_sprgrp_remove_sprite(struct ps_sprgrp *grp,struct ps_sprite *spr);
#define ps_sprite_has_sprgrp(spr,grp) ps_sprgrp_has_sprite(grp,spr)
#define ps_sprite_add_sprgrp(spr,grp) ps_sprgrp_add_sprite(grp,spr)
#define ps_sprite_remove_sprgrp(spr,grp) ps_sprgrp_remove_sprite(grp,spr)

int ps_sprgrp_add_all(struct ps_sprgrp *dst,const struct ps_sprgrp *src);
int ps_sprgrp_remove_all(struct ps_sprgrp *dst,const struct ps_sprgrp *src);

int ps_sprite_kill(struct ps_sprite *spr); // Remove all groups.
int ps_sprgrp_clear(struct ps_sprgrp *grp); // Remove all sprites.
int ps_sprgrp_kill(struct ps_sprgrp *grp); // Remove all groups from all sprites.
int ps_sprite_join_all(struct ps_sprite *spr,struct ps_sprite *ref); // Add (spr) to every group that (ref) is in.

/* Largely a convenience: Add (spr) to DEATHROW group and otherwise leave it untouched.
 * When the update cycle completes, the sprite will be killed.
 * Use this in preference to ps_sprite_kill() unless you know what you're doing.
 */
int ps_sprite_kill_later(struct ps_sprite *spr,struct ps_game *game);

int ps_sprgrp_sort(struct ps_sprgrp *grp,int completely);

/* Add to (dst) every sprite which is in both (a) and (b).
 * Returns count added.
 */
int ps_sprgrp_intersect(struct ps_sprgrp *dst,struct ps_sprgrp *a,struct ps_sprgrp *b);

/* ps_sprtype: Definition of a sprite type.
 * These are always statically-allocated and constant.
 *****************************************************************************/

struct ps_sprtype {
  const char *name;
  int objlen;

  double radius;
  int shape;
  int layer;
  int ignore_collisions_on_same_type;

  int (*init)(struct ps_sprite *spr);
  void (*del)(struct ps_sprite *spr);

  int (*configure)(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef);
  const char *(*get_configure_argument_name)(int argp);

  int (*update)(struct ps_sprite *spr,struct ps_game *game);

  int (*draw)(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr);

  int (*hurt)(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant);
  int (*set_switch)(struct ps_game *game,struct ps_sprite *spr,int value);

};

const struct ps_sprtype *ps_sprtype_by_name(const char *name,int namec);

/* List of all sprite types, terminated with a NULL.
 */
extern const struct ps_sprtype *ps_all_sprtypes[];

extern const struct ps_sprtype ps_sprtype_dummy;
extern const struct ps_sprtype ps_sprtype_hero;
extern const struct ps_sprtype ps_sprtype_anim2;
extern const struct ps_sprtype ps_sprtype_hookshot;
extern const struct ps_sprtype ps_sprtype_arrow;
extern const struct ps_sprtype ps_sprtype_healmissile;
extern const struct ps_sprtype ps_sprtype_explosion;
extern const struct ps_sprtype ps_sprtype_switch;
extern const struct ps_sprtype ps_sprtype_bug;
extern const struct ps_sprtype ps_sprtype_seamonster;
extern const struct ps_sprtype ps_sprtype_missile;
extern const struct ps_sprtype ps_sprtype_treasurechest;
extern const struct ps_sprtype ps_sprtype_dragonbug;
extern const struct ps_sprtype ps_sprtype_bumblebat;
extern const struct ps_sprtype ps_sprtype_blueberry;
extern const struct ps_sprtype ps_sprtype_rabbit;
extern const struct ps_sprtype ps_sprtype_prize;
extern const struct ps_sprtype ps_sprtype_swordswitch;
extern const struct ps_sprtype ps_sprtype_lobster;
extern const struct ps_sprtype ps_sprtype_fireworks;
extern const struct ps_sprtype ps_sprtype_bloodhound;
extern const struct ps_sprtype ps_sprtype_turtle;
extern const struct ps_sprtype ps_sprtype_dragon;
extern const struct ps_sprtype ps_sprtype_toast;
extern const struct ps_sprtype ps_sprtype_bomb;
extern const struct ps_sprtype ps_sprtype_skeleton;
extern const struct ps_sprtype ps_sprtype_flames;
extern const struct ps_sprtype ps_sprtype_killozap;
extern const struct ps_sprtype ps_sprtype_friedheart;
extern const struct ps_sprtype ps_sprtype_heroindicator;
extern const struct ps_sprtype ps_sprtype_giraffe;
extern const struct ps_sprtype ps_sprtype_yak;
extern const struct ps_sprtype ps_sprtype_chicken;
extern const struct ps_sprtype ps_sprtype_egg;
extern const struct ps_sprtype ps_sprtype_motionsensor;
extern const struct ps_sprtype ps_sprtype_gorilla;
extern const struct ps_sprtype ps_sprtype_penguin;
extern const struct ps_sprtype ps_sprtype_snake;
extern const struct ps_sprtype ps_sprtype_teleporter;
extern const struct ps_sprtype ps_sprtype_boxingglove;
extern const struct ps_sprtype ps_sprtype_lwizard;
extern const struct ps_sprtype ps_sprtype_elefence;
extern const struct ps_sprtype ps_sprtype_conveyor;
extern const struct ps_sprtype ps_sprtype_shooter;
extern const struct ps_sprtype ps_sprtype_teleffect;
extern const struct ps_sprtype ps_sprtype_inert;
extern const struct ps_sprtype ps_sprtype_edgecrawler;
extern const struct ps_sprtype ps_sprtype_mimic;
extern const struct ps_sprtype ps_sprtype_piston;
extern const struct ps_sprtype ps_sprtype_multipiston;
extern const struct ps_sprtype ps_sprtype_landmine;
extern const struct ps_sprtype ps_sprtype_sawblade;
extern const struct ps_sprtype ps_sprtype_flamethrower;
//INSERT SPRTYPE DEFINITION HERE

/* API for sprite types too trivial to warrant their own headers.
 */

int ps_prize_fling(struct ps_sprite *spr,int dir);
int ps_swordswitch_activate(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero);
struct ps_sprite *ps_sprite_fireworks_new(struct ps_game *game,int x,int y,int p,int c);
int ps_sprite_dragon_add_player(struct ps_sprite *spr,int playerid,struct ps_game *game);
int ps_sprite_bomb_throw(struct ps_sprite *spr,int direction,int magnitude);
int ps_sprite_flames_throw(struct ps_sprite *spr,int direction);
struct ps_sprite *ps_sprite_flames_find_for_hero(const struct ps_game *game,const struct ps_sprite *hero); // Does not return moving flames.
struct ps_sprite *ps_sprite_heroindicator_get_hero(const struct ps_sprite *spr);
int ps_sprite_heroindicator_set_hero(struct ps_sprite *spr,struct ps_sprite *hero);
int ps_sprite_react_to_sword(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero,int state); // Dispatcher for SWORDAWARE group. (state) in (0,1,2)
int ps_sprite_inert_fling(struct ps_sprite *spr,struct ps_game *game,int dir);

/* Toggle a switch.
 * Works for 'switch' and 'swordswitch', and we'll add other types as needed.
 * (value) is >0 to turn on, 0 to turn off, or <0 to toggle.
 * Returns zero if this sprite doesn't support actuation, >0 on success, or <0 for real errors.
 */
int ps_sprite_actuate(struct ps_sprite *spr,struct ps_game *game,int value);

int ps_sprite_turtle_drop_slave(struct ps_sprite *spr,struct ps_game *game);
int ps_sprite_rabbit_drop_slave(struct ps_sprite *spr,struct ps_game *game);
int ps_sprite_hookshot_drop_slave(struct ps_sprite *spr,struct ps_sprite *slave,struct ps_game *game); // pumpkin or user

/* ps_sprdef: Resource type combining sprtype with parameters.
 * This is the interface you typically want for creating new sprites.
 *****************************************************************************/

#define PS_SPRDEF_FLD_layer        1
#define PS_SPRDEF_FLD_grpmask      2
#define PS_SPRDEF_FLD_radius       3
#define PS_SPRDEF_FLD_shape        4
#define PS_SPRDEF_FLD_type         5 /* Not stored in extension. */
#define PS_SPRDEF_FLD_tileid       6 /* Not stored in extension. */
#define PS_SPRDEF_FLD_impassable   7
#define PS_SPRDEF_FLD_difficulty   8
#define PS_SPRDEF_FLD_framec       9 /* not used generically */
#define PS_SPRDEF_FLD_frametime   10 /* not used generically */
#define PS_SPRDEF_FLD_switchid    11
#define PS_SPRDEF_FLD_missileid   12 /* not used generically */
#define PS_SPRDEF_FLD_enable      13 /* not used generically */

struct ps_sprdef_fld {
  int k,v;
};

struct ps_sprdef {
  int refc;
  const struct ps_sprtype *type;
  uint16_t tileid;
  int fldc;
  struct ps_sprdef_fld fldv[];
};

struct ps_sprdef *ps_sprdef_new(int fldc);
void ps_sprdef_del(struct ps_sprdef *sprdef);
int ps_sprdef_ref(struct ps_sprdef *sprdef);

int ps_sprdef_fld_search(const struct ps_sprdef *sprdef,int k);
int ps_sprdef_fld_get(const struct ps_sprdef *sprdef,int k,int def); // (def) if not found.

/* Sprites created this way are always a member of the KEEPALIVE group.
 * (x,y) are in pixels, not cells.
 * This returns a WEAK reference.
 */
struct ps_sprite *ps_sprdef_instantiate(struct ps_game *game,struct ps_sprdef *sprdef,const int *argv,int argc,int x,int y);

/* Encoded form is line-oriented "KEY VALUE".
 * See ps_sprdef_fld_k_{eval,repr} and ps_sprdef_fld_v_{eval,repr}.
 */
struct ps_sprdef *ps_sprdef_decode(const void *src,int srcc);
int ps_sprdef_encode(void *dstpp,const struct ps_sprdef *sprdef);
int ps_sprdef_encode_binary(void *dst,int dsta,const struct ps_sprdef *sprdef);

/* Represent or evaluate value for most sprdef fields.
 * The only exception is "type", whose value is a type name or pointer to ps_sprtype.
 */
int ps_sprdef_fld_v_eval(int *dst,int k,const char *src,int srcc);
int ps_sprdef_fld_v_repr(char *dst,int dsta,int k,int v);

/* Look at my surroundings and if necessary and possible, move a little bit to get to a legal position.
 */
int ps_sprite_attempt_legal_position(struct ps_sprite *spr,struct ps_game *game);

#endif
