#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_BOXINGGLOVE_STRUT_COUNT 6
#define PS_BOXINGGLOVE_STRUT_OFFSET_MIN 2
#define PS_BOXINGGLOVE_STRUT_OFFSET_MAX 16
#define PS_BOXINGGLOVE_STRUT_ANGLE_MIN 8
#define PS_BOXINGGLOVE_STRUT_ANGLE_MAX 64

#define PS_BOXINGGLOVE_PHASE_IDLE 0
#define PS_BOXINGGLOVE_PHASE_ADVANCE 1
#define PS_BOXINGGLOVE_PHASE_RETREAT 2

#define PS_BOXINGGLOVE_ADVANCE_SPEED 2.5
#define PS_BOXINGGLOVE_RETREAT_SPEED 1.0

/* Private sprite object.
 */

struct ps_sprite_boxingglove {
  struct ps_sprite hdr;
  int direction;
  int base; // x or y depending on direction; where I am connected to the wall. 0 if not initialized
  int limit; // furthest extent of x or y depending on direction.
  int home; // rest position of x or y
  int phase;
  int enable;
};

#define SPR ((struct ps_sprite_boxingglove*)spr)

/* Delete.
 */

static void _ps_boxingglove_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_boxingglove_init(struct ps_sprite *spr) {
  SPR->direction=PS_DIRECTION_SOUTH;
  SPR->enable=1;
  return 0;
}

/* Configure.
 */

static int _ps_boxingglove_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    SPR->direction=argv[0];
    if (argc>=2) {
      spr->switchid=argv[1];
    }
  }
  return 0;
}

static const char *_ps_boxingglove_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "dir (N,S,W,E)";
    case 1: return "switchid";
  }
  return 0;
}

/* Deferred init.
 */

static int ps_boxingglove_setup(struct ps_sprite *spr,struct ps_game *game) {

  /* If no switch, we start enabled. */
  if (spr->switchid) SPR->enable=0;

  /* If the grid is flipped from its blueprint, we must flip too. Check both axes. */
  if (game&&game->scenario) {
    struct ps_screen *screen=game->scenario->screenv+game->gridy*game->scenario->w+game->gridx;
    switch (SPR->direction) {
      case PS_DIRECTION_SOUTH: if (screen->xform&PS_BLUEPRINT_XFORM_VERT) SPR->direction=PS_DIRECTION_NORTH; break;
      case PS_DIRECTION_NORTH: if (screen->xform&PS_BLUEPRINT_XFORM_VERT) SPR->direction=PS_DIRECTION_SOUTH; break;
      case PS_DIRECTION_WEST: if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) SPR->direction=PS_DIRECTION_EAST; break;
      case PS_DIRECTION_EAST: if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) SPR->direction=PS_DIRECTION_WEST; break;
    }
  }

  /* Take some measurements. */
  const int abslimit=PS_BOXINGGLOVE_STRUT_COUNT*PS_BOXINGGLOVE_STRUT_OFFSET_MAX;
  const int homebase=PS_BOXINGGLOVE_STRUT_COUNT*PS_BOXINGGLOVE_STRUT_OFFSET_MIN;
  switch (SPR->direction) {
    case PS_DIRECTION_SOUTH: {
        SPR->base=spr->y-(PS_TILESIZE>>1);
        SPR->home=spr->y+homebase;
        spr->y=SPR->home;
      } break;
    case PS_DIRECTION_NORTH: {
        SPR->base=spr->y+(PS_TILESIZE>>1);
        SPR->home=spr->y-homebase;
        spr->y=SPR->home;
      } break;
    case PS_DIRECTION_WEST: {
        SPR->base=spr->x+(PS_TILESIZE>>1);
        SPR->home=spr->x-homebase;
        spr->x=SPR->home;
      } break;
    case PS_DIRECTION_EAST: {
        SPR->base=spr->x-(PS_TILESIZE>>1);
        SPR->home=spr->x+homebase;
        spr->x=SPR->home;
      } break;
    default: {
        ps_log(GAME,ERROR,"Invalid direction %d for boxingglove.",SPR->direction);
        return -1;
      }
  }
  if (!SPR->base) SPR->base=1;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH:
    case PS_DIRECTION_WEST: {
        SPR->limit=SPR->base-abslimit;
      } break;
    case PS_DIRECTION_SOUTH:
    case PS_DIRECTION_EAST: {
        SPR->limit=SPR->base+abslimit;
      } break;
  }
  
  return 0;
}

/* Detect potential victims in my line of sight. Return 0 or 1.
 */

static int ps_boxingglove_detect_victim(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_fbox bounds=ps_fbox(spr->x-6.0,spr->x+6.0,spr->y-6.0,spr->y+6.0);
  double extra=PS_TILESIZE*2.0; // Trigger on movement a little beyond my actual range.
  switch (SPR->direction) {
    case PS_DIRECTION_SOUTH: {
        bounds.n=SPR->home;
        bounds.s=SPR->limit+extra;
      } break;
    case PS_DIRECTION_NORTH: {
        bounds.n=SPR->limit-extra;
        bounds.s=SPR->home;
      } break;
    case PS_DIRECTION_WEST: {
        bounds.w=SPR->limit-extra;
        bounds.e=SPR->home;
      } break;
    case PS_DIRECTION_EAST: {
        bounds.w=SPR->home;
        bounds.e=SPR->limit+extra;
      } break;
  }

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->type==&ps_sprtype_prize) continue; // This gets ridiculous
    if (victim->x+victim->radius<bounds.w) continue;
    if (victim->x-victim->radius>bounds.e) continue;
    if (victim->y+victim->radius<bounds.n) continue;
    if (victim->y-victim->radius>bounds.s) continue;
    return 1;
  }

  return 0;
}

/* Victim detected: enter ADVANCE phase.
 */

static int ps_boxingglove_begin_advance(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_BOXINGGLOVE_PHASE_ADVANCE;
  return 0;
}

/* Update in ADVANCE phase.
 */

static int ps_boxingglove_update_advance(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: {
        spr->y-=PS_BOXINGGLOVE_ADVANCE_SPEED;
        if (spr->y<=SPR->limit) SPR->phase=PS_BOXINGGLOVE_PHASE_RETREAT;
      } break;
    case PS_DIRECTION_SOUTH: {
        spr->y+=PS_BOXINGGLOVE_ADVANCE_SPEED;
        if (spr->y>=SPR->limit) SPR->phase=PS_BOXINGGLOVE_PHASE_RETREAT;
      } break;
    case PS_DIRECTION_WEST: {
        spr->x-=PS_BOXINGGLOVE_ADVANCE_SPEED;
        if (spr->x<=SPR->limit) SPR->phase=PS_BOXINGGLOVE_PHASE_RETREAT;
      } break;
    case PS_DIRECTION_EAST: {
        spr->x+=PS_BOXINGGLOVE_ADVANCE_SPEED;
        if (spr->x>=SPR->limit) SPR->phase=PS_BOXINGGLOVE_PHASE_RETREAT;
      } break;
  }
  return 0;
}

/* Update in RETREAT phase.
 */

static int ps_boxingglove_update_retreat(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: {
        spr->y+=PS_BOXINGGLOVE_RETREAT_SPEED;
        if (spr->y>=SPR->home) {
          spr->y=SPR->home;
          SPR->phase=PS_BOXINGGLOVE_PHASE_IDLE;
        }
      } break;
    case PS_DIRECTION_SOUTH: {
        spr->y-=PS_BOXINGGLOVE_RETREAT_SPEED;
        if (spr->y<=SPR->home) {
          spr->y=SPR->home;
          SPR->phase=PS_BOXINGGLOVE_PHASE_IDLE;
        }
      } break;
    case PS_DIRECTION_WEST: {
        spr->x+=PS_BOXINGGLOVE_RETREAT_SPEED;
        if (spr->x>=SPR->home) {
          spr->x=SPR->home;
          SPR->phase=PS_BOXINGGLOVE_PHASE_IDLE;
        }
      } break;
    case PS_DIRECTION_EAST: {
        spr->x-=PS_BOXINGGLOVE_RETREAT_SPEED;
        if (spr->x<=SPR->home) {
          spr->x=SPR->home;
          SPR->phase=PS_BOXINGGLOVE_PHASE_IDLE;
        }
      } break;
  }
  return 0;
}

/* Update.
 */

static int _ps_boxingglove_update(struct ps_sprite *spr,struct ps_game *game) {

  if (!SPR->base) {
    if (ps_boxingglove_setup(spr,game)<0) return -1;
  }

  switch (SPR->phase) {
    case PS_BOXINGGLOVE_PHASE_IDLE: {
        if (SPR->enable) {
          if (ps_boxingglove_detect_victim(spr,game)) {
            if (ps_boxingglove_begin_advance(spr,game)<0) return -1;
          }
        }
      } break;
    case PS_BOXINGGLOVE_PHASE_ADVANCE: {
        if (ps_boxingglove_update_advance(spr,game)<0) return -1;
      } break;
    case PS_BOXINGGLOVE_PHASE_RETREAT: {
        if (ps_boxingglove_update_retreat(spr,game)<0) return -1;
      } break;
  }

  return 0;
}

/* Draw.
 */

static int _ps_boxingglove_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=1+PS_BOXINGGLOVE_STRUT_COUNT*2;
  if (vtxc>vtxa) return vtxc;

  int strutsend;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: strutsend=spr->y+(PS_TILESIZE>>1); break;
    case PS_DIRECTION_SOUTH: strutsend=spr->y-(PS_TILESIZE>>1); break;
    case PS_DIRECTION_WEST: strutsend=spr->x+(PS_TILESIZE>>1); break;
    case PS_DIRECTION_EAST: strutsend=spr->x-(PS_TILESIZE>>1); break;
    default: return -1;
  }

  int length_per_strut=(strutsend-SPR->base)/PS_BOXINGGLOVE_STRUT_COUNT;
  if (length_per_strut<0) length_per_strut=-length_per_strut;
  if (length_per_strut<PS_BOXINGGLOVE_STRUT_OFFSET_MIN) length_per_strut=PS_BOXINGGLOVE_STRUT_OFFSET_MIN;
  else if (length_per_strut>PS_BOXINGGLOVE_STRUT_OFFSET_MAX) length_per_strut=PS_BOXINGGLOVE_STRUT_OFFSET_MAX;
  const int offsetrange=PS_BOXINGGLOVE_STRUT_OFFSET_MAX-PS_BOXINGGLOVE_STRUT_OFFSET_MIN;
  const int anglerange=PS_BOXINGGLOVE_STRUT_ANGLE_MAX-PS_BOXINGGLOVE_STRUT_ANGLE_MIN;
  int extent;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: extent=SPR->home-spr->y; break;
    case PS_DIRECTION_SOUTH: extent=spr->y-SPR->home; break;
    case PS_DIRECTION_WEST: extent=SPR->home-spr->x; break;
    case PS_DIRECTION_EAST: extent=spr->x-SPR->home; break;
  }
  const int extentrange=PS_BOXINGGLOVE_STRUT_COUNT*offsetrange;
  int angle=PS_BOXINGGLOVE_STRUT_ANGLE_MIN+((extentrange-extent)*anglerange)/extentrange;

  int x=spr->x,y=spr->y,dx=0,dy=0;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: y=SPR->base-length_per_strut/2; dy=-length_per_strut; break;
    case PS_DIRECTION_SOUTH: y=SPR->base+length_per_strut/2; dy=length_per_strut; break;
    case PS_DIRECTION_WEST: x=SPR->base-length_per_strut/2; dx=-length_per_strut; break;
    case PS_DIRECTION_EAST: x=SPR->base+length_per_strut/2; dx=length_per_strut; break;
  }
  int xform=AKGL_XFORM_NONE;
  if ((SPR->direction==PS_DIRECTION_WEST)||(SPR->direction==PS_DIRECTION_EAST)) xform=AKGL_XFORM_90;

  struct akgl_vtx_maxtile *vtx=vtxv;
  int i=0; for (;i<PS_BOXINGGLOVE_STRUT_COUNT;i++,vtx+=2) {
    vtx[0].x=x;
    vtx[0].y=y;
    vtx[0].tileid=spr->tileid-0x10;
    vtx[0].size=PS_TILESIZE;
    vtx[0].ta=0;
    vtx[0].pr=0x80;
    vtx[0].pg=0x80;
    vtx[0].pb=0x80;
    vtx[0].a=0xff;
    vtx[0].t=angle;
    vtx[0].xform=xform;
    memcpy(vtx+1,vtx,sizeof(struct akgl_vtx_maxtile));
    vtx[1].t=256-angle;
    x+=dx;
    y+=dy;
  }

  vtx=vtxv+vtxc-1;
  vtx->x=spr->x;
  vtx->y=spr->y;
  vtx->tileid=spr->tileid;
  vtx->size=PS_TILESIZE;
  if (SPR->enable) {
    vtx->ta=0;
  } else {
    vtx->tr=vtx->tg=vtx->tb=0x80;
    vtx->ta=0xc0;
  }
  vtx->pr=0x80;
  vtx->pg=0x80;
  vtx->pb=0x80;
  vtx->a=0xff;
  vtx->t=0;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtx->xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_SOUTH: vtx->xform=AKGL_XFORM_NONE; break;
    case PS_DIRECTION_WEST: vtx->xform=AKGL_XFORM_90; break;
    case PS_DIRECTION_EAST: vtx->xform=AKGL_XFORM_270; break;
  }
  
  return vtxc;
}

/* Set switch.
 */

static int _ps_boxingglove_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  SPR->enable=value;
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_boxingglove={
  .name="boxingglove",
  .objlen=sizeof(struct ps_sprite_boxingglove),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_boxingglove_init,
  .del=_ps_boxingglove_del,
  .configure=_ps_boxingglove_configure,
  .get_configure_argument_name=_ps_boxingglove_get_configure_argument_name,
  .update=_ps_boxingglove_update,
  .draw=_ps_boxingglove_draw,
  
  .set_switch=_ps_boxingglove_set_switch,

};
