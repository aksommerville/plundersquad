#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "game/sprites/ps_sprite_hero.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "akgl/akgl.h"

#define PS_MOTIONSENSOR_INACTIVE_COLOR_R 0x60
#define PS_MOTIONSENSOR_INACTIVE_COLOR_G 0x50
#define PS_MOTIONSENSOR_INACTIVE_COLOR_B 0x50

#define PS_MOTIONSENSOR_ACTIVE_COLOR_R 0x00
#define PS_MOTIONSENSOR_ACTIVE_COLOR_G 0xff
#define PS_MOTIONSENSOR_ACTIVE_COLOR_B 0x00

/* Private sprite object.
 */

struct ps_motionsensor_position {
  struct ps_sprite *spr; // Reference only, not retained.
  double x,y; // Last recorded position of this sprite.
};

struct ps_sprite_motionsensor {
  struct ps_sprite hdr;
  int active; // Counts down.
  int duration; // Constantish, time to remain active.
  struct ps_sprgrp *tmpgrp; // Transient use.
  struct ps_motionsensor_position *positionv;
  int positionc,positiona;
  int initial; // Don't record motion on my first update.
  int face_up; // Nonzero if pointing up instead of down.
};

#define SPR ((struct ps_sprite_motionsensor*)spr)

/* Delete.
 */

static void _ps_motionsensor_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->tmpgrp);
  ps_sprgrp_del(SPR->tmpgrp);
  if (SPR->positionv) free(SPR->positionv);
}

/* Initialize.
 */

static int _ps_motionsensor_init(struct ps_sprite *spr) {

  if (!(SPR->tmpgrp=ps_sprgrp_new())) return -1;
  SPR->duration=60;
  SPR->initial=1;

  return 0;
}

/* Configure.
 */

static int _ps_motionsensor_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      SPR->duration=argv[1];
    }
  }
  return 0;
}

static const char *_ps_motionsensor_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "duration";
  }
  return 0;
}

/* Change active state.
 */

static int ps_motionsensor_activate(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->duration<1) return 0;
  if (!SPR->active) {
    PS_SFX_MOTIONSENSOR_ACTIVATE
    if (ps_game_set_switch(game,spr->switchid,1)<0) return -1;
  }
  SPR->active=SPR->duration;
  return 0;
}

static int ps_motionsensor_deactivate(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->active) {
    PS_SFX_MOTIONSENSOR_DEACTIVATE
    if (ps_game_set_switch(game,spr->switchid,0)<0) return -1;
  }
  SPR->active=0;
  return 0;
}

/* Repopulate SPR->tmpgrp with the sprites in my line of sight.
 */

static int ps_motionsensor_gather_visible_sprites(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_sprgrp_clear(SPR->tmpgrp)<0) return -1;

  /* Grid may establish a vertical limit. */
  double top=spr->y;
  double bottom=PS_SCREENH;
  if (game->grid) {
    int col=(int)spr->x/PS_TILESIZE;
    if ((col>=0)&&(col<PS_GRID_COLC)) {

      if (SPR->face_up) {
        top=0.0;
        bottom=spr->y;
        int row=((int)spr->y/PS_TILESIZE)-1;
        if (row>=PS_GRID_ROWC) row=PS_GRID_ROWC-1;
        const struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
        for (;row>=0;row--,cell-=PS_GRID_COLC) {
          if (
            (cell->physics==PS_BLUEPRINT_CELL_SOLID)||
            (cell->physics==PS_BLUEPRINT_CELL_LATCH)
          ) {
            top=(row+1)*PS_TILESIZE;
            break;
          }
        }

      } else {
        int row=((int)spr->y/PS_TILESIZE)+1;
        if (row<0) row=0;
        const struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
        for (;row<PS_GRID_ROWC;row++,cell+=PS_GRID_COLC) {
          if (
            (cell->physics==PS_BLUEPRINT_CELL_SOLID)||
            (cell->physics==PS_BLUEPRINT_CELL_LATCH)
          ) {
            bottom=row*PS_TILESIZE;
            break;
          }
        }
      }
    }
  }

  /* Collect all SOLID sprites that intersect my line of sight. */
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_SOLID;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=grp->sprv[i];

    /* Special case for ghosts: They don't trip the sensor. */
    if ((pumpkin->type==&ps_sprtype_hero)&&(((struct ps_sprite_hero*)pumpkin)->hp<1)) {
      continue;
    }

    if (pumpkin->y<top) continue;
    if (pumpkin->y>bottom) continue;
    if (pumpkin->x+pumpkin->radius<left) continue;
    if (pumpkin->x-pumpkin->radius>right) continue;

    /* If pumpkin fully occludes my line of sight, change (bottom) to prevent occluding sprites appearing. */
    if ((pumpkin->x-pumpkin->radius<=left)&&(pumpkin->x+pumpkin->radius>=right)) {
      if (SPR->face_up) {
        top=pumpkin->y-pumpkin->radius;
      } else {
        bottom=pumpkin->y+pumpkin->radius;
      }
    }

    if (ps_sprgrp_add_sprite(SPR->tmpgrp,pumpkin)<0) return -1;
  }

  /* One more pass over the sprite list, examining (bottom) again. */
  if (SPR->face_up) {
    i=SPR->tmpgrp->sprc; while (i-->0) {
      struct ps_sprite *pumpkin=SPR->tmpgrp->sprv[i];
      if (pumpkin->y<top) {
        ps_sprgrp_remove_sprite(SPR->tmpgrp,pumpkin);
      }
    }
  } else {
    i=SPR->tmpgrp->sprc; while (i-->0) {
      struct ps_sprite *pumpkin=SPR->tmpgrp->sprv[i];
      if (pumpkin->y>bottom) {
        ps_sprgrp_remove_sprite(SPR->tmpgrp,pumpkin);
      }
    }
  }
  
  return 0;
}

/* Rebuild SPR->positionv from SPR->tmpgrp.
 */

static int ps_motionsensor_populate_positions(struct ps_sprite *spr) {

  if (SPR->tmpgrp->sprc>SPR->positiona) {
    int na=(SPR->tmpgrp->sprc+8)&~7;
    void *nv=realloc(SPR->positionv,sizeof(struct ps_motionsensor_position)*na);
    if (!nv) return -1;
    SPR->positionv=nv;
    SPR->positiona=na;
  }

  int i=SPR->tmpgrp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=SPR->tmpgrp->sprv[i];
    struct ps_motionsensor_position *position=SPR->positionv+i;
    position->spr=pumpkin;
    position->x=pumpkin->x;
    position->y=pumpkin->y;
  }
  SPR->positionc=SPR->tmpgrp->sprc;

  return 0;
}

/* Look for moving sprites in my line of sight.
 * Calls ps_motionsensor_activate() if any motion detected.
 */

static int ps_motionsensor_sense_motion(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_motionsensor_gather_visible_sprites(spr,game)<0) return -1;

  int motion=0;
  if (SPR->initial) {
    // Don't report any motion on my first update.
  } else if (SPR->positionc!=SPR->tmpgrp->sprc) {
    // Sprite added or removed from visibility -- motion
    motion=1;
  } else {
    // Both lists are sorted by address, just run down the line looking for differences.
    const struct ps_motionsensor_position *position=SPR->positionv;
    int i=0; for (;i<SPR->tmpgrp->sprc;i++,position++) {
      struct ps_sprite *pumpkin=SPR->tmpgrp->sprv[i];
      if (position->spr!=pumpkin) { motion=1; break; }
      if (position->x!=pumpkin->x) { motion=1; break; }
      if (position->y!=pumpkin->y) { motion=1; break; }
    }
  }

  if (ps_motionsensor_populate_positions(spr)<0) return -1;
  if (ps_sprgrp_clear(SPR->tmpgrp)<0) return -1;

  if (motion) {
    if (ps_motionsensor_activate(spr,game)<0) return -1;
  }
  
  return 0;
}

/* We normally can only point down.
 * If we are playing a blueprint that was flipped vertically, this might interfere with the puzzle design.
 * So on the first update, try to detect vertically-flipped blueprint, and change my orientation if so.
 */

static int ps_motionsensor_verify_orientation(struct ps_sprite *spr,struct ps_game *game) {
  if (!game) return 0;
  if (!game->grid) return 0;
  if (!game->scenario) return 0;
  if ((game->gridx<0)||(game->gridx>=game->scenario->w)) return 0;
  if ((game->gridy<0)||(game->gridy>=game->scenario->h)) return 0;
  struct ps_screen *screen=game->scenario->screenv+game->gridy*game->scenario->w+game->gridx;
  if (!screen->blueprint) return 0;
  
  if (screen->xform&PS_BLUEPRINT_XFORM_VERT) {
    SPR->face_up=1;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_motionsensor_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->initial) {
    if (ps_motionsensor_verify_orientation(spr,game)<0) return -1;
  }

  if (ps_motionsensor_sense_motion(spr,game)<0) return -1;

  if (SPR->active==1) {
    if (ps_motionsensor_deactivate(spr,game)<0) return -1;
  } else if (SPR->active>0) {
      SPR->active--;
  }

  if (SPR->initial) {
    SPR->initial=0;
  }
  return 0;
}

/* Draw.
 */

static int _ps_motionsensor_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=spr->tileid;
  vtxv->ta=0;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=SPR->face_up?AKGL_XFORM_FLIP:AKGL_XFORM_NONE;

  if (SPR->active>0) {
    vtxv->tileid+=0x01;
    vtxv->pr=PS_MOTIONSENSOR_INACTIVE_COLOR_R+(SPR->active*(PS_MOTIONSENSOR_ACTIVE_COLOR_R-PS_MOTIONSENSOR_INACTIVE_COLOR_R))/SPR->duration;
    vtxv->pg=PS_MOTIONSENSOR_INACTIVE_COLOR_G+(SPR->active*(PS_MOTIONSENSOR_ACTIVE_COLOR_G-PS_MOTIONSENSOR_INACTIVE_COLOR_G))/SPR->duration;
    vtxv->pb=PS_MOTIONSENSOR_INACTIVE_COLOR_B+(SPR->active*(PS_MOTIONSENSOR_ACTIVE_COLOR_B-PS_MOTIONSENSOR_INACTIVE_COLOR_B))/SPR->duration;
  } else {
    vtxv->pr=PS_MOTIONSENSOR_INACTIVE_COLOR_R;
    vtxv->pg=PS_MOTIONSENSOR_INACTIVE_COLOR_G;
    vtxv->pb=PS_MOTIONSENSOR_INACTIVE_COLOR_B;
  }
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_motionsensor={
  .name="motionsensor",
  .objlen=sizeof(struct ps_sprite_motionsensor),

  .radius=4,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_motionsensor_init,
  .del=_ps_motionsensor_del,
  .configure=_ps_motionsensor_configure,
  .get_configure_argument_name=_ps_motionsensor_get_configure_argument_name,
  .update=_ps_motionsensor_update,
  .draw=_ps_motionsensor_draw,
  
};
