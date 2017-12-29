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
  int collide_hole;
  int collide_sprites;
  
  uint8_t tsid; // Sprites may have multiple tiles, but all from the same sheet.
  int layer; // Low layers are drawn first.

  // Rendering source; used only if (type->draw) not implemented.
  uint8_t tileid;
  uint8_t opacity;

  int phreconsider; // For transient use by physics.
  
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

/* ps_sprtype: Definition of a sprite type.
 * These are always statically-allocated and constant.
 *****************************************************************************/

struct ps_sprtype {
  const char *name;
  int objlen;

  double radius;
  int shape;
  int layer;

  int (*init)(struct ps_sprite *spr);
  void (*del)(struct ps_sprite *spr);

  int (*configure)(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc);
  const char *(*get_configure_argument_name)(int argp);

  int (*update)(struct ps_sprite *spr,struct ps_game *game);

  int (*draw)(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr);

  int (*hurt)(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant);

};

const struct ps_sprtype *ps_sprtype_by_name(const char *name,int namec);

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

/* API for sprite types too trivial to warrant their own headers.
 */

int ps_prize_fling(struct ps_sprite *spr,int dir);
int ps_swordswitch_activate(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero,int force);
struct ps_sprite *ps_sprite_fireworks_new(struct ps_game *game,int x,int y,int p,int c);

/* ps_sprdef: Resource type combining sprtype with parameters.
 * This is the interface you typically want for creating new sprites.
 *****************************************************************************/

#define PS_SPRDEF_FLD_layer      1
#define PS_SPRDEF_FLD_grpmask    2
#define PS_SPRDEF_FLD_radius     3
#define PS_SPRDEF_FLD_shape      4

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
 * This returns a WEAK reference.
 */
struct ps_sprite *ps_sprdef_instantiate(struct ps_game *game,struct ps_sprdef *sprdef,const int *argv,int argc,int x,int y);

struct ps_sprdef *ps_sprdef_decode(const void *src,int srcc);

#endif
