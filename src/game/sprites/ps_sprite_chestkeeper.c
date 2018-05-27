/* Monster with a treasure chest for a head and chainsaws for arms.
 * Disguises himself as a treasure chest and pops out when a hero gets close.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_CHESTKEEPER_BLOWCOVER_DISTANCE (PS_TILESIZE*2.0)
#define PS_CHESTKEEPER_BLOWCOVER_TIME 60
#define PS_CHESTKEEPER_BLOWCOVER_DISPERSAL_SPEED 1.0 /* maximum; attenuates as the transition ends */
#define PS_CHESTKEEPER_JAW_ANIMC 30
#define PS_CHESTKEEPER_ARM_LINKC 8
#define PS_CHESTKEEPER_ARM_LENGTH_HI (PS_TILESIZE*3.0)
#define PS_CHESTKEEPER_ARM_LENGTH_LO (PS_TILESIZE)
#define PS_CHESTKEEPER_ARM_LENGTH_SPEED 0.3
#define PS_CHESTKEEPER_ARM_ANGLE_SPEED 0.1
#define PS_CHESTKEEPER_ANIM_DELAY 10
#define PS_CHESTKEEPER_ANIM_COUNT 4
#define PS_CHESTKEEPER_BLADE_RADIUS 6.0
#define PS_CHESTKEEPER_WALK_TIME_LO  30
#define PS_CHESTKEEPER_WALK_TIME_HI  90
#define PS_CHESTKEEPER_WALK_SPEED_LO  0.5
#define PS_CHESTKEEPER_WALK_SPEED_HI  1.5
#define PS_CHESTKEEPER_HP 3
#define PS_CHESTKEEPER_INVINCIBLE_TIME 60

#define PS_TREASURECHEST_DAZZLE_LIMIT 20 /* 1/256 circle from center */
#define PS_TREASURECHEST_DAZZLE_RATE   2 /* 1/256 circle per frame */
#define PS_TREASURECHEST_DAZZLE_SIZE_MIN ((PS_TILESIZE*150)/100)
#define PS_TREASURECHEST_DAZZLE_SIZE_MAX ((PS_TILESIZE*200)/100)
#define PS_TREASURECHEST_DAZZLE_SIZE_PERIOD 50

/* Private sprite object.
 */

struct ps_chestkeeper_arm {
  double shoulderx; // relative to sprite
  double shouldery;
  double t;
  double dt;
  double length;
  double dlength;
};

struct ps_sprite_chestkeeper {
  struct ps_sprite hdr;
  int incognito;
  int blowcover_transition; // counts down during transition
  int jaw_animp;
  int animtime;
  int animframe;
  struct ps_chestkeeper_arm arml;
  struct ps_chestkeeper_arm armr;
  double walkdx,walkdy;
  int walktime;
  int hp;
  int invincible;
  int dazzlet; // All the dazzle jazz is copied straight from ps_sprite_treasurechest.c
  int dazzletd;
  int dazzlesizep;
  int dazzlesized;
};

#define SPR ((struct ps_sprite_chestkeeper*)spr)

/* Delete.
 */

static void _ps_chestkeeper_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_chestkeeper_init(struct ps_sprite *spr) {
  SPR->incognito=1;
  SPR->hp=PS_CHESTKEEPER_HP;

  SPR->arml.shoulderx=-6.0;
  SPR->arml.shouldery=-4.0;
  SPR->arml.t=M_PI/2.0;
  SPR->arml.dt=PS_CHESTKEEPER_ARM_ANGLE_SPEED*0.8;
  SPR->arml.length=PS_CHESTKEEPER_ARM_LENGTH_LO;
  SPR->arml.dlength=PS_CHESTKEEPER_ARM_LENGTH_SPEED;

  SPR->armr.shoulderx=6.0;
  SPR->armr.shouldery=-4.0;
  SPR->armr.t=(M_PI*3.0)/2.0;
  SPR->armr.dt=-PS_CHESTKEEPER_ARM_ANGLE_SPEED;
  SPR->armr.length=PS_CHESTKEEPER_ARM_LENGTH_LO;
  SPR->armr.dlength=PS_CHESTKEEPER_ARM_LENGTH_SPEED*0.8;
  
  SPR->dazzlet=0;
  SPR->dazzletd=PS_TREASURECHEST_DAZZLE_RATE;
  SPR->dazzlesized=1;
  
  return 0;
}

/* Configure.
 */

static int _ps_chestkeeper_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_chestkeeper_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_chestkeeper_configure(), for editor.
  return 0;
}

/* Is a hero nearby? Should I blow cover?
 */

static int ps_chestkeeper_should_blow_cover(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;

    double dx=hero->x-spr->x;
    double adx=(dx<0.0)?-dx:dx;
    if (adx>PS_CHESTKEEPER_BLOWCOVER_DISTANCE) continue;
    double dy=hero->y-spr->y;
    double ady=(dy<0.0)?-dy:dy;
    if (ady>PS_CHESTKEEPER_BLOWCOVER_DISTANCE) continue;
    double manhattan=adx+ady;
    if (manhattan>PS_CHESTKEEPER_BLOWCOVER_DISTANCE*1.4142) continue; // sqrt(2.0)
    if (manhattan>PS_CHESTKEEPER_BLOWCOVER_DISTANCE*0.07071) { // sqrt(0.5)
      double distance=sqrt(dx*dx+dy*dy);
      if (distance>PS_CHESTKEEPER_BLOWCOVER_DISTANCE) continue;
    }

    return 1;
  }
  return 0;
}

/* Blow cover.
 */

static int ps_chestkeeper_blow_cover(struct ps_sprite *spr,struct ps_game *game) {
  SPR->incognito=0;
  SPR->blowcover_transition=PS_CHESTKEEPER_BLOWCOVER_TIME;
  PS_SFX_CHESTKEEPER_BLOW_COVER
  return 0;
}

/* Update while incognito.
 */

static int ps_chestkeeper_update_incognito(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_chestkeeper_should_blow_cover(spr,game)) {
    if (ps_chestkeeper_blow_cover(spr,game)<0) return -1;
  }

  SPR->dazzlet+=SPR->dazzletd;
  if (SPR->dazzlet>=PS_TREASURECHEST_DAZZLE_LIMIT) {
    SPR->dazzlet=PS_TREASURECHEST_DAZZLE_LIMIT;
    if (SPR->dazzletd>0) SPR->dazzletd=-PS_TREASURECHEST_DAZZLE_RATE;
  } else if (SPR->dazzlet<=-PS_TREASURECHEST_DAZZLE_LIMIT) {
    SPR->dazzlet=-PS_TREASURECHEST_DAZZLE_LIMIT;
    if (SPR->dazzletd<0) SPR->dazzletd=PS_TREASURECHEST_DAZZLE_RATE;
  }

  SPR->dazzlesizep+=SPR->dazzlesized;
  if (SPR->dazzlesizep>=PS_TREASURECHEST_DAZZLE_SIZE_PERIOD) {
    SPR->dazzlesizep=PS_TREASURECHEST_DAZZLE_SIZE_PERIOD;
    if (SPR->dazzlesized>0) SPR->dazzlesized=-SPR->dazzlesized;
  } else if (SPR->dazzlesizep<0) {
    SPR->dazzlesizep=0;
    if (SPR->dazzlesized<0) SPR->dazzlesized=-SPR->dazzlesized;
  }
  
  return 0;
}

/* Push all physics sprites away from me.
 */

static int ps_chestkeeper_disperse_all(struct ps_sprite *spr,struct ps_game *game,double speed) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_PHYSICS;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=grp->sprv[i];
    if (pumpkin==spr) continue; // don't blow self; nobody is that flexible.
    double dx=pumpkin->x-spr->x;
    double dy=pumpkin->y-spr->y;
    double distance=sqrt(dx*dx+dy*dy);
    double normx=dx/distance;
    double normy=dy/distance;
    pumpkin->x+=speed*normx;
    pumpkin->y+=speed*normy;
  }
  return 0;
}

/* Look for victims of one arm and kill them.
 */

static int ps_chestkeeper_arm_kill_pigeons(struct ps_sprite *spr,struct ps_chestkeeper_arm *arm,struct ps_game *game) {

  double bladex=spr->x+arm->shoulderx-sin(arm->t)*arm->length;
  double bladey=spr->y+arm->shouldery+cos(arm->t)*arm->length;

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim==spr) continue;
    double radiussum=PS_CHESTKEEPER_BLADE_RADIUS+victim->radius;
    double dx=victim->x-bladex;
    double adx=(dx<0.0)?-dx:dx;
    if (adx>=radiussum) continue;
    double dy=victim->y-bladey;
    double ady=(dy<0.0)?-dy:dy;
    if (ady>=radiussum) continue;
    double manhattan=adx+ady;
    if (manhattan>radiussum*1.4142) continue;
    if (manhattan>radiussum*0.07071) {
      double distance=sqrt(adx*adx+ady*ady);
      if (distance>=radiussum) continue;
    }
    if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
  }

  return 0;
}

/* Update arm.
 */

static int ps_chestkeeper_arm_update(struct ps_sprite *spr,struct ps_chestkeeper_arm *arm,struct ps_game *game) {
  arm->t+=arm->dt;
  if (arm->t>=M_PI*2.0) arm->t-=M_PI*2.0;
  else if (arm->t<0.0) arm->t+=M_PI*2.0;
  arm->length+=arm->dlength;
  if (arm->length<PS_CHESTKEEPER_ARM_LENGTH_LO) {
    if (arm->dlength<0.0) arm->dlength=-arm->dlength;
  } else if (arm->length>PS_CHESTKEEPER_ARM_LENGTH_HI) {
    if (arm->dlength>0.0) arm->dlength=-arm->dlength;
  }
  if (ps_chestkeeper_arm_kill_pigeons(spr,arm,game)<0) return -1;
  return 0;
}

/* Select new direction and time for walking.
 */

static int ps_chestkeeper_adjust_walk(struct ps_sprite *spr,struct ps_game *game) {

  double speed=PS_CHESTKEEPER_WALK_SPEED_LO+(rand()%1000)*((PS_CHESTKEEPER_WALK_SPEED_HI-PS_CHESTKEEPER_WALK_SPEED_LO)/1000.0);

  double t=(rand()%6282)/1000.0;
  double dx=-sin(t)*speed;
  double dy=cos(t)*speed;

  if ((spr->x<PS_SCREENW/4)&&(dx<0.0)) dx=-dx;
  else if ((spr->x>(PS_SCREENW*3)/4)&&(dx>0.0)) dx=-dx;
  if ((spr->y<PS_SCREENH/4)&&(dy<0.0)) dy=-dy;
  else if ((spr->y>(PS_SCREENH*3)/4)&&(dy>0.0)) dy=-dy;

  SPR->walkdx=dx;
  SPR->walkdy=dy;

  SPR->walktime=PS_CHESTKEEPER_WALK_TIME_LO+rand()%(PS_CHESTKEEPER_WALK_TIME_HI-PS_CHESTKEEPER_WALK_TIME_LO);

  return 0;
}

/* Walk.
 */

static int ps_chestkeeper_walk(struct ps_sprite *spr,struct ps_game *game) {
  if (--(SPR->walktime)<=0) {
    if (ps_chestkeeper_adjust_walk(spr,game)<0) return -1;
  }
  // Walk on one axis at a time, alternating rapidly.
  if (SPR->walktime&1) {
    spr->x+=SPR->walkdx;
  } else {
    spr->y+=SPR->walkdy;
  }
  return 0;
}

/* Update.
 */

static int _ps_chestkeeper_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->invincible>0) SPR->invincible--;

  if (SPR->incognito) {
    if (ps_chestkeeper_update_incognito(spr,game)<0) return -1;

  } else if (SPR->blowcover_transition>0) {
    SPR->blowcover_transition--;
    double speed = (SPR->blowcover_transition*PS_CHESTKEEPER_BLOWCOVER_DISPERSAL_SPEED)/PS_CHESTKEEPER_BLOWCOVER_TIME;
    if (ps_chestkeeper_disperse_all(spr,game,speed)<0) return -1;

  } else {
    if (++(SPR->jaw_animp)>=PS_CHESTKEEPER_JAW_ANIMC) {
      SPR->jaw_animp=0;
    }
    if (++(SPR->animtime)>=PS_CHESTKEEPER_ANIM_DELAY) {
      SPR->animtime=0;
      if (++(SPR->animframe)>=PS_CHESTKEEPER_ANIM_COUNT) {
        SPR->animframe=0;
      }
    }
    if (ps_chestkeeper_arm_update(spr,&SPR->arml,game)<0) return -1;
    if (ps_chestkeeper_arm_update(spr,&SPR->armr,game)<0) return -1;
    if (ps_chestkeeper_walk(spr,game)<0) return -1;
  }
  return 0;
}

/* Draw, incognito.
 */

static int ps_chestkeeper_draw_incognito(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;
  struct akgl_vtx_maxtile *vtx_dazzle=vtxv;
  struct akgl_vtx_maxtile *vtx_chest=vtxv+1;
  
  vtx_chest->x=spr->x;
  vtx_chest->y=spr->y;
  vtx_chest->tileid=0x52;
  vtx_chest->size=PS_TILESIZE;
  vtx_chest->pr=vtx_chest->pg=vtx_chest->pb=0x80;
  vtx_chest->a=0xff;
  vtx_chest->ta=0;
  vtx_chest->t=0;
  vtx_chest->xform=AKGL_XFORM_NONE;
  
  memcpy(vtx_dazzle,vtx_chest,sizeof(struct akgl_vtx_maxtile));
  vtx_dazzle->tileid=0x55;
  vtx_dazzle->t=SPR->dazzlet;
  vtx_dazzle->size=PS_TREASURECHEST_DAZZLE_SIZE_MIN+(SPR->dazzlesizep*(PS_TREASURECHEST_DAZZLE_SIZE_MAX-PS_TREASURECHEST_DAZZLE_SIZE_MIN))/PS_TREASURECHEST_DAZZLE_SIZE_PERIOD;
  
  return 2;
}

/* Draw one arm.
 */

static void ps_chestkeeper_draw_arm(struct akgl_vtx_maxtile *vtxv,const struct ps_sprite *spr,const struct ps_chestkeeper_arm *arm) {
  struct akgl_vtx_maxtile *linkv=vtxv;
  struct akgl_vtx_maxtile *blade=vtxv+PS_CHESTKEEPER_ARM_LINKC;
  struct akgl_vtx_maxtile *box=blade+1;

  double normx=-sin(arm->t);
  double normy=cos(arm->t);
  int t=(arm->t*128.0)/M_PI+128;

  double bladex=spr->x+arm->shoulderx+normx*arm->length;
  double bladey=spr->y+arm->shouldery+normy*arm->length;
  blade->x=bladex;
  blade->y=bladey;
  blade->size=PS_TILESIZE;
  blade->tileid=spr->tileid+2;
  blade->a=0xff;
  blade->ta=0;
  blade->pr=blade->pg=blade->pb=0x80;
  blade->t=t;
  blade->xform=AKGL_XFORM_NONE;
  
  const double blade_box_distance=13.0;
  box->x=bladex+blade_box_distance*-normx;
  box->y=bladey+blade_box_distance*-normy;
  box->size=PS_TILESIZE;
  box->tileid=spr->tileid+1;
  box->a=0xff;
  box->ta=0;
  box->pr=blade->pg=blade->pb=0x80;
  box->t=t;
  box->xform=AKGL_XFORM_NONE;

  int i;
  int xbase=spr->x+arm->shoulderx;
  int ybase=spr->y+arm->shouldery;
  for (i=0;i<PS_CHESTKEEPER_ARM_LINKC;i++) {
    linkv[i].x=xbase+((box->x-xbase)*i)/PS_CHESTKEEPER_ARM_LINKC;
    linkv[i].y=ybase+((box->y-ybase)*i)/PS_CHESTKEEPER_ARM_LINKC;
    linkv[i].size=PS_TILESIZE;
    linkv[i].tileid=0x22;
    linkv[i].pr=0x80;
    linkv[i].pg=0xa0;
    linkv[i].pb=0xc0;
    linkv[i].a=0xff;
    linkv[i].ta=0;
    linkv[i].t=0; // no rotation; links are circular
    linkv[i].xform=AKGL_XFORM_NONE;
  }
}

/* Draw.
 */

static int _ps_chestkeeper_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  /* Incognito is a different and much simpler regime. */
  if (SPR->incognito) {
    return ps_chestkeeper_draw_incognito(vtxv,vtxa,spr);
  }

  int vtxc=3; // 1 body + 2 head
  vtxc+=2+PS_CHESTKEEPER_ARM_LINKC; // left arm
  vtxc+=2+PS_CHESTKEEPER_ARM_LINKC; // right arm
  if (vtxc>vtxa) return vtxc;
  struct akgl_vtx_maxtile *vtx_body=vtxv+0;
  struct akgl_vtx_maxtile *vtx_jaw=vtxv+1;
  struct akgl_vtx_maxtile *vtx_cranium=vtxv+2;
  struct akgl_vtx_maxtile *vtxv_arml=vtxv+3;
  struct akgl_vtx_maxtile *vtxv_armr=vtxv+5+PS_CHESTKEEPER_ARM_LINKC;

  vtx_body->x=spr->x;
  vtx_body->y=spr->y;
  vtx_body->tileid=spr->tileid;
  vtx_body->size=PS_TILESIZE;
  vtx_body->pr=vtx_body->pg=vtx_body->pb=0x80;
  vtx_body->a=0xff;
  vtx_body->ta=0x00;
  vtx_body->t=0;
  vtx_body->xform=AKGL_XFORM_NONE;

  /* Tint. */
  if (SPR->blowcover_transition>0) {
    if (SPR->blowcover_transition&0x08) {
      vtx_body->tr=0xff;
      vtx_body->tg=0x00;
      vtx_body->tb=0x00;
      vtx_body->ta=0x80;
    }
  } else if (SPR->invincible>0) {
    vtx_body->tr=0xff;
    vtx_body->tg=0x00;
    vtx_body->tb=0x00;
    vtx_body->ta=30+(SPR->invincible*225)/PS_CHESTKEEPER_INVINCIBLE_TIME;
  }

  /* Place jaw and cranium. */
  memcpy(vtx_jaw,vtx_body,sizeof(struct akgl_vtx_maxtile));
  vtx_jaw->y-=14; // (13..15) acceptable
  vtx_jaw->tileid-=15;
  memcpy(vtx_cranium,vtx_body,sizeof(struct akgl_vtx_maxtile));
  vtx_cranium->y=vtx_jaw->y-5;
  vtx_cranium->tileid-=16;

  /* Animate cranium if we are done blowing cover. */
  if (!SPR->blowcover_transition) {
    int halfc=PS_CHESTKEEPER_JAW_ANIMC>>1;
    int halfp;
    if (SPR->jaw_animp>=halfc) {
      halfp=PS_CHESTKEEPER_JAW_ANIMC-SPR->jaw_animp;
    } else {
      halfp=SPR->jaw_animp;
    }
    const int maxdisplacement=8;
    vtx_cranium->y=vtx_jaw->y-5-(halfp*maxdisplacement)/halfc;
  }

  /* Animate walking. */
  if (!SPR->blowcover_transition) {
    switch (SPR->animframe) {
      case 0: break;
      case 1: vtx_body->tileid-=14; break;
      case 2: break;
      case 3: vtx_body->tileid-=14; vtx_body->xform=AKGL_XFORM_FLOP; break;
    }
  }

  ps_chestkeeper_draw_arm(vtxv_arml,spr,&SPR->arml);
  ps_chestkeeper_draw_arm(vtxv_armr,spr,&SPR->armr);
  
  return vtxc;
}

/* Hurt.
 */

static int _ps_chestkeeper_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->incognito) return 0;
  if (SPR->blowcover_transition>0) return 0;
  if (SPR->invincible>0) return 0;

  if (SPR->hp<=1) {
    SPR->hp=0;
    SPR->invincible=INT_MAX;  
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    if (ps_game_decorate_monster_death(game,spr->x,spr->y)<0) return -1;
    if (ps_game_report_kill(game,assailant,spr)<0) return -1;
    if (ps_game_recreate_treasure_chest(game,spr->x,spr->y)<0) return -1;
    return 0;
  }

  PS_SFX_MONSTER_HURT
  SPR->hp--;
  SPR->invincible=PS_CHESTKEEPER_INVINCIBLE_TIME;

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_chestkeeper={
  .name="chestkeeper",
  .objlen=sizeof(struct ps_sprite_chestkeeper),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_chestkeeper_init,
  .del=_ps_chestkeeper_del,
  .configure=_ps_chestkeeper_configure,
  .get_configure_argument_name=_ps_chestkeeper_get_configure_argument_name,
  .update=_ps_chestkeeper_update,
  .draw=_ps_chestkeeper_draw,
  
  .hurt=_ps_chestkeeper_hurt,

};
