#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "akgl/akgl.h"

#define PS_ELEFENCE_FRAME_TIME     9
#define PS_ELEFENCE_FRAME_COUNT    4
#define PS_ELEFENCE_HEAD_OFFSET_X  7
#define PS_ELEFENCE_HEAD_OFFSET_Y -6
#define PS_ELEFENCE_WALK_SPEED     0.8

#define PS_ELEFENCE_WALK_EPSILON 0.3
#define PS_ELEFENCE_OBSTRUCTION_TOLERANCE 10.0
#define PS_ELEFENCE_OBSTRUCTION_DROP 1.0
#define PS_ELEFENCE_FUZZ_IMPROBABLE 20.0

/* Private sprite object.
 */

struct ps_sprite_elefence {
  struct ps_sprite hdr;
  int animtime;
  int animframe;
  int facedx;
  double expectx; // My horizontal position last frame, ie where I expect to be at the start of this frame.
  double obstruction; // Counts up from zero to PS_ELEFENCE_OBSTRUCTION_TOLERANCE when movement come up short.
};

#define SPR ((struct ps_sprite_elefence*)spr)

/* Delete.
 */

static void _ps_elefence_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_elefence_init(struct ps_sprite *spr) {
  SPR->facedx=(rand()&1)?-1:1;
  return 0;
}

/* Update.
 */

static int _ps_elefence_update(struct ps_sprite *spr,struct ps_game *game) {
  
  if (++(SPR->animtime)>=PS_ELEFENCE_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_ELEFENCE_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }
  
  double walkfuzz; // How far off target are we (due to physics or interference). Positive when we come up short.
  if (SPR->facedx<0) {
    walkfuzz=spr->x-SPR->expectx;
  } else {
    walkfuzz=SPR->expectx-spr->x;
  }
  
  /* If walkfuzz is way off, scratch it and do nothing for a frame. */
  if ((walkfuzz>-PS_ELEFENCE_FUZZ_IMPROBABLE)&&(walkfuzz<PS_ELEFENCE_FUZZ_IMPROBABLE)) {
  
    /* If we are where we expect, or ahead of it, keep on truckin */
    if (walkfuzz<=PS_ELEFENCE_WALK_EPSILON) {
      if ((SPR->obstruction-=PS_ELEFENCE_OBSTRUCTION_DROP)<0.0) SPR->obstruction=0.0;
    
    /* If we are short, step the obstruction detector. */
    } else {
      SPR->obstruction+=walkfuzz;
      //ps_log(GAME,DEBUG,"elefence %p obstruction=%f",spr,SPR->obstruction);
      if (SPR->obstruction>=PS_ELEFENCE_OBSTRUCTION_TOLERANCE) {
        SPR->facedx=-SPR->facedx;
        SPR->obstruction=0.0;
      }
    }
    spr->x+=PS_ELEFENCE_WALK_SPEED*SPR->facedx;
  }
  
  SPR->expectx=spr->x;
  
  return 0;
}

/* Draw.
 */

static int _ps_elefence_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;
  struct akgl_vtx_maxtile *body=vtxv+0;
  struct akgl_vtx_maxtile *head=vtxv+1;
  
  body->x=spr->x;
  body->y=spr->y;
  body->size=PS_TILESIZE;
  body->tileid=spr->tileid;
  body->pr=body->pg=body->pb=0x80;
  body->ta=0;
  body->a=0xff;
  body->t=0;
  body->xform=AKGL_XFORM_NONE;
  memcpy(head,body,sizeof(struct akgl_vtx_maxtile));
  
  head->tileid=spr->tileid+0x10;
  head->x+=PS_ELEFENCE_HEAD_OFFSET_X*SPR->facedx;
  head->y+=PS_ELEFENCE_HEAD_OFFSET_Y;
  if (SPR->facedx>0) {
    head->xform=AKGL_XFORM_FLOP;
  }
  
  switch (SPR->animframe) {
    case 0: break;
    case 1: body->tileid++; break;
    case 2: break;
    case 3: body->tileid++; body->xform=AKGL_XFORM_FLOP; break;
  }
  
  return 2;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_elefence={
  .name="elefence",
  .objlen=sizeof(struct ps_sprite_elefence),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_elefence_init,
  .del=_ps_elefence_del,
  .update=_ps_elefence_update,
  .draw=_ps_elefence_draw,
  
};

/* Drop slave, public entry point (via ps_sprite_release_from_master()).
 */
 
int ps_sprite_elefence_drop_slave(struct ps_sprite *spr,struct ps_sprite *slave,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_elefence)) return -1;
  // This is no longer necessary.
  return 0;
}
