/* ps_sprite_yak.c
 * A double-wide monster that walks around.
 * I'd like to have it walk to water, drink and fill up, then walk elsewhere and spit the water out.
 * Getting motion and physics first.
 *
 * There are two sprites, butt and head.
 * Butt sprite is invisible, the head draws both pieces.
 * Head tile is always located at the sprite's principal position, whichever direction we are facing.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_YAK_PHASE_IDLE      0
#define PS_YAK_PHASE_WALK      1
#define PS_YAK_PHASE_DRINK     2
#define PS_YAK_PHASE_SPIT      3

#define PS_YAK_IDLE_TIME_MIN  30
#define PS_YAK_IDLE_TIME_MAX 100

#define PS_YAK_WALK_SPEED 0.7
#define PS_YAK_WALK_FRAME_TIME 8
#define PS_YAK_WALK_FRAME_COUNT 4
#define PS_YAK_WALK_TIME_MIN  40
#define PS_YAK_WALK_TIME_MAX 200

#define PS_YAK_DRINK_TIME 180
#define PS_YAK_DRINK_FRAME_TIME 8
#define PS_YAK_DRINK_FRAME_COUNT 2

#define PS_YAK_SPIT_TIME 120
#define PS_YAK_SPIT_FRAME_TIME 5
#define PS_YAK_SPIT_FRAME_COUNT 2

#define PS_YAK_BUTT_EPSILON 0.001

/* Private sprite object.
 */

struct ps_sprite_yak {
  struct ps_sprite hdr;
  struct ps_sprgrp *butt; // Make one extra invisible sprite for my butt's physics.
  int facedx;
  int phase;
  int phasetime;
  int phaselimit;
  int animtime;
  int animframe;
  double walkdx,walkdy;
  int force_butt_position; // Set nonzero if the butt might have moved wildly, to take it as-is.
  int first_frame;
  int full_of_water;
};

#define SPR ((struct ps_sprite_yak*)spr)

/* Delete.
 */

static void _ps_yak_del(struct ps_sprite *spr) {
  ps_sprgrp_kill(SPR->butt);
  ps_sprgrp_del(SPR->butt);
}

/* Initialize.
 */

static int _ps_yak_init(struct ps_sprite *spr) {

  if (!(SPR->butt=ps_sprgrp_new())) return -1;

  SPR->facedx=-1;
  SPR->first_frame=1;

  return 0;
}

/* Configure.
 */

static int _ps_yak_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_yak_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_yak_configure(), for editor.
  return 0;
}

/* Create butt sprite.
 */

static int ps_yak_create_butt(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprite *butt=ps_sprite_new(&ps_sprtype_dummy);
  if (!butt) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_KEEPALIVE,butt)<0) {
    ps_sprite_del(butt);
    return -1;
  }
  ps_sprite_del(butt);

  if (ps_sprgrp_add_sprite(SPR->butt,butt)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_SOLID,butt)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_PHYSICS,butt)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,butt)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_FRAGILE,butt)<0) return -1;

  butt->shape=spr->shape;
  butt->radius=spr->radius;
  butt->collide_sprites=spr->collide_sprites;
  butt->impassable=spr->impassable;

  SPR->force_butt_position=1;

  return 0;
}

/* Set butt position.
 * We don't just blindly position it -- If the butt was acted upon by physics, react to that.
 */

static int ps_yak_position_butt(struct ps_sprite *spr) {
  if (!SPR->butt) return 0;
  if (SPR->butt->sprc<1) return 0;
  struct ps_sprite *butt=SPR->butt->sprv[0];

  double targetx,targety;
  if (SPR->facedx<0) {
    targetx=spr->x+PS_TILESIZE;
  } else {
    targetx=spr->x-PS_TILESIZE;
  }
  targety=spr->y;

  if (SPR->force_butt_position) {
    butt->x=targetx;
    butt->y=targety;
    SPR->force_butt_position=0;
  } else {
    double expectx=butt->x; // Where the butt would be, if head moved without interference.
    double expecty=butt->y;
    if (SPR->phase==PS_YAK_PHASE_WALK) {
      expectx+=SPR->walkdx;
      expecty+=SPR->walkdy;
    }
    double errx=expectx-targetx; if (errx<0.0) errx=-errx;
    double erry=expecty-targety; if (erry<0.0) erry=-erry;
    if (errx+erry>PS_YAK_BUTT_EPSILON) {
      butt->x=(expectx+targetx)/2.0;
      butt->y=(expecty+targety)/2.0;
      if (SPR->facedx<0) {
        spr->x=butt->x-PS_TILESIZE;
      } else {
        spr->x=butt->x+PS_TILESIZE;
      }
      spr->y=butt->y;
    } else {
      butt->x=targetx;
      butt->y=targety;
    }
  }
  
  return 0;
}

/* Look for any fragile sprites in the path of my waterspout.
 */

static int ps_yak_check_spit_victims(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_fbox dangerbox;
  dangerbox.n=spr->y-2.0;
  dangerbox.s=spr->y+6.0;
  if (SPR->facedx<0) {
    dangerbox.w=spr->x-PS_TILESIZE-7.0;
    dangerbox.e=spr->x-PS_TILESIZE-3.0;
  } else {
    dangerbox.w=spr->x+PS_TILESIZE+3.0;
    dangerbox.e=spr->x+PS_TILESIZE+7.0;
  }

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (ps_sprite_collide_fbox(victim,&dangerbox)) {
      if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
    }
  }

  return 0;
}

/* Begin IDLE phase.
 */

static int ps_yak_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_YAK_PHASE_IDLE;
  SPR->phaselimit=PS_YAK_IDLE_TIME_MIN+rand()%(PS_YAK_IDLE_TIME_MAX-PS_YAK_IDLE_TIME_MIN);
  return 0;
}

/* Begin WALK phase.
 */

static int ps_yak_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_YAK_PHASE_WALK;
  SPR->phaselimit=PS_YAK_WALK_TIME_MIN+rand()%(PS_YAK_WALK_TIME_MAX-PS_YAK_WALK_TIME_MIN);
  SPR->animframe=0;

  double t=(rand()%6282)/1000.0;
  SPR->walkdx=cos(t)*PS_YAK_WALK_SPEED;
  SPR->walkdy=-sin(t)*PS_YAK_WALK_SPEED;

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
    if (SPR->facedx>0) {
      spr->x-=PS_TILESIZE;
      SPR->facedx=-1;
      SPR->force_butt_position=1;
    }
  } else {
    if (SPR->facedx<0) {
      spr->x+=PS_TILESIZE;
      SPR->facedx=1;
      SPR->force_butt_position=1;
    }
  }
  return 0;
}

/* Begin DRINK phase.
 */

static int ps_yak_begin_DRINK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_YAK_PHASE_DRINK;
  SPR->phaselimit=PS_YAK_DRINK_TIME;
  SPR->animframe=0;
  SPR->full_of_water=1;
  return 0;
}

/* Begin SPIT phase.
 */

static int ps_yak_begin_SPIT(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_YAK_PHASE_SPIT;
  SPR->phaselimit=PS_YAK_SPIT_TIME;
  SPR->animframe=0;
  SPR->full_of_water=0;
  return 0;
}

/* Update WALK phase.
 */

static int ps_yak_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animtime)>=PS_YAK_WALK_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_YAK_WALK_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  spr->x+=SPR->walkdx;
  spr->y+=SPR->walkdy;
  
  return 0;
}

/* Update DRINK phase.
 */

static int ps_yak_update_DRINK(struct ps_sprite *spr,struct ps_game *game) {
  if (++(SPR->animtime)>=PS_YAK_DRINK_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_YAK_DRINK_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }
  return 0;
}

/* Update SPIT phase.
 */

static int ps_yak_update_SPIT(struct ps_sprite *spr,struct ps_game *game) {
  if (++(SPR->animtime)>=PS_YAK_SPIT_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_YAK_SPIT_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }
  if (ps_yak_check_spit_victims(spr,game)<0) return -1;
  return 0;
}

/* Are we standing at water's edge, facing the water?
 */

static int ps_yak_can_drink_here(const struct ps_sprite *spr,const struct ps_game *game) {

  /* If we are already full, we can't drink anywhere. */
  if (SPR->full_of_water) return 0;

  if (!game||!game->grid) return 0;

  /* Take a sample point at about where the snout will be if we do drink.
   * On second thought, take two sample points: One a little high and one a little low.
   */
  int x,ya,yz;
  x=spr->x+((PS_TILESIZE*7)/8)*SPR->facedx;
  ya=spr->y;
  yz=spr->y+((PS_TILESIZE*5)/8);

  if ((x<0)||(ya<0)) return 0;
  int col=x/PS_TILESIZE;
  int rowa=ya/PS_TILESIZE;
  int rowz=yz/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(rowz>=PS_GRID_ROWC)) return 0;

  uint8_t physics=game->grid->cellv[rowa*PS_GRID_COLC+col].physics;
  if (physics!=PS_BLUEPRINT_CELL_HOLE) return 0;
  physics=game->grid->cellv[rowz*PS_GRID_COLC+col].physics;
  if (physics!=PS_BLUEPRINT_CELL_HOLE) return 0;

  return 1;
}

/* Choose a new phase, IDLE or WALK.
 */

static int ps_yak_choose_phase(struct ps_sprite *spr,struct ps_game *game) {

  /* Finding a drink position is kind of rare, since we do nothing to encourage it.
   * So if it becomes possible, do it.
   */
  if (ps_yak_can_drink_here(spr,game)) {
    return ps_yak_begin_DRINK(spr,game);
  }

  /* If SPIT is an option, give it 1/4 odds.
   */
  if (SPR->full_of_water) {
    if (!(rand()&3)) {
      return ps_yak_begin_SPIT(spr,game);
    }
  }

  /* Choose randomly between IDLE and WALK.
   */
  int selection=rand()%10;
  if (selection<5) {
    return ps_yak_begin_IDLE(spr,game);
  } else {
    return ps_yak_begin_WALK(spr,game);
  }
}

/* Select a new phase.
 */

static int ps_yak_advance_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_YAK_PHASE_IDLE: return ps_yak_choose_phase(spr,game);
    case PS_YAK_PHASE_WALK: return ps_yak_begin_IDLE(spr,game);
    case PS_YAK_PHASE_DRINK: return ps_yak_begin_IDLE(spr,game);
    case PS_YAK_PHASE_SPIT: return ps_yak_begin_IDLE(spr,game);
    default: return ps_yak_begin_IDLE(spr,game);
  }
}

/* Update, dispatch on phase.
 */

static int ps_yak_update_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_YAK_PHASE_WALK: return ps_yak_update_WALK(spr,game);
    case PS_YAK_PHASE_DRINK: return ps_yak_update_DRINK(spr,game);
    case PS_YAK_PHASE_SPIT: return ps_yak_update_SPIT(spr,game);
  }
  return 0;
}

/* Update.
 */

static int _ps_yak_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->phasetime)>=SPR->phaselimit) {
    SPR->phasetime=0;
    if (ps_yak_advance_phase(spr,game)<0) return -1;
  } else {
    if (ps_yak_update_phase(spr,game)<0) return -1;
  }

  /* We can't really create the butt without a reference to game, so it's done here instead of during init.
   * We also hijack this test to look for heroes attacking the butt.
   * If butt disappeared and it's not our first frame, quietly kill this sprite.
   */
  if (SPR->butt->sprc<1) {
    if (SPR->first_frame) {
      if (ps_yak_create_butt(spr,game)<0) return -1;
    } else {
      return ps_sprite_kill_later(spr,game);
    }
  }

  if (ps_yak_position_butt(spr)<0) return -1;
  SPR->first_frame=0;
  return 0;
}

/* Draw.
 */

static int _ps_yak_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=2;
  if (SPR->phase==PS_YAK_PHASE_DRINK) {
    vtxc=3;
  } else if (SPR->phase==PS_YAK_PHASE_SPIT) {
    vtxc=3;
  }
  if (vtxa<vtxc) return vtxc;

  struct akgl_vtx_maxtile *vtx_head=vtxv;
  struct akgl_vtx_maxtile *vtx_butt=vtxv+1;
  struct akgl_vtx_maxtile *vtx_snout=(SPR->phase==PS_YAK_PHASE_DRINK)?(vtxv+2):0;
  struct akgl_vtx_maxtile *vtx_spout=(SPR->phase==PS_YAK_PHASE_SPIT)?(vtxv+2):0;

  vtx_head->x=spr->x;
  vtx_head->y=spr->y;
  vtx_head->tileid=spr->tileid;
  vtx_head->size=PS_TILESIZE;
  vtx_head->ta=0;
  vtx_head->pr=0x80;
  vtx_head->pg=0x80;
  vtx_head->pb=0x80;
  vtx_head->a=0xff;
  vtx_head->t=0;
  vtx_head->xform=(SPR->facedx<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  switch (SPR->phase) {
    case PS_YAK_PHASE_WALK: {
        switch (SPR->animframe) {
          case 0: vtx_head->tileid+=0x02; break;
          case 2: vtx_head->tileid+=0x12; break;
        }
      } break;
    case PS_YAK_PHASE_DRINK: {
        switch (SPR->animframe) {
          case 0: vtx_head->tileid+=0x05; break;
          case 1: vtx_head->tileid+=0x15; break;
        }
      } break;
    case PS_YAK_PHASE_SPIT: {
        vtx_head->tileid+=0x08;
      } break;
  }

  memcpy(vtx_butt,vtx_head,sizeof(struct akgl_vtx_maxtile));
  vtx_butt->tileid+=1;
  if (SPR->facedx<0) {
    vtx_butt->x+=PS_TILESIZE;
  } else {
    vtx_butt->x-=PS_TILESIZE;
  }

  if (vtx_snout) {
    memcpy(vtx_snout,vtx_head,sizeof(struct akgl_vtx_maxtile));
    vtx_snout->tileid-=1;
    if (SPR->facedx<0) {
      vtx_snout->x-=PS_TILESIZE;
    } else {
      vtx_snout->x+=PS_TILESIZE;
    }
  }

  if (vtx_spout) {
    memcpy(vtx_spout,vtx_head,sizeof(struct akgl_vtx_maxtile));
    vtx_spout->tileid-=1;
    if (SPR->animframe) vtx_spout->tileid+=0x10;
    if (SPR->facedx<0) {
      vtx_spout->x-=PS_TILESIZE;
    } else {
      vtx_spout->x+=PS_TILESIZE;
    }
  }
  
  return vtxc;
}

/* Hurt.
 * We only need this hook because the yak is two sprites, which are both fragile.
 */

static int _ps_yak_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  // No matter what else, don't let this fire again.
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;

  /* Get the butt. If it's on death row, pretend it doesn't exist. */
  struct ps_sprite *butt=0;
  if (SPR->butt&&SPR->butt->sprc) {
    butt=SPR->butt->sprv[0];
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_DEATHROW,butt)) butt=0;
  }

  // Butt present: Remove it and do the monster-killed dance.
  if (butt) {
    if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,butt)<0) return -1;
    if (ps_sprite_kill_later(butt,game)<0) return -1;
    if (ps_game_decorate_monster_death(game,spr->x,spr->y)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;

  // Butt absent: Assume that it made the decorations, and quietly kill myself.
  } else {
    if (ps_sprite_kill_later(spr,game)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_yak={
  .name="yak",
  .objlen=sizeof(struct ps_sprite_yak),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_yak_init,
  .del=_ps_yak_del,
  .configure=_ps_yak_configure,
  .get_configure_argument_name=_ps_yak_get_configure_argument_name,
  .update=_ps_yak_update,
  .draw=_ps_yak_draw,
  .hurt=_ps_yak_hurt,

};
