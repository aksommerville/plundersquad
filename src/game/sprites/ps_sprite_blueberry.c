#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_BLUEBERRY_NODE_LIMIT 6
#define PS_BLUEBERRY_NODE_OFFSET (PS_TILESIZE-5)
#define PS_BLUEBERRY_WALK_SPEED 1.0
#define PS_BLUEBERRY_SINEC 60
#define PS_BLUEBERRY_BOUNCEM_DEFAULT 3.0
#define PS_BLUEBERRY_INVINCIBLE_TIME 60

/* Private sprite object.
 */

static double ps_blueberry_sinev[PS_BLUEBERRY_SINEC];
static double ps_blueberry_walk_attenuationv[PS_BLUEBERRY_SINEC];
static int ps_blueberry_sinev_valid=0;

struct ps_sprite_blueberry {
  struct ps_sprite hdr;
  int nodec;
  double dx,dy;
  int bouncep; // Position in wave function.
  double bouncem; // Magnitude of wave function, ie maximum bounce.
  int invincible;
};

#define SPR ((struct ps_sprite_blueberry*)spr)

/* Initialize bounce coefficients.
 */

static void ps_blueberry_initialize_sinev_if_needed() {
  if (ps_blueberry_sinev_valid) return;
  const double phase_shift=(M_PI*3.0)/2.0; // Start at the lowest point of the function.
  int i=0; for (;i<PS_BLUEBERRY_SINEC;i++) {
    double t=(i*M_PI*2.0)/PS_BLUEBERRY_SINEC+phase_shift;
    ps_blueberry_sinev[i]=sin(t);
  }
  for (i=0;i<PS_BLUEBERRY_SINEC;i++) {
    double t=(i*M_PI)/PS_BLUEBERRY_SINEC;
    ps_blueberry_walk_attenuationv[i]=sin(t);
  }
  ps_blueberry_sinev_valid=1;
}

/* Delete.
 */

static void _ps_blueberry_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_blueberry_init(struct ps_sprite *spr) {

  ps_blueberry_initialize_sinev_if_needed();

  SPR->bouncem=PS_BLUEBERRY_BOUNCEM_DEFAULT;

  return 0;
}

/* Configure.
 */

static int _ps_blueberry_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc) {
  return 0;
}

/* Rebild nodes.
 */

static int ps_blueberry_rebuild(struct ps_sprite *spr,int nodec) {
  if ((nodec<1)||(nodec>PS_BLUEBERRY_NODE_LIMIT)) return -1;

  SPR->nodec=nodec;
  
  return 0;
}

/* Begin step.
 */

static int ps_blueberry_begin_step(struct ps_sprite *spr,struct ps_game *game) {

  double t=(rand()%628)/100.0;
  SPR->dx=PS_BLUEBERRY_WALK_SPEED*cos(t);
  SPR->dy=PS_BLUEBERRY_WALK_SPEED*sin(t);

  /* If we are currently within 25% of an edge, point towards the bulk of the screen. */
  if (spr->x<PS_SCREENW>>2) {
    if (SPR->dx<0.0) SPR->dx=-SPR->dx;
  } else if (spr->x>(PS_SCREENW*3)>>2) {
    if (SPR->dx>0.0) SPR->dx=-SPR->dx;
  }
  if (spr->y<PS_SCREENH>>2) {
    if (SPR->dy<0.0) SPR->dy=-SPR->dy;
  } else if (spr->y>(PS_SCREENH*3)>>2) {
    if (SPR->dy>0.0) SPR->dy=-SPR->dy;
  }
  
  return 0;
}

/* End step.
 */

static int ps_blueberry_end_step(struct ps_sprite *spr,struct ps_game *game) {
  SPR->bouncep=0;
  return 0;
}

/* Update step.
 */

static int ps_blueberry_update_step(struct ps_sprite *spr,struct ps_game *game) {
  spr->x+=SPR->dx*ps_blueberry_walk_attenuationv[SPR->bouncep];
  spr->y+=SPR->dy*ps_blueberry_walk_attenuationv[SPR->bouncep];
  return 0;
}

/* Update.
 */

static int _ps_blueberry_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->invincible>0) {
    SPR->invincible--;
  }

  if (!SPR->nodec) {
    if (ps_blueberry_rebuild(spr,PS_BLUEBERRY_NODE_LIMIT)<0) return -1;
  }

  if (!SPR->bouncep) {
    if (ps_blueberry_begin_step(spr,game)<0) return -1;
  }

  if (++(SPR->bouncep)>=PS_BLUEBERRY_SINEC) {
    if (ps_blueberry_end_step(spr,game)<0) return -1;
  } else {
    if (ps_blueberry_update_step(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_blueberry_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=SPR->nodec;
  if (vtxc>vtxa) return vtxc;

  int compression; // 0..2, add to tileid to reflect compression from neighbors.
  switch ((SPR->bouncep*6)/PS_BLUEBERRY_SINEC) {
    case 0: compression=2; break;
    case 1: compression=1; break;
    case 2: compression=0; break;
    case 3: compression=0; break;
    case 4: compression=1; break;
    case 5: compression=2; break;
  }

  /* Set up the bottom vertex. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid+3+compression;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0x00;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  /* Set up invincibility flash. */
  if (SPR->invincible) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*0xff)/PS_BLUEBERRY_INVINCIBLE_TIME;
  }

  /* Copy to other vertices. Update (y) and (tileid). */
  double y=spr->y;
  double offset=PS_BLUEBERRY_NODE_OFFSET+ps_blueberry_sinev[SPR->bouncep]*SPR->bouncem;
  int i=1; for (;i<SPR->nodec;i++) {
    memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
    y-=offset;
    vtxv[i].tileid-=3;
    vtxv[i].x=spr->x;
    vtxv[i].y=y;
  }
  
  return vtxc;
}

/* Hurt.
 */

static int _ps_blueberry_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->invincible) return 0;

  if ((SPR->nodec<=1)||!spr->grpc) {
    PS_SFX_MONSTER_DEAD
    SPR->invincible=INT_MAX;
    if (ps_game_create_fireworks(game,spr->x,spr->y)<0) return -1;
    if (ps_game_create_prize(game,spr->x,spr->y)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    if (ps_game_check_deathgate(game)<0) return -1;
    return 0;
  }

  PS_SFX_MONSTER_HURT

  struct ps_sprite *other=ps_sprite_new(&ps_sprtype_blueberry);
  if (!other) return -1;
  struct ps_sprite_blueberry *OTHER=(struct ps_sprite_blueberry*)other;

  if (ps_sprite_join_all(other,spr)<0) {
    ps_sprite_kill(other);
    ps_sprite_del(other);
    return -1;
  }
  ps_sprite_del(other);

  other->x=spr->x;
  other->y=spr->y;
  other->radius=spr->radius;
  other->shape=spr->shape;
  other->collide_hole=spr->collide_hole;
  other->collide_sprites=spr->collide_sprites;
  other->tsid=spr->tsid;
  other->layer=spr->layer;
  other->tileid=spr->tileid;

  int nodec0=SPR->nodec;
  SPR->nodec>>=1;
  OTHER->nodec=nodec0-SPR->nodec;

  SPR->invincible=PS_BLUEBERRY_INVINCIBLE_TIME;
  OTHER->invincible=PS_BLUEBERRY_INVINCIBLE_TIME;

  OTHER->bouncep=0;

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_blueberry={
  .name="blueberry",
  .objlen=sizeof(struct ps_sprite_blueberry),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_blueberry_init,
  .del=_ps_blueberry_del,
  .configure=_ps_blueberry_configure,
  .update=_ps_blueberry_update,
  .draw=_ps_blueberry_draw,
  
  .hurt=_ps_blueberry_hurt,

};
