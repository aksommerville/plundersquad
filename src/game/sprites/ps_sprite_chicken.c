#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_sound_effects.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_CHICKEN_PHASE_IDLE     1
#define PS_CHICKEN_PHASE_WALK     2
#define PS_CHICKEN_PHASE_CHAT     3
#define PS_CHICKEN_PHASE_SQUAWK   4
#define PS_CHICKEN_PHASE_PECK     5

#define PS_CHICKEN_IDLE_TIME_MIN   30
#define PS_CHICKEN_IDLE_TIME_MAX   60
#define PS_CHICKEN_WALK_TIME_MIN   40
#define PS_CHICKEN_WALK_TIME_MAX  120
#define PS_CHICKEN_CHAT_TIME       30
#define PS_CHICKEN_SQUAWK_TIME     50
#define PS_CHICKEN_PECK_TIME       30
#define PS_CHICKEN_EDGE_MARGIN (PS_TILESIZE>>1)

#define PS_CHICKEN_WALK_SPEED 0.4
#define PS_CHICKEN_WALK_FRAME_TIME 8
#define PS_CHICKEN_WALK_FRAME_COUNT 4

#define PS_CHICKEN_POPULATION_LIMIT 12

/* Private sprite object.
 */

struct ps_sprite_chicken {
  struct ps_sprite hdr;
  int facedx;
  int phase;
  int phasetime;
  int animtime;
  int animframe;
  double walkdx,walkdy;
};

#define SPR ((struct ps_sprite_chicken*)spr)

/* Delete.
 */

static void _ps_chicken_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_chicken_init(struct ps_sprite *spr) {

  SPR->facedx=(rand()&1)?1:-1;

  return 0;
}

/* Phase initiators.
 */

static int ps_chicken_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_CHICKEN_PHASE_IDLE;
  SPR->phasetime=PS_CHICKEN_IDLE_TIME_MIN+rand()%(PS_CHICKEN_IDLE_TIME_MAX-PS_CHICKEN_IDLE_TIME_MIN);
  return 0;
}

static int ps_chicken_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_CHICKEN_PHASE_WALK;
  SPR->phasetime=PS_CHICKEN_WALK_TIME_MIN+rand()%(PS_CHICKEN_WALK_TIME_MAX-PS_CHICKEN_WALK_TIME_MIN);
  SPR->animtime=0;
  SPR->animframe=0;
  
  if (ps_game_select_random_travel_vector(&SPR->walkdx,&SPR->walkdy,game,spr->x,spr->y,PS_CHICKEN_WALK_SPEED,spr->impassable)<0) return -1;

  if (SPR->walkdx<0.0) SPR->facedx=-1;
  else SPR->facedx=1;
  
  return 0;
}

static int ps_chicken_begin_PECK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_CHICKEN_PHASE_PECK;
  SPR->phasetime=PS_CHICKEN_PECK_TIME;
  return 0;
}

static int ps_chicken_begin_CHAT(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_CHICKEN_PHASE_CHAT;
  SPR->phasetime=PS_CHICKEN_CHAT_TIME;
  PS_SFX_CHICKEN_CHAT
  return 0;
}

/* Select new phase.
 */

static int ps_chicken_advance_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {

    case PS_CHICKEN_PHASE_IDLE: {
        switch (rand()%3) {
          case 0: return ps_chicken_begin_WALK(spr,game);
          case 1: return ps_chicken_begin_PECK(spr,game);
          case 2: return ps_chicken_begin_CHAT(spr,game);
        }
      } break;
    
    case PS_CHICKEN_PHASE_WALK:
    case PS_CHICKEN_PHASE_PECK:
    case PS_CHICKEN_PHASE_CHAT:
    case PS_CHICKEN_PHASE_SQUAWK:
    default: return ps_chicken_begin_IDLE(spr,game);
  }
  return 0;
}

/* Update in WALK phase.
 */

static int ps_chicken_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animtime)>=PS_CHICKEN_WALK_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_CHICKEN_WALK_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  spr->x+=SPR->walkdx;
  spr->y+=SPR->walkdy;
  
  if ((spr->x<PS_CHICKEN_EDGE_MARGIN)&&(SPR->walkdx<0.0)) {
    SPR->walkdx=-SPR->walkdx;
  } else if ((spr->x>PS_SCREENW-PS_CHICKEN_EDGE_MARGIN)&&(SPR->walkdx>0.0)) {
    SPR->walkdx=-SPR->walkdx;
  }
  if ((spr->y<PS_CHICKEN_EDGE_MARGIN)&&(SPR->walkdy<0.0)) {
    SPR->walkdy=-SPR->walkdy;
  } else if ((spr->y>PS_SCREENH-PS_CHICKEN_EDGE_MARGIN)&&(SPR->walkdy>0.0)) {
    SPR->walkdy=-SPR->walkdy;
  }

  return 0;
}

/* Update.
 */

static int _ps_chicken_update(struct ps_sprite *spr,struct ps_game *game) {

  if (--(SPR->phasetime)<=0) {
    if (ps_chicken_advance_phase(spr,game)<0) return -1;
  } else switch (SPR->phase) {
    case PS_CHICKEN_PHASE_IDLE: break;
    case PS_CHICKEN_PHASE_WALK: if (ps_chicken_update_WALK(spr,game)<0) return -1; break;
    case PS_CHICKEN_PHASE_CHAT: break;
    case PS_CHICKEN_PHASE_PECK: break;
    case PS_CHICKEN_PHASE_SQUAWK: break;
  }

  return 0;
}

/* Draw.
 */

static int _ps_chicken_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=0x80;
  vtxv->pg=0x80;
  vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=(SPR->facedx>0)?AKGL_XFORM_FLOP:AKGL_XFORM_NONE;

  switch (SPR->phase) {
    case PS_CHICKEN_PHASE_IDLE: break;
    case PS_CHICKEN_PHASE_WALK: switch (SPR->animframe) {
        case 0: vtxv->tileid+=0x0f; break;
        case 2: vtxv->tileid+=0x10; break;
      } break;
    case PS_CHICKEN_PHASE_CHAT: vtxv->tileid+=0x01; break;
    case PS_CHICKEN_PHASE_PECK: vtxv->tileid+=0x12; break;
    case PS_CHICKEN_PHASE_SQUAWK: vtxv->tileid+=0x02; break;
  }
  
  return 1;
}

/* Lay egg.
 */

static int ps_chicken_lay_egg(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprite *egg=ps_sprite_new(&ps_sprtype_egg);
  if (!egg) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_KEEPALIVE,egg)<0) {
    ps_sprite_del(egg);
    return -1;
  }
  ps_sprite_del(egg);

  if (ps_game_set_group_mask_for_sprite(game,egg,
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_SOLID)|
    (1<<PS_SPRGRP_PHYSICS)|
  0)<0) return -1;

  egg->tsid=0x04;
  egg->tileid=0x7f;
  egg->opacity=0xff;
  egg->impassable=spr->impassable;

  // Wee randomization to ensure we don't get two eggs in precisely the same location.
  // If that happens in a horizontal corridor, physics gives us some unpleasant twitch.
  double randomx=(rand()%100)/100.0;
  double randomy=(rand()%100)/100.0;

  egg->x=spr->x+PS_TILESIZE*-SPR->facedx+randomx;
  egg->y=spr->y+randomy;
  
  return 0;
}

/* Test whether to lay an egg, minding the global population limit.
 */

static int ps_chicken_tally_population(const struct ps_game *game) {
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_SOLID; // Probably the smallest group chickens are always in.
  int c=0,i=grp->sprc;
  while (i-->0) {
    const struct ps_sprite *spr=grp->sprv[i];
    if (spr->type==&ps_sprtype_chicken) c++;
    else if (spr->type==&ps_sprtype_egg) c++; // We disregard grandma's advice and count these too.
  }
  return c;
}

static int ps_chicken_ok_to_lay_egg(const struct ps_sprite *spr,const struct ps_game *game) {
  return (ps_chicken_tally_population(game)<PS_CHICKEN_POPULATION_LIMIT);
}

/* Hurt.
 */

static int ps_chicken_assailant_should_cause_egg(struct ps_sprite *assailant) {
  if (!assailant) return 1;
  if (assailant->type==&ps_sprtype_boxingglove) return 0;
  if (assailant->type==&ps_sprtype_killozap) return 0;
  if (assailant->type==&ps_sprtype_flamethrower) return 0;
  return 1;
}

static int _ps_chicken_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->phase==PS_CHICKEN_PHASE_SQUAWK) {
    return 0;
  }

  if (!ps_chicken_assailant_should_cause_egg(assailant)) {
    if (ps_game_decorate_monster_death(game,spr->x,spr->y)<0) return -1;
    if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    if (ps_game_report_kill(game,assailant,spr)<0) return -1;
    return 0;
  }

  PS_SFX_CHICKEN_SQUAWK

  SPR->phase=PS_CHICKEN_PHASE_SQUAWK;
  SPR->phasetime=PS_CHICKEN_SQUAWK_TIME;

  if (ps_chicken_ok_to_lay_egg(spr,game)) {
    if (ps_chicken_lay_egg(spr,game)<0) return -1;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_chicken={
  .name="chicken",
  .objlen=sizeof(struct ps_sprite_chicken),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_chicken_init,
  .del=_ps_chicken_del,
  .update=_ps_chicken_update,
  .draw=_ps_chicken_draw,
  
  .hurt=_ps_chicken_hurt,

};
