#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_path.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
#include "akgl/akgl.h"
#include "util/ps_geometry.h"

#define PS_BLOODHOUND_PHASE_SUMMON   0
#define PS_BLOODHOUND_PHASE_BARK     1
#define PS_BLOODHOUND_PHASE_SPIN     2
#define PS_BLOODHOUND_PHASE_COMPLETE 3

#define PS_BLOODHOUND_SUMMON_TIME  240
#define PS_BLOODHOUND_WALK_SPEED 1
#define PS_BLOODHOUND_FRAME_TIME 8

#define PS_BLOODHOUND_BARK_TIME 30

#define PS_BLOODHOUND_SPIN_TIME_MIN 60
#define PS_BLOODHOUND_SPIN_SPEED 5

/* Private sprite object.
 */

struct ps_sprite_bloodhound {
  struct ps_sprite hdr;
  int phase;
  int dx;
  uint8_t armt;
  uint8_t armt_target;
  int phase_counter;
  int animframe;
  int animcounter;
  struct ps_path path;
  int pathp;
};

#define SPR ((struct ps_sprite_bloodhound*)spr)

/* Delete.
 */

static void _ps_bloodhound_del(struct ps_sprite *spr) {
  ps_path_cleanup(&SPR->path);
}

/* Initialize.
 */

static int _ps_bloodhound_init(struct ps_sprite *spr) {

  SPR->phase=PS_BLOODHOUND_PHASE_SUMMON;

  return 0;
}

/* Configure.
 */

static int _ps_bloodhound_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_bloodhound_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_bloodhound_configure(), for editor.
  return 0;
}

/* Generate path from current position to somewhere near the center.
 */

static int ps_bloodhound_valid_destination(const struct ps_grid_cell *cellv,int x,int y) {
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=PS_GRID_COLC) return 0;
  if (y>=PS_GRID_ROWC) return 0;
  if (cellv[y*PS_GRID_COLC+x].physics!=PS_BLUEPRINT_CELL_VACANT) return 0;
  return 1;
}

static int ps_bloodhound_generate_path(struct ps_sprite *spr,struct ps_game *game) {

  int srcx=(int)spr->x/PS_TILESIZE;
  int srcy=(int)spr->y/PS_TILESIZE;

  /* Start near the center and radiate outward until we find a VACANT cell with a valid path from where we are.
   */
  int midx=PS_GRID_COLC>>1;
  int midy=PS_GRID_ROWC>>1;
  int distance=0;
  while (1) {
    int xa=midx-distance;
    int xz=midx+distance;
    int ya=midy-distance;
    int yz=midy+distance;
    if ((xa<0)&&(ya<0)&&(xz>=PS_GRID_COLC)&&(yz>=PS_GRID_ROWC)) break;
    int i;
    for (i=ya;i<=yz;i++) {
      if (ps_bloodhound_valid_destination(game->grid->cellv,xa,i)) {
        if (ps_game_compose_grid_path(&SPR->path,game->grid,xa,i,srcx,srcy,spr->impassable)>=0) return 0;
      }
      if (xz!=xa) {
        if (ps_bloodhound_valid_destination(game->grid->cellv,xz,i)) {
          if (ps_game_compose_grid_path(&SPR->path,game->grid,xz,i,srcx,srcy,spr->impassable)>=0) return 0;
        }
      }
    }
    for (i=xa+1;i<=xz-1;i++) {
      if (ps_bloodhound_valid_destination(game->grid->cellv,i,ya)) {
        if (ps_game_compose_grid_path(&SPR->path,game->grid,i,ya,srcx,srcy,spr->impassable)>=0) return 0;
      }
      if (ya!=yz) {
        if (ps_bloodhound_valid_destination(game->grid->cellv,i,yz)) {
          if (ps_game_compose_grid_path(&SPR->path,game->grid,i,yz,srcx,srcy,spr->impassable)>=0) return 0;
        }
      }
    }
    distance++;
  }

  /* We can leave the path empty and get a reasonable-ish fallback. */
  ps_log(GAME,INFO,"Failed to generate path for bloodhound.");
  return 0;
}

/* Enter BARK phase.
 */

static int ps_bloodhound_begin_BARK(struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_WOOF
  SPR->phase=PS_BLOODHOUND_PHASE_BARK;
  SPR->phase_counter=PS_BLOODHOUND_BARK_TIME;
  return 0;
}

/* Enter SPIN phase.
 */

static int ps_bloodhound_begin_SPIN(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_BLOODHOUND_PHASE_SPIN;
  SPR->phase_counter=PS_BLOODHOUND_SPIN_TIME_MIN;
  int n;
  switch (n=ps_game_get_direction_to_nearest_treasure(game)) {
    case PS_DIRECTION_NORTH: SPR->armt_target=0x80; break;
    case PS_DIRECTION_SOUTH: SPR->armt_target=0x00; break;
    case PS_DIRECTION_WEST:  SPR->armt_target=0x40; break;
    case PS_DIRECTION_EAST:  SPR->armt_target=0xc0; break;
  }
  return 0;
}

/* Walk towards the given cell.
 */

static int ps_bloodhound_walk_to_cell(struct ps_sprite *spr,struct ps_game *game,int col,int row) {
  int dstx=col*PS_TILESIZE+(PS_TILESIZE>>1);
  int dsty=row*PS_TILESIZE+(PS_TILESIZE>>1);
  int xok=0,yok=0;
  int dx=dstx-spr->x;
  if (dx>PS_BLOODHOUND_WALK_SPEED) {
    SPR->dx=1;
    spr->x+=PS_BLOODHOUND_WALK_SPEED;
  } else if (dx<-PS_BLOODHOUND_WALK_SPEED) {
    SPR->dx=-1;
    spr->x-=PS_BLOODHOUND_WALK_SPEED;
  } else {
    xok=1;
  }
  int dy=dsty-spr->y;
  if (dy>PS_BLOODHOUND_WALK_SPEED) {
    spr->y+=PS_BLOODHOUND_WALK_SPEED;
  } else if (dy<-PS_BLOODHOUND_WALK_SPEED) {
    spr->y-=PS_BLOODHOUND_WALK_SPEED;
  } else {
    yok=1;
  }
  if (xok&&yok) {
    SPR->pathp++;
  }
  return 0;
}

/* Walk with no guidance.
 */

static int ps_bloodhound_walk_unguided(struct ps_sprite *spr,struct ps_game *game) {
  int dx=(PS_SCREENW>>1)-spr->x;
  if (dx>PS_BLOODHOUND_WALK_SPEED) {
    SPR->dx=1;
    spr->x+=PS_BLOODHOUND_WALK_SPEED;
  } else if (dx<-PS_BLOODHOUND_WALK_SPEED) {
    SPR->dx=-1;
    spr->x-=PS_BLOODHOUND_WALK_SPEED;
  }
  int dy=(PS_SCREENH>>1)-spr->y;
  if (dy>PS_BLOODHOUND_WALK_SPEED) {
    spr->y+=PS_BLOODHOUND_WALK_SPEED;
  } else if (dy<-PS_BLOODHOUND_WALK_SPEED) {
    spr->y-=PS_BLOODHOUND_WALK_SPEED;
  }
  return 0;
}

/* Update, in SUMMON phase.
 */

static int ps_bloodhound_update_SUMMON(struct ps_sprite *spr,struct ps_game *game) {

  if (!SPR->path.c) {
    if (ps_bloodhound_generate_path(spr,game)<0) return -1;
  }

  SPR->animcounter++;
  if (SPR->animcounter>=PS_BLOODHOUND_FRAME_TIME) {
    SPR->animcounter=0;
    SPR->animframe++;
    if (SPR->animframe>=2) SPR->animframe=0;
  }

  SPR->phase_counter++;
  if (SPR->phase_counter>=PS_BLOODHOUND_SUMMON_TIME) {
    if (ps_bloodhound_begin_BARK(spr,game)<0) return -1;
  } else {
    if (SPR->pathp<SPR->path.c) {
      if (ps_bloodhound_walk_to_cell(spr,game,SPR->path.v[SPR->pathp].x,SPR->path.v[SPR->pathp].y)<0) return -1;
    } else if (!SPR->path.c) {
      if (ps_bloodhound_walk_unguided(spr,game)<0) return -1;
    } else {
      if (ps_bloodhound_begin_BARK(spr,game)<0) return -1;
    }
  }
  
  return 0;
}

/* Update, in BARK phase.
 */

static int ps_bloodhound_update_BARK(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->phase_counter--<0) {
    return ps_bloodhound_begin_SPIN(spr,game);
  }
  return 0;
}

/* Update, in SPIN phase.
 */

static int ps_bloodhound_update_SPIN(struct ps_sprite *spr,struct ps_game *game) {
  SPR->armt+=PS_BLOODHOUND_SPIN_SPEED;
  if (SPR->phase_counter>0) {
    SPR->phase_counter--;
  } else {
    int d=SPR->armt-SPR->armt_target;
    if (d<0) d=-d;
    if (d<=PS_BLOODHOUND_SPIN_SPEED) {
      SPR->armt=SPR->armt_target;
      SPR->phase=PS_BLOODHOUND_PHASE_COMPLETE;
    }
  }
  return 0;
}

/* Update.
 */

static int _ps_bloodhound_update(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,DEBUG,"bloodhound %p (%d,%d) phase=%d",spr,(int)spr->x,(int)spr->y,SPR->phase);
  switch (SPR->phase) {
    case PS_BLOODHOUND_PHASE_SUMMON: if (ps_bloodhound_update_SUMMON(spr,game)<0) return -1; break;
    case PS_BLOODHOUND_PHASE_BARK: if (ps_bloodhound_update_BARK(spr,game)<0) return -1; break;
    case PS_BLOODHOUND_PHASE_SPIN: if (ps_bloodhound_update_SPIN(spr,game)<0) return -1; break;
  }
  return 0;
}

/* Draw.
 */

static int ps_bloodhound_draw_walking(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  if (SPR->phase==PS_BLOODHOUND_PHASE_BARK) {
    vtxv->tileid=spr->tileid;
  } else {
    vtxv->tileid=spr->tileid+1+SPR->animframe;
  }
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=(SPR->dx<0)?AKGL_XFORM_FLOP:AKGL_XFORM_NONE;
  return 1;
}

static int ps_bloodhound_draw_sitting(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;
  struct akgl_vtx_maxtile *body=vtxv;
  struct akgl_vtx_maxtile *arm=vtxv+1;
  
  body->x=spr->x;
  body->y=spr->y;
  body->tileid=spr->tileid+3;
  body->size=PS_TILESIZE;
  body->ta=0;
  body->pr=vtxv->pg=vtxv->pb=0x80;
  body->a=0xff;
  body->t=0;
  vtxv->xform=(SPR->dx<0)?AKGL_XFORM_FLOP:AKGL_XFORM_NONE;

  memcpy(arm,body,sizeof(struct akgl_vtx_maxtile));
  arm->tileid=spr->tileid+4;
  arm->t=SPR->armt;
  arm->xform=AKGL_XFORM_NONE;
  
  return 2;
}

static int _ps_bloodhound_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  switch (SPR->phase) {
    case PS_BLOODHOUND_PHASE_SUMMON: 
    case PS_BLOODHOUND_PHASE_BARK: return ps_bloodhound_draw_walking(vtxv,vtxa,spr);
    case PS_BLOODHOUND_PHASE_SPIN:
    case PS_BLOODHOUND_PHASE_COMPLETE: return ps_bloodhound_draw_sitting(vtxv,vtxa,spr);
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_bloodhound={
  .name="bloodhound",
  .objlen=sizeof(struct ps_sprite_bloodhound),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_bloodhound_init,
  .del=_ps_bloodhound_del,
  .configure=_ps_bloodhound_configure,
  .get_configure_argument_name=_ps_bloodhound_get_configure_argument_name,
  .update=_ps_bloodhound_update,
  .draw=_ps_bloodhound_draw,
  
};
