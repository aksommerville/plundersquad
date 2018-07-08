#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_GIRAFFE_VERTEBRA_COUNT   6
#define PS_GIRAFFE_VERTEBRA_LENGTH  8
#define PS_GIRAFFE_NECK_LENGTH_MIN  8
#define PS_GIRAFFE_NECK_LENGTH_MAX (PS_GIRAFFE_VERTEBRA_COUNT*PS_GIRAFFE_VERTEBRA_LENGTH)
#define PS_GIRAFFE_NECK_ANGLE_MIN   0.0
#define PS_GIRAFFE_NECK_ANGLE_MAX  (M_PI/2.0)
#define PS_GIRAFFE_WALK_ANIM_TIME   8
#define PS_GIRAFFE_WALK_ANIM_COUNT  4
#define PS_GIRAFFE_WALK_SPEED       0.5
#define PS_GIRAFFE_INVINCIBLE_TIME  60

#define PS_GIRAFFE_PHASE_IDLE         0
#define PS_GIRAFFE_PHASE_WALK         1
#define PS_GIRAFFE_PHASE_DRAWBACK     2
#define PS_GIRAFFE_PHASE_AIM          3
#define PS_GIRAFFE_PHASE_FIRE         4
#define PS_GIRAFFE_PHASE_POSTDRAWBACK 5
#define PS_GIRAFFE_PHASE_POSTAIM      6
#define PS_GIRAFFE_PHASE_EXTEND       7

#define PS_GIRAFFE_IDLE_TIME_MIN         20
#define PS_GIRAFFE_IDLE_TIME_MAX        100
#define PS_GIRAFFE_WALK_TIME_MIN         30
#define PS_GIRAFFE_WALK_TIME_MAX        180
#define PS_GIRAFFE_DRAWBACK_TIME         60
#define PS_GIRAFFE_AIM_TIME              60
#define PS_GIRAFFE_FIRE_TIME             15
#define PS_GIRAFFE_POSTDRAWBACK_TIME     80
#define PS_GIRAFFE_POSTAIM_TIME          60
#define PS_GIRAFFE_EXTEND_TIME           60

/* Private sprite object.
 */

struct ps_sprite_giraffe {
  struct ps_sprite hdr;
  
  int facedx; // -1,1

  int neck_length;
  double neck_angle; // Angle doesn't change much, so cache its sine and cosine.
  double neck_angle_cos;
  double neck_angle_sin;
  uint8_t neck_angle_akgl;

  int walk_anim_timer;
  int walk_anim_frame;
  double walkdx,walkdy;

  int phase;
  int phase_time;
  int phase_limit;

  int hp;
  int invincible;
};

#define SPR ((struct ps_sprite_giraffe*)spr)

/* Recalculate cached derivitives of neck_angle.
 */

static void ps_giraffe_neck_angle_changed(struct ps_sprite *spr) {
  while (SPR->neck_angle<0.0) SPR->neck_angle+=M_PI*2.0;
  while (SPR->neck_angle>=M_PI*2.0) SPR->neck_angle-=M_PI*2.0;
  SPR->neck_angle_cos=cos(SPR->neck_angle);
  SPR->neck_angle_sin=sin(SPR->neck_angle);
  SPR->neck_angle_akgl=(SPR->neck_angle*255.0)/(M_PI*2.0);
}

/* Delete.
 */

static void _ps_giraffe_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_giraffe_init(struct ps_sprite *spr) {

  SPR->facedx=(rand()&1)?-1:1;
  SPR->neck_length=PS_GIRAFFE_NECK_LENGTH_MAX;
  SPR->neck_angle=0.8;
  ps_giraffe_neck_angle_changed(spr);

  SPR->phase=PS_GIRAFFE_PHASE_EXTEND; // Leave times zero; will enter IDLE phase at update.

  SPR->hp=2;

  return 0;
}

/* Configure.
 */

static int _ps_giraffe_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_giraffe_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_giraffe_configure(), for editor.
  return 0;
}

/* During FIRE phase, look for anything we might be headbutting.
 * The head is way outside our physical body, so we do this check outside regular physics.
 */

static int ps_giraffe_check_headbutt(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_fbox headbox;
  headbox.n=spr->y-(PS_TILESIZE>>1);
  headbox.s=spr->y;
  if (SPR->facedx<0) {
    headbox.e=spr->x-SPR->neck_length;
    headbox.w=headbox.e-(PS_TILESIZE>>1);
  } else {
    headbox.w=spr->x+SPR->neck_length;
    headbox.e=headbox.w+(PS_TILESIZE>>1);
  }

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->type!=&ps_sprtype_hero) continue;
    if (ps_sprite_collide_fbox(victim,&headbox)) {
      if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
    }
  }
  return 0;
}

/* Update, WALK.
 */

static int ps_giraffe_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  /* Animate. */
  if (++(SPR->walk_anim_timer)>=PS_GIRAFFE_WALK_ANIM_TIME) {
    SPR->walk_anim_timer=0;
    if (++(SPR->walk_anim_frame)>=PS_GIRAFFE_WALK_ANIM_COUNT) {
      SPR->walk_anim_frame=0;
    }
  }

  spr->x+=SPR->walkdx;
  spr->y+=SPR->walkdy;
  
  return 0;
}

/* Update, neck adjustment phases.
 */

static int ps_giraffe_update_DRAWBACK(struct ps_sprite *spr,struct ps_game *game) {
  // This is also POSTDRAWBACK.
  const int neckrange=PS_GIRAFFE_NECK_LENGTH_MAX-PS_GIRAFFE_NECK_LENGTH_MIN;
  SPR->neck_length=PS_GIRAFFE_NECK_LENGTH_MAX-(SPR->phase_time*neckrange)/SPR->phase_limit;
  return 0;
}

static int ps_giraffe_update_AIM(struct ps_sprite *spr,struct ps_game *game) {
  const double range=PS_GIRAFFE_NECK_ANGLE_MAX-PS_GIRAFFE_NECK_ANGLE_MIN;
  SPR->neck_angle=PS_GIRAFFE_NECK_ANGLE_MIN+(SPR->phase_time*range)/SPR->phase_limit;
  ps_giraffe_neck_angle_changed(spr);
  return 0;
}

static int ps_giraffe_update_FIRE(struct ps_sprite *spr,struct ps_game *game) {
  const int neckrange=PS_GIRAFFE_NECK_LENGTH_MAX-PS_GIRAFFE_NECK_LENGTH_MIN;
  SPR->neck_length=PS_GIRAFFE_NECK_LENGTH_MIN+(SPR->phase_time*neckrange)/SPR->phase_limit;
  if (ps_giraffe_check_headbutt(spr,game)<0) return -1;
  return 0;
}

static int ps_giraffe_update_POSTAIM(struct ps_sprite *spr,struct ps_game *game) {
  const double range=PS_GIRAFFE_NECK_ANGLE_MAX-PS_GIRAFFE_NECK_ANGLE_MIN;
  SPR->neck_angle=PS_GIRAFFE_NECK_ANGLE_MAX-(SPR->phase_time*range)/SPR->phase_limit;
  ps_giraffe_neck_angle_changed(spr);
  return 0;
}

static int ps_giraffe_update_EXTEND(struct ps_sprite *spr,struct ps_game *game) {
  const int neckrange=PS_GIRAFFE_NECK_LENGTH_MAX-PS_GIRAFFE_NECK_LENGTH_MIN;
  SPR->neck_length=PS_GIRAFFE_NECK_LENGTH_MIN+(SPR->phase_time*neckrange)/SPR->phase_limit;
  return 0;
}

/* Begin phase.
 */

static int ps_giraffe_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_IDLE;
  SPR->phase_limit=PS_GIRAFFE_IDLE_TIME_MIN+rand()%(PS_GIRAFFE_IDLE_TIME_MAX-PS_GIRAFFE_IDLE_TIME_MIN);
  SPR->neck_angle=0.0;
  ps_giraffe_neck_angle_changed(spr);
  return 0;
}

static int ps_giraffe_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_WALK;
  SPR->phase_limit=PS_GIRAFFE_WALK_TIME_MIN+rand()%(PS_GIRAFFE_WALK_TIME_MAX-PS_GIRAFFE_WALK_TIME_MIN);
  
  double t=(rand()%6282)/1000.0;
  SPR->walkdx=cos(t)*PS_GIRAFFE_WALK_SPEED;
  SPR->walkdy=-sin(t)*PS_GIRAFFE_WALK_SPEED;

  // If close to screen edge, walk toward center.
  const int horzmargin=(PS_SCREENW/5);
  const int vertmargin=(PS_SCREENH/3);
  if (spr->x<horzmargin) {
    if (SPR->walkdx<0.0) SPR->walkdx=-SPR->walkdx;
  } else if (spr->x>PS_SCREENW-horzmargin) {
    if (SPR->walkdx>0.0) SPR->walkdx=-SPR->walkdx;
  }
  if (spr->y<vertmargin) {
    if (SPR->walkdy<0.0) SPR->walkdy=-SPR->walkdy;
  } else if (spr->y>PS_SCREENH-vertmargin) {
    if (SPR->walkdy>0.0) SPR->walkdy=-SPR->walkdy;
  }

  if (SPR->walkdx<0.0) {
    SPR->facedx=-1;
  } else {
    SPR->facedx=1;
  }
  return 0;
}

static int ps_giraffe_begin_DRAWBACK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_DRAWBACK;
  SPR->phase_limit=PS_GIRAFFE_DRAWBACK_TIME;
  return 0;
}

static int ps_giraffe_begin_AIM(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_AIM;
  SPR->phase_limit=PS_GIRAFFE_AIM_TIME;
  return 0;
}

static int ps_giraffe_begin_FIRE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_FIRE;
  SPR->phase_limit=PS_GIRAFFE_FIRE_TIME;
  return 0;
}

static int ps_giraffe_begin_POSTDRAWBACK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_POSTDRAWBACK;
  SPR->phase_limit=PS_GIRAFFE_POSTDRAWBACK_TIME;
  return 0;
}

static int ps_giraffe_begin_POSTAIM(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_POSTAIM;
  SPR->phase_limit=PS_GIRAFFE_POSTAIM_TIME;
  return 0;
}

static int ps_giraffe_begin_EXTEND(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GIRAFFE_PHASE_EXTEND;
  SPR->phase_limit=PS_GIRAFFE_EXTEND_TIME;
  return 0;
}

/* Choose phase at our own discretion.
 * IDLE, WALK, or DRAWBACK.
 */

static int ps_giraffe_choose_phase(struct ps_sprite *spr,struct ps_game *game) {
  int selection=rand()%10;
  if (selection<5) {
    return ps_giraffe_begin_WALK(spr,game);
  } else if (selection<6) {
    return ps_giraffe_begin_IDLE(spr,game);
  } else {
    return ps_giraffe_begin_DRAWBACK(spr,game);
  }
}

/* Advance phase.
 */

static int ps_giraffe_advance_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_GIRAFFE_PHASE_IDLE: return ps_giraffe_choose_phase(spr,game);
    case PS_GIRAFFE_PHASE_WALK: return ps_giraffe_begin_IDLE(spr,game);
    case PS_GIRAFFE_PHASE_DRAWBACK: return ps_giraffe_begin_AIM(spr,game);
    case PS_GIRAFFE_PHASE_AIM: return ps_giraffe_begin_FIRE(spr,game);
    case PS_GIRAFFE_PHASE_FIRE: return ps_giraffe_begin_POSTDRAWBACK(spr,game);
    case PS_GIRAFFE_PHASE_POSTDRAWBACK: return ps_giraffe_begin_POSTAIM(spr,game);
    case PS_GIRAFFE_PHASE_POSTAIM: return ps_giraffe_begin_EXTEND(spr,game);
    case PS_GIRAFFE_PHASE_EXTEND: return ps_giraffe_begin_IDLE(spr,game);
    default: return ps_giraffe_begin_IDLE(spr,game);
  }
}

/* Update, dispatch on phase.
 */

static int ps_giraffe_update_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_GIRAFFE_PHASE_WALK: return ps_giraffe_update_WALK(spr,game);
    case PS_GIRAFFE_PHASE_DRAWBACK: return ps_giraffe_update_DRAWBACK(spr,game);
    case PS_GIRAFFE_PHASE_AIM: return ps_giraffe_update_AIM(spr,game);
    case PS_GIRAFFE_PHASE_FIRE: return ps_giraffe_update_FIRE(spr,game);
    case PS_GIRAFFE_PHASE_POSTDRAWBACK: return ps_giraffe_update_DRAWBACK(spr,game);
    case PS_GIRAFFE_PHASE_POSTAIM: return ps_giraffe_update_POSTAIM(spr,game);
    case PS_GIRAFFE_PHASE_EXTEND: return ps_giraffe_update_EXTEND(spr,game);
  }
  return 0;
}

/* Update.
 */

static int _ps_giraffe_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->invincible>0) SPR->invincible--;

  if (++(SPR->phase_time)>=SPR->phase_limit) {
    SPR->phase_time=0;
    if (ps_giraffe_advance_phase(spr,game)<0) return -1;
  } else {
    if (ps_giraffe_update_phase(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_giraffe_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=2+PS_GIRAFFE_VERTEBRA_COUNT;
  if (vtxc>vtxa) return vtxc;

  struct akgl_vtx_maxtile *vtx_body=vtxv;
  struct akgl_vtx_maxtile *vtx_neck=vtxv+1;
  struct akgl_vtx_maxtile *vtx_head=vtxv+1+PS_GIRAFFE_VERTEBRA_COUNT;

  vtx_body->x=spr->x;
  vtx_body->y=spr->y;
  vtx_body->tileid=spr->tileid;
  vtx_body->size=PS_TILESIZE;
  vtx_body->pr=0x80;
  vtx_body->pg=0x80;
  vtx_body->pb=0x80;
  vtx_body->a=0xff;
  vtx_body->t=0;
  vtx_body->xform=(SPR->facedx<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;  

  if (SPR->invincible) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*255)/PS_GIRAFFE_INVINCIBLE_TIME;
  } else {
    vtxv->tr=0x00;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=0x00;
  }

  if (SPR->phase==PS_GIRAFFE_PHASE_WALK) {
    switch (SPR->walk_anim_frame) {
      case 1: vtx_body->tileid=spr->tileid+0x10; break;
      case 3: vtx_body->tileid=spr->tileid+0x11; break;
    }
  }

  int i; for (i=vtxc;i-->1;) memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));

  int basex=spr->x+(PS_TILESIZE>>2)*SPR->facedx;;
  int basey=spr->y-(PS_TILESIZE*3)/8;

  uint8_t t;
  if (SPR->facedx<0) t=255-SPR->neck_angle_akgl;
  else t=SPR->neck_angle_akgl;

  vtx_head->tileid=spr->tileid+3;
  vtx_head->x=basex+SPR->neck_length*SPR->neck_angle_sin*SPR->facedx;
  vtx_head->y=basey-SPR->neck_length*SPR->neck_angle_cos;
  vtx_head->t=t;
  int neckdx=vtx_head->x-basex;
  int neckdy=vtx_head->y-basey;

  for (i=0;i<PS_GIRAFFE_VERTEBRA_COUNT;i++) {
    vtx_neck[i].tileid=spr->tileid+2;
    vtx_neck[i].x=basex+(neckdx*i)/PS_GIRAFFE_VERTEBRA_COUNT;
    vtx_neck[i].y=basey+(neckdy*i)/PS_GIRAFFE_VERTEBRA_COUNT;
    vtx_neck[i].t=t;
  }
  
  return vtxc;
}

/* Hurt.
 */

static int _ps_giraffe_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->invincible) return 0;

  if (--(SPR->hp)>0) {
    PS_SFX_MONSTER_HURT
    SPR->invincible=PS_GIRAFFE_INVINCIBLE_TIME;
    return 0;
  }

  PS_SFX_MONSTER_DEAD
  SPR->invincible=INT_MAX;  
  if (ps_game_create_fireworks(game,spr->x,spr->y)<0) return -1;
  if (ps_game_create_prize(game,spr->x,spr->y)<0) return -1;
  if (ps_sprite_kill_later(spr,game)<0) return -1;
  if (ps_game_check_deathgate(game)<0) return -1;
  if (ps_game_report_kill(game,assailant,spr)<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_giraffe={
  .name="giraffe",
  .objlen=sizeof(struct ps_sprite_giraffe),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_giraffe_init,
  .del=_ps_giraffe_del,
  .configure=_ps_giraffe_configure,
  .get_configure_argument_name=_ps_giraffe_get_configure_argument_name,
  .update=_ps_giraffe_update,
  .draw=_ps_giraffe_draw,
  
  .hurt=_ps_giraffe_hurt,

};
