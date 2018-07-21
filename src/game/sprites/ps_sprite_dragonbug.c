#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/sprites/ps_sprite_hookshot.h"
#include "game/ps_sound_effects.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_DRAGONBUG_PHASE_IDLE     0 /* Sitting still, contemplating movement. */
#define PS_DRAGONBUG_PHASE_WALK     1 /* Moving about. */
#define PS_DRAGONBUG_PHASE_AIM      2 /* Sitting still, spinning head to select target. */
#define PS_DRAGONBUG_PHASE_INHALE   3 /* Make a face just before spitting fire. */
#define PS_DRAGONBUG_PHASE_EXHALE   4 /* Just after spitting fire. */

#define PS_DRAGONBUG_WALK_ANIM_TIME   10
#define PS_DRAGONBUG_WALK_ANIM_FRAMEC  4

#define PS_DRAGONBUG_WALK_SPEED 2.0
#define PS_DRAGONBUG_WALK_CHECK_DISTANCE (PS_TILESIZE*5)
#define PS_DRAGONBUG_WALK_EDGE_SIZE (PS_TILESIZE*2)

#define PS_DRAGONBUG_HEAD_OFFSET 4
#define PS_DRAGONBUG_EDGE_MARGIN (PS_TILESIZE>>1)

#define PS_DRAGONBUG_FAST_SPIN_INCREMENT 10
#define PS_DRAGONBUG_AIM_SPEED 2

#define PS_DRAGONBUG_IDLE_TIME_MIN   20
#define PS_DRAGONBUG_IDLE_TIME_MAX  100
#define PS_DRAGONBUG_WALK_TIME_MIN   20
#define PS_DRAGONBUG_WALK_TIME_MAX   60
#define PS_DRAGONBUG_AIM_TIME_MIN    60
#define PS_DRAGONBUG_AIM_TIME_MAX   150
#define PS_DRAGONBUG_INHALE_TIME     30
#define PS_DRAGONBUG_EXHALE_TIME     30

#define PS_DRAGONBUG_FIREBALL_SPRDEF_ID 11

/* Private sprite object.
 */

struct ps_sprite_dragonbug {
  struct ps_sprite hdr;
  int phase;
  int phasetime;
  int animtime;
  int animframe;
  uint8_t headt;
  double dx,dy;
};

#define SPR ((struct ps_sprite_dragonbug*)spr)

/* Delete.
 */

static void _ps_dragonbug_del(struct ps_sprite *spr) {
}

/* Set SPR->phasetime based on SPR->phase.
 */

static void ps_dragonbug_reset_phase_time(struct ps_sprite *spr) {
 _again_:
  #define RANGE(tag) \
    SPR->phasetime=PS_DRAGONBUG_##tag##_TIME_MIN+rand()%(PS_DRAGONBUG_##tag##_TIME_MAX-PS_DRAGONBUG_##tag##_TIME_MIN+1);
  switch (SPR->phase) {
    case PS_DRAGONBUG_PHASE_IDLE: RANGE(IDLE) break;
    case PS_DRAGONBUG_PHASE_WALK: RANGE(WALK) break;
    case PS_DRAGONBUG_PHASE_AIM: RANGE(AIM) break;
    case PS_DRAGONBUG_PHASE_INHALE: SPR->phasetime=PS_DRAGONBUG_INHALE_TIME; break;
    case PS_DRAGONBUG_PHASE_EXHALE: SPR->phasetime=PS_DRAGONBUG_EXHALE_TIME; break;
    default: SPR->phase=PS_DRAGONBUG_PHASE_IDLE; goto _again_;
  }
  #undef RANGE
}

/* Initialize.
 */

static int _ps_dragonbug_init(struct ps_sprite *spr) {

  SPR->phase=PS_DRAGONBUG_PHASE_IDLE;
  ps_dragonbug_reset_phase_time(spr);

  return 0;
}

/* Configure.
 */

static int _ps_dragonbug_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Restore default head angle incrementally.
 */

static void ps_dragonbug_advance_head_to_default(struct ps_sprite *spr) {
  if (!SPR->headt) return;
  if (SPR->headt>=0x80) SPR->headt++;
  else SPR->headt--;
}

/* Context for aiming.
 */

struct ps_dragonbug_aim {
  double headt; // Head angle in radians.
  struct ps_fvector headnorm; // Normal vector of head position.
  struct ps_sprite *candidatev[PS_PLAYER_LIMIT]; // Potential targets.
  double distancev[PS_PLAYER_LIMIT];
  int candidatec;
};

/* Sort candidates for targetting.
 * Positive projection is always preferable to negative.
 * Beyond that, smaller is better.
 */

static int ps_dragonbug_compare_candidates(const struct ps_dragonbug_aim *ctx,int a,int b) {
  double an=ctx->distancev[a]; if (an<0.0) an=-an;
  double bn=ctx->distancev[b]; if (bn<0.0) bn=-bn;
  if (an<bn) return -1;
  return 1;
}

static void ps_dragonbug_sort_aim_candidates(struct ps_dragonbug_aim *ctx) {
  int lo=0,hi=ctx->candidatec-1,d=1;
  while (lo<hi) {
    int first,last,done=1,i;
    if (d==1) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      int cmp=ps_dragonbug_compare_candidates(ctx,i,i+d);
      if (cmp==d) {
        done=0;
        struct ps_sprite *candidate=ctx->candidatev[i];
        double distance=ctx->distancev[i];
        ctx->candidatev[i]=ctx->candidatev[i+d];
        ctx->distancev[i]=ctx->distancev[i+d];
        ctx->candidatev[i+d]=candidate;
        ctx->distancev[i+d]=distance;
      }
    }
    if (done) return;
    if (d==1) { d=-1; hi--; }
    else { d=1; lo++; }
  }
}

/* Compare angles.
 */

static int ps_dragonbug_compare_angles(double a,double b) {
  double d=b-a;
  uint8_t dfinal=(d*128.0)/M_PI;
  if (dfinal<PS_DRAGONBUG_AIM_SPEED) return 0;
  if (dfinal>255-PS_DRAGONBUG_AIM_SPEED) return 0;
  if (dfinal<128) return 1;
  return -1;
}

static double ps_dragonbug_angle_diff(double a,double b) {
  double d=b-a;
  while (d<-M_PI) d+=M_PI*2.0;
  while (d>M_PI) d-=M_PI*2.0;
  return d;
}

/* Identify a target sprite and take aim at it.
 */

static int ps_dragonbug_update_aim(struct ps_sprite *spr,struct ps_game *game) {

  /* Prepare context. */
  struct ps_dragonbug_aim ctx={0};
  ctx.headt=(SPR->headt*M_PI)/128.0+M_PI;
  while (ctx.headt>M_PI) ctx.headt-=M_PI*2.0;
  ctx.headnorm=ps_fvector_from_polcoord(ps_polcoord(ctx.headt,1.0));

  /* Identify candidates. */
  int i=game->grpv[PS_SPRGRP_HERO].sprc;
  for (;i-->0;) {
    struct ps_sprite *candidate=game->grpv[PS_SPRGRP_HERO].sprv[i];
    // Target dead heroes just like living ones; that gives ghosts a purpose.
    ctx.candidatev[ctx.candidatec++]=candidate;
    if (ctx.candidatec>=PS_PLAYER_LIMIT) break;
  }

  /* If no candidates, spin my head wildly. */
  if (!ctx.candidatec) {
    SPR->headt+=PS_DRAGONBUG_FAST_SPIN_INCREMENT;
    return 0;
  }

  /* If we only have one candidate, no need for expensive comparison. */
  if (ctx.candidatec>1) {

    /* Calculate manhattan distance to each candidate. */
    for (i=0;i<ctx.candidatec;i++) {
      double dx=ctx.candidatev[i]->x-spr->x;
      double dy=ctx.candidatev[i]->y-spr->y;
      ctx.distancev[i]=((dx<0.0)?-dx:dx)+((dy<0.0)?-dy:dy);
    }

    /* Sort candidates. */
    ps_dragonbug_sort_aim_candidates(&ctx);
  }

  /* Aim at the most preferred. */
  struct ps_sprite *target=ctx.candidatev[0];
  double targett=atan2(target->y-spr->y,target->x-spr->x)+M_PI/2.0;
  targett=ps_dragonbug_angle_diff(ctx.headt,targett);
  if (targett<0.0) SPR->headt-=PS_DRAGONBUG_AIM_SPEED;
  else if (targett>0.0) SPR->headt+=PS_DRAGONBUG_AIM_SPEED;
  else SPR->headt=((targett-M_PI)*128.0)/M_PI;

  return 0;
}

/* Advance position in WALK phase.
 */

static int ps_dragonbug_update_walk(struct ps_sprite *spr,struct ps_game *game) {

  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  
  if ((spr->x<PS_DRAGONBUG_EDGE_MARGIN)&&(SPR->dx<0.0)) {
    SPR->dx=-SPR->dx;
  } else if ((spr->x>PS_SCREENW-PS_DRAGONBUG_EDGE_MARGIN)&&(SPR->dx>0.0)) {
    SPR->dx=-SPR->dx;
  }
  if ((spr->y<PS_DRAGONBUG_EDGE_MARGIN)&&(SPR->dy<0.0)) {
    SPR->dy=-SPR->dy;
  } else if ((spr->y>PS_SCREENH-PS_DRAGONBUG_EDGE_MARGIN)&&(SPR->dy>0.0)) {
    SPR->dy=-SPR->dy;
  }
  
  return 0;
}

/* Set the phase to WALK and select a direction.
 */

static int ps_dragonbug_begin_walk(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_DRAGONBUG_PHASE_WALK;
  
  if (ps_game_select_random_travel_vector(&SPR->dx,&SPR->dy,game,spr->x,spr->y,PS_DRAGONBUG_WALK_SPEED,spr->impassable)<0) return -1;
  
  return 0;
}

/* Set the phase to EXHALE and create a fireball.
 */

static int ps_dragonbug_begin_exhale(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_DRAGONBUG_PHASE_EXHALE;

  PS_SFX_DRAGONBUG_EXHALE
  
  double t=(SPR->headt*M_PI)/128.0;
  double dstx=spr->x-100.0*sin(t);
  double dsty=spr->y-PS_DRAGONBUG_HEAD_OFFSET+100.0*cos(t);

  struct ps_sprite *fireball=ps_sprite_missile_new(PS_DRAGONBUG_FIREBALL_SPRDEF_ID,spr,dstx,dsty,game);
  if (!fireball) return -1;
  fireball->y-=PS_DRAGONBUG_HEAD_OFFSET;

  return 0;
}

/* Complete the current phase and set a new one.
 */

static int ps_dragonbug_complete_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_DRAGONBUG_PHASE_IDLE: return ps_dragonbug_begin_walk(spr,game);
    case PS_DRAGONBUG_PHASE_WALK: SPR->phase=PS_DRAGONBUG_PHASE_AIM; break;
    case PS_DRAGONBUG_PHASE_AIM: SPR->phase=PS_DRAGONBUG_PHASE_INHALE; break;
    case PS_DRAGONBUG_PHASE_INHALE: return ps_dragonbug_begin_exhale(spr,game);
    case PS_DRAGONBUG_PHASE_EXHALE: SPR->phase=PS_DRAGONBUG_PHASE_IDLE; break;
  }
  return 0;
}

/* Update.
 */

static int _ps_dragonbug_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Switch phase based on timer. */
  if (--(SPR->phasetime)<=0) {
    if (ps_dragonbug_complete_phase(spr,game)<0) return -1;
    ps_dragonbug_reset_phase_time(spr);
  }

  /* Cycle the walk animation counter whether we're using it or not. */
  if (++(SPR->animtime)>=PS_DRAGONBUG_WALK_ANIM_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_DRAGONBUG_WALK_ANIM_FRAMEC) {
      SPR->animframe=0;
    }
  }

  /* In IDLE or WALK phase, return head to default position. */
  if ((SPR->phase==PS_DRAGONBUG_PHASE_IDLE)||(SPR->phase==PS_DRAGONBUG_PHASE_WALK)) {
    ps_dragonbug_advance_head_to_default(spr);
  } else if (SPR->phase==PS_DRAGONBUG_PHASE_AIM) {
    if (ps_dragonbug_update_aim(spr,game)<0) return -1;
  }

  /* In WALK phase, take a step. */
  if (SPR->phase==PS_DRAGONBUG_PHASE_WALK) {
    if (ps_dragonbug_update_walk(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_dragonbug_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;

  struct akgl_vtx_maxtile *vtx_body=vtxv+0;
  struct akgl_vtx_maxtile *vtx_head=vtxv+1;

  /* Constants in both vertices. */
  vtx_body->size=vtx_head->size=PS_TILESIZE;
  vtx_body->ta=vtx_head->ta=0;
  vtx_body->pr=vtx_head->pr=0x80;
  vtx_body->pg=vtx_head->pg=0x80;
  vtx_body->pb=vtx_head->pb=0x80;
  vtx_body->a=vtx_head->a=0xff;
  vtx_body->xform=vtx_head->xform=AKGL_XFORM_NONE;

  /* Positions. */
  vtx_body->x=spr->x;
  vtx_body->y=spr->y;
  vtx_head->x=spr->x;
  vtx_head->y=spr->y-PS_DRAGONBUG_HEAD_OFFSET;

  /* Rotation. */
  vtx_body->t=0;
  vtx_head->t=SPR->headt;

  /* Tiles. */
  if (SPR->phase==PS_DRAGONBUG_PHASE_WALK) {
    switch (SPR->animframe) {
      case 0: vtx_body->tileid=spr->tileid+0; break;
      case 1: vtx_body->tileid=spr->tileid+1; break;
      case 2: vtx_body->tileid=spr->tileid+0; break;
      case 3: vtx_body->tileid=spr->tileid+2; break;
    }
  } else {
    vtx_body->tileid=spr->tileid;
  }
  if (SPR->phase==PS_DRAGONBUG_PHASE_INHALE) {
    vtx_head->tileid=spr->tileid+0x11;
  } else if (SPR->phase==PS_DRAGONBUG_PHASE_EXHALE) {
    vtx_head->tileid=spr->tileid+0x12;
  } else {
    vtx_head->tileid=spr->tileid+0x10;
  }
  
  return 2;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_dragonbug={
  .name="dragonbug",
  .objlen=sizeof(struct ps_sprite_dragonbug),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_dragonbug_init,
  .del=_ps_dragonbug_del,
  .configure=_ps_dragonbug_configure,
  .update=_ps_dragonbug_update,
  .draw=_ps_dragonbug_draw,
  
};
