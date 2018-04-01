#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_SNAKE_INITIAL_LENGTH 3/*12*/
#define PS_SNAKE_INITIAL_OFFSET 12.0
#define PS_SNAKE_OFFSET 12.0
#define PS_SNAKE_TOO_CLOSE 8.0
#define PS_SNAKE_TOO_FAR 16.0
#define PS_SNAKE_SPEED 1.0
#define PS_SNAKE_DDT 0.01
#define PS_SNAKE_DT_LIMIT 0.1
#define PS_SNAKE_FOLLOWER_DT 0.01
#define PS_SNAKE_INVINCIBLE_TIME 40

/* Private sprite object.
 */

struct ps_sprite_snake {
  struct ps_sprite hdr;
  int startup;
  struct ps_sprgrp *leader;
  double t;
  double dt;
  int invincible;
};

#define SPR ((struct ps_sprite_snake*)spr)

/* Delete.
 */

static void _ps_snake_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->leader);
  ps_sprgrp_del(SPR->leader);
}

/* Initialize.
 */

static int _ps_snake_init(struct ps_sprite *spr) {
  SPR->startup=1;
  if (!(SPR->leader=ps_sprgrp_new())) return -1;
  return 0;
}

/* Configure.
 */

static int _ps_snake_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_snake_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_snake_configure(), for editor.
  return 0;
}

/* Test role.
 */

static int ps_snake_is_leader(const struct ps_sprite *spr) {
  if (!SPR->leader) return 1;
  if (!SPR->leader->sprc) return 1;
  return 0;
}

/* Set leader.
 */

static int ps_snake_set_leader(struct ps_sprite *spr,struct ps_sprite *leader) {
  if (!spr||(spr->type!=&ps_sprtype_snake)) return -1;
  ps_sprgrp_clear(SPR->leader);
  if (leader) {
    if (leader->type!=&ps_sprtype_snake) return -1;
    if (ps_sprgrp_add_sprite(SPR->leader,leader)<0) return -1;
    SPR->startup=0; // Important! If it has a leader, don't spawn followers for it.
  }
  return 0;
}

/* Spawn children (deferred init).
 */

static int ps_snake_spawn_children(struct ps_sprite *spr,struct ps_game *game) {

  uint32_t grpmask=ps_game_get_group_mask_for_sprite(game,spr);
  struct ps_sprite *leader=spr;
  double advancet=M_PI;
  double advancedt=0.8;
  int i=PS_SNAKE_INITIAL_LENGTH; while (i-->0) {
  
    struct ps_sprite *child=ps_sprite_new(&ps_sprtype_snake);
    if (!child) return -1;
    if (ps_game_set_group_mask_for_sprite(game,child,grpmask)<0) {
      ps_sprite_del(child);
      return -1;
    }
    ps_sprite_del(child);

    child->x=leader->x+sin(advancet)*PS_SNAKE_INITIAL_OFFSET;
    child->y=leader->y-cos(advancet)*PS_SNAKE_INITIAL_OFFSET;
    child->tsid=leader->tsid;
    child->tileid=leader->tileid;
    child->radius=leader->radius;
    child->shape=leader->shape;
    child->layer=leader->layer+1;
    child->impassable=leader->impassable;
    
    if (ps_snake_set_leader(child,leader)<0) return -1;
    leader=child;
    advancet+=advancedt;
    advancedt-=0.05;
  }

  return 0;
}

/* Update leader.
 */

static int ps_snake_update_leader(struct ps_sprite *spr,struct ps_game *game) {

  double ddt=((rand()%1000-500)*PS_SNAKE_DDT)/1000.0;
  SPR->dt+=ddt;
  if (SPR->dt<-PS_SNAKE_DT_LIMIT) SPR->dt=-PS_SNAKE_DT_LIMIT;
  if (SPR->dt>PS_SNAKE_DT_LIMIT) SPR->dt=PS_SNAKE_DT_LIMIT;

  SPR->t+=SPR->dt;
  while (SPR->t<0.0) SPR->t+=M_PI*2.0;
  while (SPR->t>M_PI*2.0) SPR->t-=M_PI*2.0;

  spr->x+=sin(SPR->t)*PS_SNAKE_SPEED;
  spr->y-=cos(SPR->t)*PS_SNAKE_SPEED;

  return 0;
}

/* Update follower.
 */

static double ps_approach_angle(double dst,double src,double limit) {
  double d=dst-src;
  while (d>M_PI) d-=M_PI*2.0;
  while (d<-M_PI) d+=M_PI*2.0;
  if (d>=limit) return src+limit;
  if (d<=-limit) return src-limit;
  return dst;
}

static int ps_snake_update_follower(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *leader=SPR->leader->sprv[0];
  struct ps_sprite_snake *LEADER=(struct ps_sprite_snake*)leader;

  SPR->t=atan2(leader->x-spr->x,spr->y-leader->y);

  double dx=leader->x-spr->x;
  double dy=leader->y-spr->y;
  double distance=sqrt(dx*dx+dy*dy);
  if (distance<PS_SNAKE_TOO_CLOSE) {
  } else if (distance>PS_SNAKE_TOO_FAR) {
    if (ps_snake_set_leader(spr,0)<0) return -1;
  } else {
    spr->x+=sin(SPR->t)*PS_SNAKE_SPEED;
    spr->y-=cos(SPR->t)*PS_SNAKE_SPEED;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_snake_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->startup) {
    if (ps_snake_spawn_children(spr,game)<0) return -1;
    SPR->startup=0;
  }

  if (SPR->invincible>0) {
    SPR->invincible--;
  }

  if (ps_snake_is_leader(spr)) {
    if (ps_snake_update_leader(spr,game)<0) return -1;
  } else {
    if (ps_snake_update_follower(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_snake_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid+(ps_snake_is_leader(spr)?0:0x10);
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=(SPR->t*255.0)/(M_PI*2.0);
  vtxv->xform=AKGL_XFORM_NONE;

  if (SPR->invincible) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*255)/PS_SNAKE_INVINCIBLE_TIME;
  }
  
  return 1;
}

/* Find all snakes connected to this one, and make them all invincible.
 */

static int ps_snake_make_forward_chain_invincible(struct ps_sprite *spr) {
  while (1) {
    if (SPR->invincible) return 0;
    SPR->invincible=PS_SNAKE_INVINCIBLE_TIME;
    if (!SPR->leader) return 0;
    if (SPR->leader->sprc<1) return 0;
    spr=SPR->leader->sprv[0];
  }
}

static int ps_snake_is_follower(const struct ps_sprite *spr,const struct ps_sprite *leader) {
  while (1) {
    if (spr==leader) return 1;
    if (!SPR->leader) return 0;
    if (SPR->leader->sprc<1) return 0;
    spr=SPR->leader->sprv[0];
  }
}

static int ps_snake_make_relatives_invincible(struct ps_sprite *focus,struct ps_game *game) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HEROHAZARD;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *spr=grp->sprv[i];
    if (spr->type!=&ps_sprtype_snake) continue;
    if (ps_snake_is_follower(spr,focus)) {
      if (ps_snake_make_forward_chain_invincible(spr)<0) return -1;
    }
  }
  if (ps_snake_make_forward_chain_invincible(focus)<0) return -1;
  return 0;
}

/* Hurt.
 */

static int _ps_snake_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->invincible) return 0;

  if (ps_snake_make_relatives_invincible(spr,game)<0) return -1;

  if (ps_sprite_kill_later(spr,game)<0) return -1;
  if (ps_game_decorate_monster_death(game,spr->x,spr->y)<0) return -1;
  ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr);
  if (ps_game_report_kill(game,assailant,spr)<0) return -1;
  
  return 0;
}

/* Set switch.
 */

static int _ps_snake_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_snake={
  .name="snake",
  .objlen=sizeof(struct ps_sprite_snake),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_snake_init,
  .del=_ps_snake_del,
  .configure=_ps_snake_configure,
  .get_configure_argument_name=_ps_snake_get_configure_argument_name,
  .update=_ps_snake_update,
  .draw=_ps_snake_draw,
  
  .hurt=_ps_snake_hurt,
  //.set_switch=_ps_snake_set_switch,

};
