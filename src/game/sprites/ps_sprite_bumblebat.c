#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include <math.h>

#define PS_BUMBLEBAT_ANIM_TIME 8
#define PS_BUMBLEBAT_ORBIT_INCREMENT 0.05
#define PS_BUMBLEBAT_TARGET_DISTANCE (PS_TILESIZE*2)
#define PS_BUMBLEBAT_SPEED 2.0
#define PS_BUMBLEBAT_RECONSIDER_TIME_MIN 120
#define PS_BUMBLEBAT_RECONSIDER_TIME_MAX 300

/* Private sprite object.
 */

struct ps_sprite_bumblebat {
  struct ps_sprite hdr;
  int animtime;
  struct ps_sprgrp *target;
  double targetangle;
  double targetdistance;
  int reconsidertime;
};

#define SPR ((struct ps_sprite_bumblebat*)spr)

/* Delete.
 */

static void _ps_bumblebat_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->target);
  ps_sprgrp_del(SPR->target);
}

/* Set reconsider time.
 */

static void ps_bumblebat_set_reconsider_time(struct ps_sprite *spr) {
  SPR->reconsidertime=PS_BUMBLEBAT_RECONSIDER_TIME_MIN+rand()%(PS_BUMBLEBAT_RECONSIDER_TIME_MAX-PS_BUMBLEBAT_RECONSIDER_TIME_MIN+1);
}

/* Initialize.
 */

static int _ps_bumblebat_init(struct ps_sprite *spr) {

  if (!(SPR->target=ps_sprgrp_new())) return -1;
  SPR->targetangle=(rand()%628)/100.0;
  SPR->targetdistance=PS_BUMBLEBAT_TARGET_DISTANCE;

  ps_bumblebat_set_reconsider_time(spr);

  return 0;
}

/* Configure.
 */

static int _ps_bumblebat_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc) {
  return 0;
}

/* Chase the target.
 */

static int ps_bumblebat_update_chase(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *target) {

  SPR->targetangle+=PS_BUMBLEBAT_ORBIT_INCREMENT;
  if (SPR->targetangle>M_PI) SPR->targetangle-=M_PI*2.0;

  double dstx=target->x+SPR->targetdistance*cos(SPR->targetangle);
  double dsty=target->y-SPR->targetdistance*sin(SPR->targetangle);

  double dx=dstx-spr->x;
  if ((dx>=-PS_BUMBLEBAT_SPEED)&&(dx<=PS_BUMBLEBAT_SPEED)) spr->x=dstx;
  else if (dx<0.0) spr->x-=PS_BUMBLEBAT_SPEED;
  else spr->x+=PS_BUMBLEBAT_SPEED;
  double dy=dsty-spr->y;
  if ((dy>=-PS_BUMBLEBAT_SPEED)&&(dy<=PS_BUMBLEBAT_SPEED)) spr->y=dsty;
  else if (dy<0.0) spr->y-=PS_BUMBLEBAT_SPEED;
  else spr->y+=PS_BUMBLEBAT_SPEED;

  return 0;
}

/* Locate a new target.
 */

static int ps_bumblebat_locate_target(struct ps_sprite *spr,struct ps_game *game) {
  if (game->grpv[PS_SPRGRP_HERO].sprc<1) return 0;
  int p=rand()%game->grpv[PS_SPRGRP_HERO].sprc;
  struct ps_sprite *target=game->grpv[PS_SPRGRP_HERO].sprv[p];
  if (ps_sprgrp_clear(SPR->target)<0) return -1;
  if (ps_sprgrp_add_sprite(SPR->target,target)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_bumblebat_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animtime)>=PS_BUMBLEBAT_ANIM_TIME) {
    SPR->animtime=0;
    spr->tileid^=1;
  }

  if (--(SPR->reconsidertime)<=0) {
    ps_bumblebat_set_reconsider_time(spr);
    ps_sprgrp_clear(SPR->target);
  }

  if (SPR->target->sprc) {
    if (ps_bumblebat_update_chase(spr,game,SPR->target->sprv[0])<0) return -1;
  } else {
    if (ps_bumblebat_locate_target(spr,game)<0) return -1;
  }

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_bumblebat={
  .name="bumblebat",
  .objlen=sizeof(struct ps_sprite_bumblebat),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_bumblebat_init,
  .del=_ps_bumblebat_del,
  .configure=_ps_bumblebat_configure,
  .update=_ps_bumblebat_update,

};
