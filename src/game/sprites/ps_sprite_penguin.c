#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_PENGUIN_PHASE_THREATEN      1 /* Just a shadow, growing. */
#define PS_PENGUIN_PHASE_FALL          2 /* Shadow at full size, penguin falling from above. */
#define PS_PENGUIN_PHASE_DAZED         3 /* Sitting on the ground, seeing spots. */
#define PS_PENGUIN_PHASE_IDLE          4 /* Standing still. */
#define PS_PENGUIN_PHASE_WALK          5 /* Walking nowhere in a straight line. */

#define PS_PENGUIN_THREATEN_TIME   60
#define PS_PENGUIN_DAZED_TIME     120
#define PS_PENGUIN_IDLE_TIME_MIN   30
#define PS_PENGUIN_IDLE_TIME_MAX  120
#define PS_PENGUIN_WALK_TIME_MIN   60
#define PS_PENGUIN_WALK_TIME_MAX  140

#define PS_PENGUIN_FALL_SPEED 4.0
#define PS_PENGUIN_DAZED_STAR_COUNT 6
#define PS_PENGUIN_SHADOW_SIZE_MIN 1
#define PS_PENGUIN_SHADOW_SIZE_MAX PS_TILESIZE
#define PS_PENGUIN_SHADOW_OFFSET 6
#define PS_PENGUIN_STARS_OFFSET ((PS_TILESIZE*4)/8)
#define PS_PENGUIN_STAR_X_RADIUS  8
#define PS_PENGUIN_STAR_Y_RADIUS  4
#define PS_PENGUIN_STAR_SPEED 0.08 /* radians per frame */
#define PS_PENGUIN_WALK_FRAME_TIME 8
#define PS_PENGUIN_WALK_FRAME_COUNT 4
#define PS_PENGUIN_WALK_SPEED 1.0

#define PS_PENGUIN_NORMAL_GROUPS ( \
  (1<<PS_SPRGRP_KEEPALIVE)| \
  (1<<PS_SPRGRP_VISIBLE)| \
  (1<<PS_SPRGRP_UPDATE)| \
  (1<<PS_SPRGRP_SOLID)| \
  (1<<PS_SPRGRP_PHYSICS)| \
  (1<<PS_SPRGRP_FRAGILE)| \
  (1<<PS_SPRGRP_HEROHAZARD)| \
  (1<<PS_SPRGRP_LATCH)| \
0)

/* Private sprite object.
 */

struct ps_sprite_penguin {
  struct ps_sprite hdr;
  int phase;
  int phasetime;
  int animtime;
  int animframe;
  double walkdx,walkdy;
};

#define SPR ((struct ps_sprite_penguin*)spr)

/* Delete.
 */

static void _ps_penguin_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_penguin_init(struct ps_sprite *spr) {

  SPR->phase=PS_PENGUIN_PHASE_THREATEN;
  SPR->phasetime=PS_PENGUIN_THREATEN_TIME;

  return 0;
}

/* Configure.
 */

static int _ps_penguin_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_penguin_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_penguin_configure(), for editor.
  return 0;
}

/* Phase transitions.
 */

static int ps_penguin_begin_FALL(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_PENGUIN_PHASE_FALL;
  SPR->phasetime=spr->y/PS_PENGUIN_FALL_SPEED;
  return 0;
}

static int ps_penguin_begin_DAZED(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_PENGUIN_PHASE_DAZED;
  SPR->phasetime=PS_PENGUIN_DAZED_TIME;
  if (ps_game_set_group_mask_for_sprite(game,spr,PS_PENGUIN_NORMAL_GROUPS)<0) return -1;
  return 0;
}

static int ps_penguin_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_PENGUIN_PHASE_IDLE;
  SPR->phasetime=PS_PENGUIN_IDLE_TIME_MIN+rand()%(PS_PENGUIN_IDLE_TIME_MAX-PS_PENGUIN_IDLE_TIME_MIN);
  return 0;
}

static int ps_penguin_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_PENGUIN_PHASE_WALK;
  SPR->phasetime=PS_PENGUIN_WALK_TIME_MIN+rand()%(PS_PENGUIN_WALK_TIME_MAX-PS_PENGUIN_WALK_TIME_MIN);

  double t=(rand()%6282)/1000.0;
  SPR->walkdx=cos(t)*PS_PENGUIN_WALK_SPEED;
  SPR->walkdy=sin(t)*PS_PENGUIN_WALK_SPEED;

  const double xmargin=PS_TILESIZE*4.0;
  const double ymargin=PS_TILESIZE*2.0;
  if ((spr->x<xmargin)&&(SPR->walkdx<0.0)) SPR->walkdx=-SPR->walkdx;
  else if ((spr->x>PS_SCREENW-xmargin)&&(SPR->walkdx>0.0)) SPR->walkdx=-SPR->walkdx;
  if ((spr->y<ymargin)&&(SPR->walkdy<0.0)) SPR->walkdy=-SPR->walkdy;
  else if ((spr->y>PS_SCREENH-ymargin)&&(SPR->walkdy>0.0)) SPR->walkdy=-SPR->walkdy;

  return 0;
}

/* Update WALK.
 */

static int ps_penguin_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animtime)>=PS_PENGUIN_WALK_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_PENGUIN_WALK_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  spr->x+=SPR->walkdx;
  spr->y+=SPR->walkdy;

  return 0;
}

/* Update.
 */

static int _ps_penguin_update(struct ps_sprite *spr,struct ps_game *game) {

  if (--(SPR->phasetime)<=0) {
    switch (SPR->phase) {
      case PS_PENGUIN_PHASE_THREATEN: if (ps_penguin_begin_FALL(spr,game)<0) return -1; break;
      case PS_PENGUIN_PHASE_FALL: if (ps_penguin_begin_DAZED(spr,game)<0) return -1; break;
      case PS_PENGUIN_PHASE_DAZED: if (ps_penguin_begin_IDLE(spr,game)<0) return -1; break;
      case PS_PENGUIN_PHASE_IDLE: if (ps_penguin_begin_WALK(spr,game)<0) return -1; break;
      case PS_PENGUIN_PHASE_WALK: default: if (ps_penguin_begin_IDLE(spr,game)<0) return -1; break;
    }
  }

  switch (SPR->phase) {
    case PS_PENGUIN_PHASE_WALK: if (ps_penguin_update_WALK(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Count vertices.
 */

static int ps_penguin_count_vertices(const struct ps_sprite *spr) {
  switch (SPR->phase) {
    case PS_PENGUIN_PHASE_THREATEN: return 1;
    case PS_PENGUIN_PHASE_FALL: return 2;
    case PS_PENGUIN_PHASE_DAZED: return 1+PS_PENGUIN_DAZED_STAR_COUNT;
    default: return 1;
  }
}

/* Draw shadow, optionally growing based on an arbitrary counter.
 */

static void ps_penguin_draw_shadow(struct akgl_vtx_maxtile *vtx,const struct ps_sprite *spr,int p,int c) {
  vtx->x=spr->x;
  vtx->y=spr->y+PS_PENGUIN_SHADOW_OFFSET;
  vtx->tileid=spr->tileid;
  vtx->ta=0;
  vtx->pr=vtx->pg=vtx->pb=0x80;
  vtx->a=0xff;
  vtx->t=0;
  vtx->xform=AKGL_XFORM_NONE;
  if (p<=0) {
    vtx->size=PS_PENGUIN_SHADOW_SIZE_MIN;
  } else if (p>=c) {
    vtx->size=PS_PENGUIN_SHADOW_SIZE_MAX;
  } else {
    vtx->size=PS_PENGUIN_SHADOW_SIZE_MIN+((c-p)*(PS_PENGUIN_SHADOW_SIZE_MAX-PS_PENGUIN_SHADOW_SIZE_MIN))/c;
  }
}

/* Draw body.
 */

static void ps_penguin_draw_body(struct akgl_vtx_maxtile *vtx,const struct ps_sprite *spr,int dy,int dtile) {
  vtx->x=spr->x;
  vtx->y=spr->y-dy;
  vtx->tileid=spr->tileid+dtile;
  vtx->size=PS_TILESIZE;
  vtx->ta=0;
  vtx->pr=vtx->pg=vtx->pb=0x80;
  vtx->a=0xff;
  vtx->t=0;
  vtx->xform=AKGL_XFORM_NONE;
}

/* Draw stars.
 */

static int ps_penguin_get_star_color(int i) {
  switch (i&7) {
    case 0: return 0xff8080;
    case 1: return 0x00ff00;
    case 2: return 0xa0c0ff;
    case 3: return 0xffff00;
    case 4: return 0xff80ff;
    case 5: return 0xc080ff;
    case 6: return 0xffffff;
    case 7: return 0x000000;
  }
  return 0;
}

static void ps_penguin_draw_stars(struct akgl_vtx_maxtile *vtxv,int vtxc,const struct ps_sprite *spr) {
  vtxv->x=spr->x;
  vtxv->y=spr->y-PS_PENGUIN_STARS_OFFSET;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  double rotation=SPR->phasetime*PS_PENGUIN_STAR_SPEED;
  struct akgl_vtx_maxtile *vtx=vtxv;
  int i=vtxc; for (;i-->0;vtx++) memcpy(vtx,vtxv,sizeof(struct akgl_vtx_maxtile));
  for (i=0,vtx=vtxv;i<vtxc;i++,vtx++) {
    int rgb=ps_penguin_get_star_color(i);
    vtx->pr=rgb>>16;
    vtx->pg=rgb>>8;
    vtx->pb=rgb;
    vtx->tileid=(i&1)?0x0d:0x1d;
    double t=(i*M_PI*2.0)/vtxc;
    t+=rotation;
    vtx->x+=cos(t)*PS_PENGUIN_STAR_X_RADIUS;
    vtx->y-=sin(t)*PS_PENGUIN_STAR_Y_RADIUS;
  }
}

/* Draw.
 */

static int _ps_penguin_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=ps_penguin_count_vertices(spr);
  if (vtxc>vtxa) return vtxc;

  switch (SPR->phase) {
    case PS_PENGUIN_PHASE_THREATEN: {
        ps_penguin_draw_shadow(vtxv,spr,SPR->phasetime,PS_PENGUIN_THREATEN_TIME);
      } break;
    case PS_PENGUIN_PHASE_FALL: {
        int dy=SPR->phasetime*PS_PENGUIN_FALL_SPEED;
        ps_penguin_draw_shadow(vtxv,spr,1,1);
        ps_penguin_draw_body(vtxv+1,spr,dy,0x01);
      } break;
    case PS_PENGUIN_PHASE_DAZED: {
        ps_penguin_draw_body(vtxv,spr,0,0x13);
        ps_penguin_draw_stars(vtxv+1,vtxc-1,spr);
      } break;
    case PS_PENGUIN_PHASE_IDLE: {
        ps_penguin_draw_body(vtxv,spr,0,0x01);
      } break;
    case PS_PENGUIN_PHASE_WALK: {
        int frame=0x01;
        switch (SPR->animframe) {
          case 0: frame=0x02; break;
          case 2: frame=0x03; break;
        }
        ps_penguin_draw_body(vtxv,spr,0,frame);
      } break;
    default: return 0;
  }
  
  return vtxc;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_penguin={
  .name="penguin",
  .objlen=sizeof(struct ps_sprite_penguin),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_penguin_init,
  .del=_ps_penguin_del,
  .configure=_ps_penguin_configure,
  .get_configure_argument_name=_ps_penguin_get_configure_argument_name,
  .update=_ps_penguin_update,
  .draw=_ps_penguin_draw,
  
};
