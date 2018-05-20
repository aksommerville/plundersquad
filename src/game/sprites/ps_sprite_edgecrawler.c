#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "akgl/akgl.h"

#define PS_EDGECRAWLER_ROTATION_SPEED 6
#define PS_EDGECRAWLER_SPEED 1.5
#define PS_EDGECRAWLER_CORRECTION_SPEED 0.1

/* Private sprite object.
 */

struct ps_sprite_edgecrawler {
  struct ps_sprite hdr;
  uint8_t t; // for display only
  int wallx,wally; // cell we are attached to (INT_MIN if detached)
  int dx,dy;
};

#define SPR ((struct ps_sprite_edgecrawler*)spr)

/* Delete.
 */

static void _ps_edgecrawler_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_edgecrawler_init(struct ps_sprite *spr) {
  SPR->wallx=INT_MIN;
  SPR->wally=INT_MIN;
  return 0;
}

/* Configure.
 */

static int _ps_edgecrawler_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_edgecrawler_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_edgecrawler_configure(), for editor.
  return 0;
}

/* Find wall from scratch.
 */

static int ps_edgecrawler_cell_is_wall(const struct ps_sprite *spr,const struct ps_game *game,int col,int row) {
  if ((col<0)||(col>=PS_GRID_COLC)) return 1;
  if ((row<0)||(row>=PS_GRID_ROWC)) return 1;
  uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
  if (spr->impassable&(1<<physics)) return 1;
  return 0;
}

static int ps_edgecrawler_find_wall(struct ps_sprite *spr,struct ps_game *game) {
  if (!game||!game->grid) return 0;
  if (spr->x<0.0) return 0;
  if (spr->y<0.0) return 0;
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if (col>=PS_GRID_COLC) return 0;
  if (row>=PS_GRID_ROWC) return 0;
  SPR->dx=0;
  SPR->dy=0;

  if (ps_edgecrawler_cell_is_wall(spr,game,col,row)) return 0;
  if (ps_edgecrawler_cell_is_wall(spr,game,col-1,row)) { SPR->wallx=col-1; SPR->wally=row; SPR->dy=1; return 0; }
  if (ps_edgecrawler_cell_is_wall(spr,game,col+1,row)) { SPR->wallx=col+1; SPR->wally=row; SPR->dy=-1; return 0; }
  if (ps_edgecrawler_cell_is_wall(spr,game,col,row-1)) { SPR->wallx=col; SPR->wally=row-1; SPR->dx=-1; return 0; }
  if (ps_edgecrawler_cell_is_wall(spr,game,col,row+1)) { SPR->wallx=col; SPR->wally=row+1; SPR->dx=1; return 0; }
  
  return 0;
}

/* Move a value towards the nearest tile center.
 */

static double ps_edgecrawler_approach_alignment(double v) {
  int cell=v/PS_TILESIZE;
  double target=cell*PS_TILESIZE+(PS_TILESIZE>>1);
  if (v<target-PS_EDGECRAWLER_CORRECTION_SPEED) return v+PS_EDGECRAWLER_CORRECTION_SPEED;
  if (v>target+PS_EDGECRAWLER_CORRECTION_SPEED) return v-PS_EDGECRAWLER_CORRECTION_SPEED;
  return target;
}

/* Advance.
 */

static int ps_edgecrawler_advance(struct ps_sprite *spr,struct ps_game *game) {

  /* Advance blindly and record the new cell we are in. */
  if (SPR->dx) {
    spr->x+=SPR->dx*PS_EDGECRAWLER_SPEED;
  } else {
    spr->x=ps_edgecrawler_approach_alignment(spr->x);
  }
  if (SPR->dy) {
    spr->y+=SPR->dy*PS_EDGECRAWLER_SPEED;
  } else {
    spr->y=ps_edgecrawler_approach_alignment(spr->y);
  }
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;

  /* If our leading edge has entered an impassable cell, we must turn counterclockwise. */
  int leadx=spr->x+SPR->dx*spr->radius;
  int leady=spr->y+SPR->dy*spr->radius;
  int leadcol=leadx/PS_TILESIZE; if (leadx<0) leadcol--;
  int leadrow=leady/PS_TILESIZE; if (leady<0) leadrow--;
  if (ps_edgecrawler_cell_is_wall(spr,game,leadcol,leadrow)) {
    SPR->wallx=leadcol;
    SPR->wally=leadrow;
         if (SPR->dx>0) { SPR->dx=0; SPR->dy=-1; }
    else if (SPR->dy<0) { SPR->dx=-1; SPR->dy=0; }
    else if (SPR->dx<0) { SPR->dx=0; SPR->dy=1; }
    else if (SPR->dy>0) { SPR->dx=1; SPR->dy=0; }
    return 0;
  }

  /* If we've aligned with the next cell, advance the "wall" and turn clockwise if passable. */
  if (SPR->dx>0) {
    if (spr->x-spr->radius>=(SPR->wallx+1)*PS_TILESIZE) {
      if (ps_edgecrawler_cell_is_wall(spr,game,SPR->wallx+1,SPR->wally)) {
        SPR->wallx++;
      } else {
        SPR->dx=0;
        SPR->dy=1;
      }
    }
  } else if (SPR->dy>0) {
    if (spr->y-spr->radius>=(SPR->wally+1)*PS_TILESIZE) {
      if (ps_edgecrawler_cell_is_wall(spr,game,SPR->wallx,SPR->wally+1)) {
        SPR->wally++;
      } else {
        SPR->dx=-1;
        SPR->dy=0;
      }
    }
  } else if (SPR->dx<0) {
    if (spr->x+spr->radius<=SPR->wallx*PS_TILESIZE) {
      if (ps_edgecrawler_cell_is_wall(spr,game,SPR->wallx-1,SPR->wally)) {
        SPR->wallx--;
      } else {
        SPR->dx=0;
        SPR->dy=-1;
      }
    }
  } else if (SPR->dy<0) {
    if (spr->y+spr->radius<=SPR->wally*PS_TILESIZE) {
      if (ps_edgecrawler_cell_is_wall(spr,game,SPR->wallx,SPR->wally-1)) {
        SPR->wally--;
      } else {
        SPR->dx=1;
        SPR->dy=0;
      }
    }
  }

  return 0;
}

/* Update.
 */

static int _ps_edgecrawler_update(struct ps_sprite *spr,struct ps_game *game) {

  SPR->t+=PS_EDGECRAWLER_ROTATION_SPEED;

  if ((SPR->wallx==INT_MIN)||(SPR->wally==INT_MIN)) {
    if (ps_edgecrawler_find_wall(spr,game)<0) return -1;
  } else {
    if (ps_edgecrawler_advance(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_edgecrawler_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;
  struct akgl_vtx_maxtile *vtx_back=vtxv;
  struct akgl_vtx_maxtile *vtx_front=vtxv+1;

  vtx_back->x=vtx_front->x=spr->x;
  vtx_back->y=vtx_front->y=spr->y;
  vtx_back->tileid=spr->tileid;
  vtx_front->tileid=spr->tileid+0x10;
  vtx_back->size=vtx_front->size=PS_TILESIZE;
  vtx_back->ta=vtx_front->ta=0;
  vtx_back->pr=vtx_front->pr=0x80;
  vtx_back->pg=vtx_front->pg=0x80;
  vtx_back->pb=vtx_front->pb=0x80;
  vtx_back->a=vtx_front->a=0xff;
  vtx_back->t=SPR->t;
  vtx_front->t=0;
  vtx_back->xform=vtx_front->xform=AKGL_XFORM_NONE;
  
  return 2;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_edgecrawler={
  .name="edgecrawler",
  .objlen=sizeof(struct ps_sprite_edgecrawler),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_edgecrawler_init,
  .del=_ps_edgecrawler_del,
  .configure=_ps_edgecrawler_configure,
  .get_configure_argument_name=_ps_edgecrawler_get_configure_argument_name,
  .update=_ps_edgecrawler_update,
  .draw=_ps_edgecrawler_draw,
  
};
