#include "ps.h"
#include "ps_sprite_hero.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"
#include "util/ps_geometry.h"

#define PS_HEALMISSILE_SPRDEF_ID 6
#define PS_HEALMISSILE_SPEED 4.0
#define PS_HEALMISSILE_RADIUS 4.0

/* Private sprite object.
 */

struct ps_sprite_healmissile {
  struct ps_sprite hdr;
  double dx,dy;
  int finished;
};

#define SPR ((struct ps_sprite_healmissile*)spr)

/* Determine if this victim should be hurt instead of healed (see comments below).
 */

static int ps_healmissile_should_hurt_victim(const struct ps_game *game,const struct ps_sprite *spr,const struct ps_sprite *victim) {
  if (!victim) return 0;
  if (victim->type!=&ps_sprtype_hero) return 0;
  struct ps_sprite_hero *VICTIM=(struct ps_sprite_hero*)victim;
  if (!VICTIM->hp) return 0;
  if (!VICTIM->player) return 0;
  if (!VICTIM->player->plrdef) return 0;
  if (VICTIM->player->plrdef->skills&PS_SKILL_IMMORTAL) return 1;
  return 0;
}

/* Kill the immortal -- we copied code from ps_sprite_hero::hurt because we have to skill the IMMORTAL skill check.
 */

static int ps_healmissile_kill_hero(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *hero) {
  struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)hero;
  PS_SFX_HERO_DEAD
  HERO->hp=0;
  if (ps_game_create_fireworks(game,hero->x,hero->y)<0) return -1;
  if (ps_game_report_kill(game,spr,hero)<0) return -1;
  if (ps_hero_add_state(hero,PS_HERO_STATE_GHOST,game)<0) return -1;
  return 0;
}

/* Check if we can heal something.
 */

static int ps_healmissile_heal_mortals(struct ps_sprite *spr,struct ps_game *game) {
  double left=spr->x-PS_HEALMISSILE_RADIUS;
  double right=spr->x+PS_HEALMISSILE_RADIUS;
  double top=spr->y-PS_HEALMISSILE_RADIUS;
  double bottom=spr->y+PS_HEALMISSILE_RADIUS;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_HERO].sprc;i++) {
    struct ps_sprite *victim=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if (victim->type!=&ps_sprtype_hero) continue;
    if (victim->x+victim->radius<=left) continue;
    if (victim->x-victim->radius>=right) continue;
    if (victim->y+victim->radius<=top) continue;
    if (victim->y-victim->radius>=bottom) continue;

    /* If the hero is "immortal" and alive, hearts kill it.
     * This is necessary because we build solutions around the assumption that 
     * "nurse plus anyone" means the non-nurse party can be killed and healed again.
     */
    if (ps_healmissile_should_hurt_victim(game,spr,victim)) {
      if (ps_healmissile_kill_hero(game,spr,victim)<0) return -1;
    } else {
      if (ps_hero_add_state(victim,PS_HERO_STATE_HEAL,game)<0) return -1;
    }

    if (ps_sprite_kill_later(spr,game)<0) return -1;
    SPR->finished=1;
    return 1;
  }
  return 0;
}

/* Abort if we hit something solid.
 */

static int ps_healmissile_check_collisions(struct ps_sprite *spr,struct ps_game *game) {

  if ((spr->x<0.0)||(spr->y<0.0)) return ps_sprite_kill_later(spr,game);
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) return ps_sprite_kill_later(spr,game);

  if (game->grid) {
    uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
    switch (physics) {
      case PS_BLUEPRINT_CELL_LATCH:
      case PS_BLUEPRINT_CELL_SOLID:
        return ps_sprite_kill_later(spr,game);
    }
  }

  // Healmissiles do not stop at solid sprites. We've built blueprints around that fact.

  return 0;
}

/* Update.
 */

static int _ps_healmissile_update(struct ps_sprite *spr,struct ps_game *game) {
  int err;
  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  if (!SPR->finished) {
    if (err=ps_healmissile_heal_mortals(spr,game)) return err;
  }
  if (ps_healmissile_check_collisions(spr,game)<0) return -1;
  return 0;
}

/* Hurt.
 */

static int _ps_healmissile_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  if (ps_sprite_kill_later(spr,game)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_PRIZE,spr)<0) return -1;

  PS_SFX_FRY_HEART

  struct ps_sprite *friedheart=ps_sprite_new(&ps_sprtype_friedheart);
  if (!friedheart) return -1;
  if (ps_game_set_group_mask_for_sprite(game,friedheart,
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)
  )<0) {
    ps_sprite_del(friedheart);
    return -1;
  }
  ps_sprite_del(friedheart);

  friedheart->x=spr->x;
  friedheart->y=spr->y;
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_healmissile={
  .name="healmissile",
  .objlen=sizeof(struct ps_sprite_healmissile),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .update=_ps_healmissile_update,
  .hurt=_ps_healmissile_hurt,
  
};

/* New, public ctor.
 */
 
struct ps_sprite *ps_sprite_healmissile_new(struct ps_sprite *user,struct ps_game *game) {
  if (!user||!game) return 0;
  if (user->type!=&ps_sprtype_hero) return 0;
  struct ps_sprite_hero *hero=(struct ps_sprite_hero*)user;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_HEALMISSILE_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found, expected for HEALMISSILE",PS_HEALMISSILE_SPRDEF_ID);
    return 0;
  }
  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,user->x,user->y);
  if (!spr) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",PS_HEALMISSILE_SPRDEF_ID);
    return 0;
  }

  switch (hero->facedir) {
    case PS_DIRECTION_NORTH: {
        SPR->dy=-1.0;
      } break;
    case PS_DIRECTION_SOUTH: {
        SPR->dy=1.0;
      } break;
    case PS_DIRECTION_WEST: {
        SPR->dx=-1.0;
      } break;
    case PS_DIRECTION_EAST: {
        SPR->dx=1.0;
      } break;
    default: return 0;
  }
  spr->x+=SPR->dx*PS_TILESIZE;
  spr->y+=SPR->dy*PS_TILESIZE;
  SPR->dx*=PS_HEALMISSILE_SPEED;
  SPR->dy*=PS_HEALMISSILE_SPEED;

  return spr;
}
