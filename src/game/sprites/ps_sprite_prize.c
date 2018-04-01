#include "ps.h"
#include "game/ps_game.h"
#include "game/ps_sprite.h"
#include "game/ps_sound_effects.h"
#include "ps_sprite_hero.h"
#include "util/ps_geometry.h"

#define PS_PRIZE_FRAME_TIME 7
#define PS_PRIZE_FRAME_COUNT 4
#define PS_PRIZE_TTL 600
#define PS_PRIZE_FADE_TIME 120
#define PS_PRIZE_SPEED 3

/* Private sprite object.
 */

struct ps_sprite_prize {
  struct ps_sprite hdr;
  uint16_t tileid_base;
  int counter,frame;
  int ttl;
  int dx,dy;
};

#define SPR ((struct ps_sprite_prize*)spr)

/* Delete.
 */

static void _ps_prize_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_prize_init(struct ps_sprite *spr) {

  SPR->tileid_base=0x04df;
  spr->tileid=SPR->tileid_base;
  SPR->ttl=PS_PRIZE_TTL;

  return 0;
}

/* Configure.
 */

static int _ps_prize_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  SPR->tileid_base=spr->tileid;
  return 0;
}

/* Update.
 */

static int _ps_prize_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Animate. */
  if (++(SPR->counter)>=PS_PRIZE_FRAME_TIME) {
    SPR->counter=0;
    if (++(SPR->frame)>=PS_PRIZE_FRAME_COUNT) {
      SPR->frame=0;
    }
    switch (SPR->frame) {
      case 0: spr->tileid=SPR->tileid_base+0x00; break;
      case 1: spr->tileid=SPR->tileid_base+0x10; break;
      case 2: spr->tileid=SPR->tileid_base+0x20; break;
      case 3: spr->tileid=SPR->tileid_base+0x10; break;
    }
  }

  /* Fade out and disappear. */
  SPR->ttl--;
  if (SPR->ttl<=0) {
    return ps_sprite_kill_later(spr,game);
  }
  if (SPR->ttl<PS_PRIZE_FADE_TIME) {
    spr->opacity=(SPR->ttl*0xff)/PS_PRIZE_FADE_TIME;
  }

  /* Get picked up. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;
    if (ps_sprites_collide(spr,hero)) {
      if (ps_hero_add_state(hero,PS_HERO_STATE_HEAL,game)<0) return -1;
      if (ps_sprite_kill_later(spr,game)<0) return -1;
      return 0;
    }
  }

  /* Get flung. */
  spr->x+=SPR->dx*PS_PRIZE_SPEED;
  spr->y+=SPR->dy*PS_PRIZE_SPEED;

  /* Force on-screen. */
  if (spr->x<spr->radius) spr->x=spr->radius;
  else if (spr->x>PS_SCREENW-spr->radius) spr->x=PS_SCREENW-spr->radius;
  if (spr->y<spr->radius) spr->y=spr->radius;
  else if (spr->y>PS_SCREENH-spr->radius) spr->y=PS_SCREENH-spr->radius;

  return 0;
}

/* Hurt.
 */

static int _ps_prize_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
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

const struct ps_sprtype ps_sprtype_prize={
  .name="prize",
  .objlen=sizeof(struct ps_sprite_prize),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_prize_init,
  .del=_ps_prize_del,
  .configure=_ps_prize_configure,
  .update=_ps_prize_update,
  
  .hurt=_ps_prize_hurt,

};

/* Fling.
 */
 
int ps_prize_fling(struct ps_sprite *spr,int dir) {
  if (!spr) return -1;
  if (spr->type!=&ps_sprtype_prize) return -1;
  struct ps_vector d=ps_vector_from_direction(dir);
  SPR->dx=d.dx;
  SPR->dy=d.dy;
  return 0;
}
