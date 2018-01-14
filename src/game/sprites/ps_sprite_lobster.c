#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_LOBSTER_PHASE_IDLE     0
#define PS_LOBSTER_PHASE_WALK     1
#define PS_LOBSTER_PHASE_KILL     2

// In 0..255, how far does each half of a claw open?
#define PS_LOBSTER_CLAW_ANGLE 40
#define PS_LOBSTER_CLAW_BASE_ANGLE 224

#define PS_LOBSTER_CLAW_OPEN_SPEED 3

#define PS_LOBSTER_ARM_UNIT_COUNT 10

#define PS_LOBSTER_CLAW_RADIUS 6

#define PS_LOBSTER_ARM_ADVANCE_SPEED 2.5
#define PS_LOBSTER_ARM_RETRACT_SPEED 2.0
#define PS_LOBSTER_ARM_REACH_LIMIT (PS_TILESIZE*5)

#define PS_LOBSTER_HEAD_STRETCH_LIMIT 3
#define PS_LOBSTER_TAIL_STRETCH_LIMIT 4

#define PS_LOBSTER_STRETCH_SPEED 4

// Head and tail offsets, most compacted position.
#define PS_LOBSTER_HEAD_OFFSETX -12
#define PS_LOBSTER_HEAD_OFFSETY   0
#define PS_LOBSTER_TAIL_OFFSETX  12
#define PS_LOBSTER_TAIL_OFFSETY   3

#define PS_LOBSTER_ARMR_HOMEX -8
#define PS_LOBSTER_ARMR_HOMEY -9
#define PS_LOBSTER_ARML_HOMEX -8
#define PS_LOBSTER_ARML_HOMEY  9

#define PS_LOBSTER_WALK_SPEED_MAX 2.0
#define PS_LOBSTER_WALK_TIME_MIN       30
#define PS_LOBSTER_WALK_TIME_MAX      120
#define PS_LOBSTER_WALK_DRAWDOWN_TIME  30

#define PS_LOBSTER_IDLE_TIME_MIN  60
#define PS_LOBSTER_IDLE_TIME_MAX 120

#define PS_LOBSTER_KILL_TIME_MIN  60
#define PS_LOBSTER_KILL_TIME_MAX 120

#define PS_LOBSTER_HP_MAX 5
#define PS_LOBSTER_INVINCIBLE_TIME 60

/* Private sprite object.
 */

struct ps_lobster_arm {
  double dx,dy; // position relative to lobster's center.
  double homex,homey; // default position (constant)
  int openness; // 0=closed .. 100=open
  int dopenness;
  struct ps_sprgrp *target;
};

struct ps_sprite_lobster {
  struct ps_sprite hdr;
  struct ps_lobster_arm arml; // left (front)
  struct ps_lobster_arm armr; // right (back)
  int stretch;
  int dstretch;
  int phase;
  int phasetime; // counts down to next phase
  double walkdx,walkdy; // normalized vector of walk angle
  double walkspeed;
  int flop;
  int killmidtime;
  int hp;
  int invincible;
};

#define SPR ((struct ps_sprite_lobster*)spr)

/* Delete.
 */

static void _ps_lobster_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->armr.target);
  ps_sprgrp_del(SPR->armr.target);
  ps_sprgrp_clear(SPR->arml.target);
  ps_sprgrp_del(SPR->arml.target);
}

/* Initialize.
 */

static int _ps_lobster_init(struct ps_sprite *spr) {

  if (!(SPR->armr.target=ps_sprgrp_new())) return -1;
  if (!(SPR->arml.target=ps_sprgrp_new())) return -1;

  SPR->armr.dopenness=PS_LOBSTER_CLAW_OPEN_SPEED;
  SPR->arml.dopenness=PS_LOBSTER_CLAW_OPEN_SPEED;
  SPR->armr.dx=SPR->armr.homex=PS_LOBSTER_ARMR_HOMEX;
  SPR->armr.dy=SPR->armr.homey=PS_LOBSTER_ARMR_HOMEY;
  SPR->arml.dx=SPR->arml.homex=PS_LOBSTER_ARML_HOMEX;
  SPR->arml.dy=SPR->arml.homey=PS_LOBSTER_ARML_HOMEY;

  SPR->dstretch=PS_LOBSTER_STRETCH_SPEED;
  SPR->hp=PS_LOBSTER_HP_MAX;

  return 0;
}

/* Configure.
 */

static int _ps_lobster_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Decide which direction to face.
 */

static int ps_lobster_adjust_flop(struct ps_sprite *spr,struct ps_game *game) {

  int heroleftc=0,herorightc=0;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->x<spr->x) heroleftc++;
    else herorightc++;
  }

  int newflop;
  if (heroleftc>herorightc) {
    newflop=0;
  } else if (heroleftc<herorightc) {
    newflop=1;
  } else if (spr->x<PS_SCREENW>>1) {
    newflop=1;
  } else {
    newflop=0;
  }
  if (newflop==SPR->flop) return 0;
  SPR->flop=newflop;

  SPR->armr.homex=-SPR->armr.homex;
  SPR->armr.dx=-SPR->armr.dx;
  SPR->arml.homex=-SPR->arml.homex;
  SPR->arml.dx=-SPR->arml.dx;

  return 0;
}

/* Retract arms.
 */
 
static int ps_lobster_retract_arm(struct ps_sprite *spr,struct ps_lobster_arm *arm,struct ps_game *game) {
  if ((arm->dx==arm->homex)&&(arm->dy==arm->homey)) return 0;
  double dx=arm->dx-arm->homex;
  double dy=arm->dy-arm->homey;
  double manhattan=((dx<0.0)?-dx:dx)+((dy<0.0)?-dy:dy);
  if (manhattan<=PS_LOBSTER_ARM_RETRACT_SPEED) {
    arm->dx=arm->homex;
    arm->dy=arm->homey;
    return 0;
  }
  double distance=sqrt(dx*dx+dy*dy);
  double udx=dx/distance;
  double udy=dy/distance;
  arm->dx-=udx*PS_LOBSTER_ARM_RETRACT_SPEED;
  arm->dy-=udy*PS_LOBSTER_ARM_RETRACT_SPEED;
  return 0;
}

static int ps_lobster_retract_arms(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_lobster_retract_arm(spr,&SPR->armr,game)<0) return -1;
  if (ps_lobster_retract_arm(spr,&SPR->arml,game)<0) return -1;
  return 0;
}

/* Update arm.
 */

static int ps_lobster_arm_update(struct ps_sprite *spr,struct ps_lobster_arm *arm,struct ps_game *game) {

  arm->openness+=arm->dopenness;
  if (arm->openness<0) {
    arm->openness=0;
    if (arm->dopenness<0) arm->dopenness=-arm->dopenness;
  } else if (arm->openness>100) {
    arm->openness=100;
    if (arm->dopenness>0) arm->dopenness=-arm->dopenness;
  }

  /* Since arms aren't part of the proper physical sprite, perform our own collision detection against heroes.
   */
  struct ps_sprgrp *heroes=game->grpv+PS_SPRGRP_HERO;
  int i=heroes->sprc; 
  if (i>0) {
    struct ps_circle clawcircle=ps_circle(spr->x+arm->dx,spr->y+arm->dy,PS_LOBSTER_CLAW_RADIUS);
    while (i-->0) {
      struct ps_sprite *hero=heroes->sprv[i];
      if (ps_sprite_collide_circle(hero,&clawcircle)) {
        if (ps_sprite_receive_damage(game,hero,spr)<0) return -1;
      }
    }
  }

  return 0;
}

/* Begin walking.
 */

static int ps_lobster_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_LOBSTER_PHASE_WALK;
  SPR->phasetime=PS_LOBSTER_WALK_TIME_MIN+(rand()%(PS_LOBSTER_WALK_TIME_MAX-PS_LOBSTER_WALK_TIME_MIN+1));
  
  double walkangle=(rand()%628)/100.0;
  SPR->walkdx=cos(walkangle);
  SPR->walkdy=sin(walkangle);

  /* If we're outside the middle half of either axis of the screen, point towards center. */
  if (spr->x<PS_SCREENW>>2) {
    if (SPR->walkdx<0.0) SPR->walkdx=-SPR->walkdx;
  } else if (spr->x>(PS_SCREENW*3)>>2) {
    if (SPR->walkdx>0.0) SPR->walkdx=-SPR->walkdx;
  }
  if (spr->y<PS_SCREENH>>2) {
    if (SPR->walkdy<0.0) SPR->walkdy=-SPR->walkdy;
  } else if (spr->y>(PS_SCREENH*3)>>2) {
    if (SPR->walkdy>0.0) SPR->walkdy=-SPR->walkdy;
  }

  SPR->walkspeed=PS_LOBSTER_WALK_SPEED_MAX;
  return 0;
}

/* Update in WALK phase.
 */

static int ps_lobster_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_lobster_retract_arms(spr,game)<0) return -1;

  if (SPR->phasetime<PS_LOBSTER_WALK_DRAWDOWN_TIME) {
    SPR->walkspeed=(SPR->phasetime*PS_LOBSTER_WALK_SPEED_MAX)/PS_LOBSTER_WALK_DRAWDOWN_TIME;
  }

  spr->x+=SPR->walkdx*SPR->walkspeed;
  spr->y+=SPR->walkdy*SPR->walkspeed;

  if (spr->x<0.0) spr->x=0.0; else if (spr->x>PS_SCREENW) spr->x=PS_SCREENW;
  if (spr->y<0.0) spr->y=0.0; else if (spr->y>PS_SCREENH) spr->y=PS_SCREENH;

  return 0;
}

/* Locate target for one arm.
 */

static int ps_lobster_arm_acquire_target(struct ps_sprite *spr,struct ps_lobster_arm *arm,struct ps_game *game,int dy) {
  ps_sprgrp_clear(arm->target);

  struct ps_sprite *target=0;
  double targetd=9999999999999.0;

  struct ps_sprgrp *candidates=game->grpv+PS_SPRGRP_HERO;
  int i=candidates->sprc; while (i-->0) {
    struct ps_sprite *candidate=candidates->sprv[i];

    if (dy<0) {
      if (candidate->y>=spr->y) continue;
    } else {
      if (candidate->y<spr->y) continue;
    }

    double cdx=candidate->x-spr->x;
    double cdy=candidate->y-spr->y;
    double cd=cdx*cdx+cdy*cdy;
    
    if (cd<targetd) {
      target=candidate;
      targetd=cd;
    }
  }

  if (target) {
    if (ps_sprgrp_add_sprite(arm->target,target)<0) return -1;
  }
  
  return 0;
}

/* Enter KILL phase.
 */

static int ps_lobster_begin_KILL(struct ps_sprite *spr,struct ps_game *game) {

  SPR->phase=PS_LOBSTER_PHASE_KILL;
  SPR->phasetime=PS_LOBSTER_KILL_TIME_MIN+(rand()%(PS_LOBSTER_KILL_TIME_MAX-PS_LOBSTER_KILL_TIME_MIN+1));
  SPR->killmidtime=SPR->phasetime>>1;

  if (ps_lobster_arm_acquire_target(spr,&SPR->arml,game,1)<0) return -1;
  if (ps_lobster_arm_acquire_target(spr,&SPR->armr,game,-1)<0) return -1;
  if (!SPR->arml.target->sprc&&!SPR->armr.target->sprc) {
    SPR->phasetime=0;
  }

  return 0;
}

/* Update in KILL phase (advance arms).
 */

static int ps_lobster_advance_arm(struct ps_sprite *spr,struct ps_lobster_arm *arm,struct ps_game *game) {
  if (arm->target->sprc<1) return 0;
  struct ps_sprite *target=arm->target->sprv[0];

  double armx=spr->x+arm->dx;
  double army=spr->y+arm->dy;
  double dx=target->x-armx;
  double dy=target->y-army;
  double distance=sqrt(dx*dx+dy*dy);
  if (distance<PS_LOBSTER_ARM_ADVANCE_SPEED) return 0;
  double udx=dx/distance;
  double udy=dy/distance;

  /* Advance only if we are not too stretched. */
  double totaldx=arm->dx-arm->homex; if (totaldx<0.0) totaldx=-totaldx;
  double totaldy=arm->dy-arm->homey; if (totaldy<0.0) totaldy=-totaldy;
  double totalmanhattan=totaldx+totaldy;
  if (totalmanhattan<PS_LOBSTER_ARM_REACH_LIMIT) {
    arm->dx+=udx*PS_LOBSTER_ARM_ADVANCE_SPEED;
    arm->dy+=udy*PS_LOBSTER_ARM_ADVANCE_SPEED;
  }

  return 0;
}

static int ps_lobster_update_KILL(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_lobster_advance_arm(spr,&SPR->armr,game)<0) return -1;
  if (ps_lobster_advance_arm(spr,&SPR->arml,game)<0) return -1;
  return 0;
}

/* Advance to next phase.
 */

static int ps_lobster_advance_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_LOBSTER_PHASE_IDLE: {
        if (ps_lobster_begin_WALK(spr,game)<0) return -1;
      } break;
    case PS_LOBSTER_PHASE_WALK: {
        if (ps_lobster_adjust_flop(spr,game)<0) return -1;
        if (ps_lobster_begin_KILL(spr,game)<0) return -1;
      } break;
    default: {
        SPR->phase=PS_LOBSTER_PHASE_IDLE;
        SPR->phasetime=PS_LOBSTER_IDLE_TIME_MIN+(rand()%(PS_LOBSTER_IDLE_TIME_MAX-PS_LOBSTER_IDLE_TIME_MIN+1));
      }
  }
  return 0;
}

/* Update.
 */

static int _ps_lobster_update(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_lobster_arm_update(spr,&SPR->armr,game)<0) return -1;
  if (ps_lobster_arm_update(spr,&SPR->arml,game)<0) return -1;

  SPR->stretch+=SPR->dstretch;
  if (SPR->stretch<0) {
    SPR->stretch=0;
    if (SPR->dstretch<0) SPR->dstretch=-SPR->dstretch;
  } else if (SPR->stretch>100) {
    SPR->stretch=100;
    if (SPR->dstretch>0) SPR->dstretch=-SPR->dstretch;
  }

  if (SPR->invincible>0) SPR->invincible--;

  if (--(SPR->phasetime)<=0) {
    if (ps_lobster_advance_phase(spr,game)<0) return -1;
  } else switch (SPR->phase) {
    case PS_LOBSTER_PHASE_WALK: if (ps_lobster_update_WALK(spr,game)<0) return -1; break;
    case PS_LOBSTER_PHASE_IDLE: if (ps_lobster_retract_arms(spr,game)<0) return -1; break;
    case PS_LOBSTER_PHASE_KILL: if (ps_lobster_update_KILL(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Draw arm.
 */

static int ps_lobster_arm_count_vertices(const struct ps_lobster_arm *arm) {
  if ((arm->dx==arm->homex)&&(arm->dy==arm->homey)) {
    return 2;
  } else {
    return PS_LOBSTER_ARM_UNIT_COUNT+2;
  }
}

static void ps_lobster_arm_draw(const struct ps_sprite *spr,const struct ps_lobster_arm *arm,struct akgl_vtx_maxtile *vtxv) {

  int armunitc=0;
  if ((arm->dx!=arm->homex)||(arm->dy!=arm->homey)) {
    armunitc=PS_LOBSTER_ARM_UNIT_COUNT;
  }
  struct akgl_vtx_maxtile *clawback=vtxv+armunitc;
  struct akgl_vtx_maxtile *clawfront=clawback+1;

  clawback->tileid+=5;
  clawfront->tileid+=4;

  clawfront->x=clawback->x+=arm->dx;
  clawfront->y=clawback->y+=arm->dy;

  clawback->t=(arm->openness*PS_LOBSTER_CLAW_ANGLE)/100;
  clawfront->t=256-clawback->t;
  clawback->t+=PS_LOBSTER_CLAW_BASE_ANGLE;
  clawfront->t+=PS_LOBSTER_CLAW_BASE_ANGLE;

  if (SPR->flop) {
    clawback->t+=128;
    clawfront->t+=128;
  }

  int basex=spr->x+arm->homex;
  int basey=spr->y+arm->homey;
  struct akgl_vtx_maxtile *armunit=vtxv;
  int i=armunitc; for (;i-->0;armunit++) {
    armunit->tileid+=3;
    armunit->x=basex+((clawfront->x-basex)*i)/armunitc;
    armunit->y=basey+((clawfront->y-basey)*i)/armunitc;
  }
}

/* Draw body.
 */

static void ps_lobster_body_draw(struct ps_sprite *spr,struct akgl_vtx_maxtile *vtxv) {

  struct akgl_vtx_maxtile *head=vtxv+1;
  struct akgl_vtx_maxtile *body=vtxv+0;
  struct akgl_vtx_maxtile *tail=vtxv+2;

  body->tileid+=1;
  tail->tileid+=2;

  head->x+=PS_LOBSTER_HEAD_OFFSETX;
  head->y+=PS_LOBSTER_HEAD_OFFSETY;
  tail->x+=PS_LOBSTER_TAIL_OFFSETX;
  tail->y+=PS_LOBSTER_TAIL_OFFSETY;
  head->x-=(SPR->stretch*PS_LOBSTER_HEAD_STRETCH_LIMIT)/100;
  tail->x+=(SPR->stretch*PS_LOBSTER_TAIL_STRETCH_LIMIT)/100;

  /* If we are flopped, swap head and tail horizontal position. */
  if (SPR->flop) {
    int tmp=head->x;
    head->x=tail->x;
    tail->x=tmp;
    head->xform=tail->xform=body->xform=AKGL_XFORM_FLOP;
  }
  
}

/* Prepare vertices with common elements.
 */

static void ps_lobster_vtxv_init(const struct ps_sprite *spr,struct akgl_vtx_maxtile *vtxv,int vtxc) {
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;

  if (SPR->invincible) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*255)/PS_LOBSTER_INVINCIBLE_TIME;
  } else {
    vtxv->tr=0x00;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=0x00;
  }
  
  vtxv->pr=0x80;
  vtxv->pg=0x80;
  vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  while (vtxc-->1) memcpy(vtxv+vtxc,vtxv,sizeof(struct akgl_vtx_maxtile));
}

/* Draw, coordination.
 */

static int _ps_lobster_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int armlvtxc=ps_lobster_arm_count_vertices(&SPR->arml);
  int armrvtxc=ps_lobster_arm_count_vertices(&SPR->armr);
  int bodyvtxc=3;
  int vtxc=armrvtxc+bodyvtxc+armlvtxc;
  if (vtxc>vtxa) return vtxc;

  ps_lobster_vtxv_init(spr,vtxv,vtxc);

  ps_lobster_arm_draw(spr,&SPR->armr,vtxv);
  ps_lobster_body_draw(spr,vtxv+armrvtxc);
  ps_lobster_arm_draw(spr,&SPR->arml,vtxv+armrvtxc+bodyvtxc);

  return vtxc;
}

/* Hurt.
 */

static int _ps_lobster_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->invincible) return 0;

  if (--(SPR->hp)>0) {
    PS_SFX_MONSTER_HURT
    SPR->invincible=PS_LOBSTER_INVINCIBLE_TIME;
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

const struct ps_sprtype ps_sprtype_lobster={
  .name="lobster",
  .objlen=sizeof(struct ps_sprite_lobster),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_lobster_init,
  .del=_ps_lobster_del,
  .configure=_ps_lobster_configure,
  .update=_ps_lobster_update,
  .draw=_ps_lobster_draw,
  
  .hurt=_ps_lobster_hurt,

};
