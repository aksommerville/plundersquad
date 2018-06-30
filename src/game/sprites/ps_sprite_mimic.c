/* ps_sprite_mimic.c
 * Monster that looks identical to a hero.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/sprites/ps_sprite_hero.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_MIMIC_HEAD_OFFSET ((PS_TILESIZE*3)/4) /* Matches ps_sprite_hero.c:PS_HERO_HEAD_OFFSET */

#define PS_MIMIC_TARGET_DISTANCE (PS_TILESIZE*2.0)
#define PS_MIMIC_WALK_SPEED 0.5

#define PS_MIMIC_FRAME_DELAY 10
#define PS_MIMIC_FRAME_COUNT 4

/* Private sprite object.
 */

struct ps_sprite_mimic {
  struct ps_sprite hdr;
  int playerid; // zero if unconfigured; will refresh on next update
  int facedir;
  int walking;
  int animtime;
  int animframe;
  int counter;

  // Cached properties from player:
  uint8_t tileid_head;
  uint8_t tileid_body;
  uint32_t rgba_head;
  uint32_t rgba_body;
  int head_on_top_always;
  struct ps_sprgrp *grp;
  
};

#define SPR ((struct ps_sprite_mimic*)spr)

/* Delete.
 */

static void _ps_mimic_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->grp);
  ps_sprgrp_del(SPR->grp);
}

/* Initialize.
 */

static int _ps_mimic_init(struct ps_sprite *spr) {
  SPR->facedir=PS_DIRECTION_SOUTH;
  if (!(SPR->grp=ps_sprgrp_new())) return -1;
  return 0;
}

/* Configure.
 */

static int _ps_mimic_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_mimic_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_mimic_configure(), for editor.
  return 0;
}

/* Quickly verify that we have a valid player attached.
 */

static int ps_mimic_validate_player(const struct ps_sprite *spr) {
  if (SPR->playerid<1) return 0;
  if (!SPR->grp) return 0;
  if (SPR->grp->sprc!=1) return 0;
  return 1;
}

/* Helpers for player search.
 */

static int ps_mimic_count_players(const struct ps_game *game) {
  int c=0;
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;
    c++;
  }
  return c;
}

static struct ps_sprite *ps_mimic_select_player(const struct ps_game *game,int p) {
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;
    if (!p--) return hero;
  }
  return 0;
}

/* Find a new player and attach it.
 */

static int ps_mimic_find_player(struct ps_sprite *spr,struct ps_game *game) {

  /* Select one randomly. */
  int candidatec=ps_mimic_count_players(game);
  if (candidatec<1) return -1;
  int candidatep=rand()%candidatec;
  struct ps_sprite *hero=ps_mimic_select_player(game,candidatep);
  if (!hero) return -1;

  /* Validate it. */
  struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)hero;
  if (!HERO->player) return -1;
  if (!HERO->player->plrdef) return -1;
  if (HERO->player->playerid<1) return -1;
  if (HERO->player->playerid>PS_PLAYER_LIMIT) return -1;
  if (HERO->player->palette<0) return -1;
  if (HERO->player->palette>=HERO->player->plrdef->palettec) return -1;

  /* Retain hero sprite. */
  ps_sprgrp_clear(SPR->grp);
  if (ps_sprgrp_add_sprite(SPR->grp,hero)<0) return -1;

  /* Attach trivial properties. */
  SPR->playerid=HERO->player->playerid;
  SPR->tileid_head=HERO->player->plrdef->tileid_head;
  SPR->tileid_body=HERO->player->plrdef->tileid_body;
  SPR->rgba_head=HERO->player->plrdef->palettev[HERO->player->palette].rgba_head;
  SPR->rgba_body=HERO->player->plrdef->palettev[HERO->player->palette].rgba_body;
  SPR->head_on_top_always=HERO->player->plrdef->head_on_top_always;
  
  return 0;
}

/* Always face towards the player we are mimicking (even if we are walking a different direction).
 */

static int ps_mimic_face_player(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *hero=SPR->grp->sprv[0];
  double dx=hero->x-spr->x;
  double adx=(dx<0.0)?-dx:dx;
  double dy=hero->y-spr->y;
  double ady=(dy<0.0)?-dy:dy;
  if (adx>ady) {
    if (dx<0.0) SPR->facedir=PS_DIRECTION_WEST;
    else SPR->facedir=PS_DIRECTION_EAST;
  } else if (dy<0.0) SPR->facedir=PS_DIRECTION_NORTH;
  else SPR->facedir=PS_DIRECTION_SOUTH;
  return 0;
}

/* Move by a constant increment to keep ourself a constant distance from the player.
 */

static int ps_mimic_approach_player(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *hero=SPR->grp->sprv[0];

  /* Our target position is cardinal to the player, based on *our own* face direction.
   * That face direction has just been set, as the last thing.
   */
  int principal_axis;
  double targetx=hero->x;
  double targety=hero->y;
  switch (SPR->facedir) {
    case PS_DIRECTION_NORTH: targety+=PS_MIMIC_TARGET_DISTANCE; principal_axis=PS_AXIS_VERT; break;
    case PS_DIRECTION_SOUTH: targety-=PS_MIMIC_TARGET_DISTANCE; principal_axis=PS_AXIS_VERT; break;
    case PS_DIRECTION_WEST: targetx+=PS_MIMIC_TARGET_DISTANCE; principal_axis=PS_AXIS_HORZ; break;
    case PS_DIRECTION_EAST: targetx-=PS_MIMIC_TARGET_DISTANCE; principal_axis=PS_AXIS_HORZ; break;
  }

  /* Check where to walk.
   * We've had problems with mimics sticking to walls, especially round-cornered ones.
   * So now they move only on the principal axis, to keep it clean.
   */
  int walking=0;
  double dx=targetx-spr->x;
  double adx=(dx<0.0)?-dx:dx;
  double dy=targety-spr->y;
  double ady=(dy<0.0)?-dy:dy;
  int walkx=(adx>PS_MIMIC_WALK_SPEED);
  int walky=(ady>PS_MIMIC_WALK_SPEED);
  if (walkx&&walky) {
    if (principal_axis==PS_AXIS_HORZ) walky=0;
    else walkx=0;
  }

  if (adx<=PS_MIMIC_WALK_SPEED) spr->x=targetx;
  else if (!walkx) ;
  else if (dx<0.0) { 
    spr->x-=PS_MIMIC_WALK_SPEED;
    walking=1;
  } else {
    spr->x+=PS_MIMIC_WALK_SPEED;
    walking=1;
  }
  if (ady<=PS_MIMIC_WALK_SPEED) spr->y=targety;
  else if (!walky) ;
  else if (dy<0.0) {
    spr->y-=PS_MIMIC_WALK_SPEED;
    walking=1;
  } else {
    spr->y+=PS_MIMIC_WALK_SPEED;
    walking=1;
  }

  /* Reset animation? */
  if (walking&&!SPR->walking) {
    SPR->animtime=0;
    SPR->animframe=0;
  }
  SPR->walking=walking;
  
  return 0;
}

/* Animate.
 */

static int ps_mimic_animate(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->walking) return 0;
  if (++(SPR->animtime)>=PS_MIMIC_FRAME_DELAY) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_MIMIC_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }
  return 0;
}

/* Update.
 */

static int _ps_mimic_update(struct ps_sprite *spr,struct ps_game *game) {
  SPR->counter++;

  if (!ps_mimic_validate_player(spr)) {
    if (ps_mimic_find_player(spr,game)<0) return -1;
  }

  if (ps_mimic_face_player(spr,game)<0) return -1;
  if (ps_mimic_approach_player(spr,game)<0) return -1;
  if (ps_mimic_animate(spr,game)<0) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_mimic_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (!ps_mimic_validate_player(spr)) return 0;
  if (vtxa<2) return 2;

  /* Put head and body vertices in the right order. */
  struct akgl_vtx_maxtile *vtx_body=vtxv;
  struct akgl_vtx_maxtile *vtx_head=vtxv+1;
  if (SPR->head_on_top_always) {
  } else if (SPR->facedir==PS_DIRECTION_NORTH) {
    vtx_head=vtxv;
    vtx_body=vtxv+1;
  }

  /* Set up basic vertices. */
  vtx_body->x=spr->x;
  vtx_body->y=spr->y;
  vtx_body->tileid=SPR->tileid_body;
  vtx_body->size=PS_TILESIZE;
  vtx_body->ta=0;
  vtx_body->pr=SPR->rgba_body>>24;
  vtx_body->pg=SPR->rgba_body>>16;
  vtx_body->pb=SPR->rgba_body>>8;
  vtx_body->a=0xff;
  vtx_body->t=0;
  vtx_body->xform=AKGL_XFORM_NONE;
  
  vtx_head->x=spr->x;
  vtx_head->y=spr->y-PS_MIMIC_HEAD_OFFSET;
  vtx_head->tileid=SPR->tileid_head;
  vtx_head->size=PS_TILESIZE;
  vtx_head->ta=0;
  vtx_head->pr=SPR->rgba_head>>24;
  vtx_head->pg=SPR->rgba_head>>16;
  vtx_head->pb=SPR->rgba_head>>8;
  vtx_head->a=0xff;
  vtx_head->t=0;
  vtx_head->xform=AKGL_XFORM_NONE;

  /* Adjust tiles and transforms. */
  switch (SPR->facedir) {
    case PS_DIRECTION_SOUTH: break;
    case PS_DIRECTION_NORTH: vtx_body->tileid+=0x01; vtx_head->tileid+=0x01; break;
    case PS_DIRECTION_WEST: vtx_body->tileid+=0x02; vtx_head->tileid+=0x02; break;
    case PS_DIRECTION_EAST: vtx_body->tileid+=0x02; vtx_head->tileid+=0x02; vtx_body->xform=AKGL_XFORM_FLOP; vtx_head->xform=AKGL_XFORM_FLOP; break;
  }
  if (SPR->walking) switch (SPR->animframe) {
    case 0: vtx_body->tileid+=0x10; break;
    case 2: vtx_body->tileid+=0x20; break;
  }
  
  return 2;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_mimic={
  .name="mimic",
  .objlen=sizeof(struct ps_sprite_mimic),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_mimic_init,
  .del=_ps_mimic_del,
  .configure=_ps_mimic_configure,
  .get_configure_argument_name=_ps_mimic_get_configure_argument_name,
  .update=_ps_mimic_update,
  .draw=_ps_mimic_draw,
  
};
