#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_BUG_FRAME_TIME 10
#define PS_BUG_FRAME_COUNT 2
#define PS_BUG_WAIT_TIME_MIN  30
#define PS_BUG_WAIT_TIME_MAX 120
#define PS_BUG_WALK_SPEED 1.0
#define PS_BUG_WALK_ABORT_TIME (PS_TILESIZE*3)

#define PS_BUG_PHASE_WAIT   1
#define PS_BUG_PHASE_WALK   2

/* Private sprite object.
 */

struct ps_sprite_bug {
  struct ps_sprite hdr;
  int animtimer;
  int animframe;
  int phase;
  int phasetime;
  double dstx,dsty;
};

#define SPR ((struct ps_sprite_bug*)spr)

/* Delete.
 */

static void _ps_bug_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_bug_init(struct ps_sprite *spr) {
  return 0;
}

/* Configure.
 */

static int _ps_bug_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Enter WAIT phase.
 */

static int ps_bug_begin_wait(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_BUG_PHASE_WAIT;
  SPR->phasetime=PS_BUG_WAIT_TIME_MIN+rand()%(PS_BUG_WAIT_TIME_MAX-PS_BUG_WAIT_TIME_MIN);
  return 0;
}

/* Select walk destination.
 */

static int ps_bug_cell_is_acceptable(const struct ps_sprite *spr,const struct ps_game *game,int col,int row) {
  if (col<0) return 0;
  if (row<0) return 0;
  if (col>=PS_GRID_COLC) return 0;
  if (row>=PS_GRID_ROWC) return 0;
  const struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
  if (spr->impassable&(1<<cell->physics)) return 0;
  return 1;
}

static int ps_bug_select_walk_direction(struct ps_sprite *spr,struct ps_game *game) {
  if (!game->grid) return -1;
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col<0)||(row<0)||(col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) {
    return ps_sprite_kill_later(spr,game);
  }

  int optionv[4];
  int optionc=0;
  if (ps_bug_cell_is_acceptable(spr,game,col-1,row)) optionv[optionc++]=PS_DIRECTION_WEST;
  if (ps_bug_cell_is_acceptable(spr,game,col+1,row)) optionv[optionc++]=PS_DIRECTION_EAST;
  if (ps_bug_cell_is_acceptable(spr,game,col,row-1)) optionv[optionc++]=PS_DIRECTION_NORTH;
  if (ps_bug_cell_is_acceptable(spr,game,col,row+1)) optionv[optionc++]=PS_DIRECTION_SOUTH;

  /* If we've been pushed onto a HEROONLY tile (easy to do with a hookshot), 
   * then we could easily be in an impossible position.
   * Detect this case, and try again with HEROONLY permitted.
   * If it should happen, we might permanently remove HEROONLY from the impassable mask.
   * I think that's better than mysteriously freezing the bug.
   */
  if (optionc<1) {
    if ((spr->impassable&(1<<PS_BLUEPRINT_CELL_HEROONLY))&&(game->grid->cellv[row*PS_GRID_COLC+col].physics==PS_BLUEPRINT_CELL_HEROONLY)) {
      optionc=0;
      spr->impassable&=~(1<<PS_BLUEPRINT_CELL_HEROONLY);
      if (ps_bug_cell_is_acceptable(spr,game,col-1,row)) optionv[optionc++]=PS_DIRECTION_WEST;
      if (ps_bug_cell_is_acceptable(spr,game,col+1,row)) optionv[optionc++]=PS_DIRECTION_EAST;
      if (ps_bug_cell_is_acceptable(spr,game,col,row-1)) optionv[optionc++]=PS_DIRECTION_NORTH;
      if (ps_bug_cell_is_acceptable(spr,game,col,row+1)) optionv[optionc++]=PS_DIRECTION_SOUTH;
      if (optionc<1) {
        spr->impassable|=(1<<PS_BLUEPRINT_CELL_HEROONLY);
        return -1;
      }
      ps_log(GAME,WARN,"Permanently removed HEROONLY from bug's impassable mask.");
    } else {
      return -1;
    }
  }

  /* We have options; pick one. */
  SPR->dstx=col*PS_TILESIZE+(PS_TILESIZE>>1);
  SPR->dsty=row*PS_TILESIZE+(PS_TILESIZE>>1);
  int direction=optionv[rand()%optionc];
  switch (direction) {
    case PS_DIRECTION_WEST:  SPR->dstx-=PS_TILESIZE; break;
    case PS_DIRECTION_EAST:  SPR->dstx+=PS_TILESIZE; break;
    case PS_DIRECTION_NORTH: SPR->dsty-=PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: SPR->dsty+=PS_TILESIZE; break;
    default: return -1;
  }

  return 0;
}

/* Enter WALK phase.
 */

static int ps_bug_begin_walk(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_bug_select_walk_direction(spr,game)<0) {
    return ps_bug_begin_wait(spr,game);
  }
  SPR->phase=PS_BUG_PHASE_WALK;
  SPR->phasetime=PS_BUG_WALK_ABORT_TIME; // maximum; we end the phase when we hit a cell boundary too.
  return 0;
}

/* Update in WALK phase.
 */

static int ps_bug_update_walk(struct ps_sprite *spr,struct ps_game *game) {

  int xok=0,yok=0;
  double dx=SPR->dstx-spr->x;
  if (dx<-PS_BUG_WALK_SPEED) spr->x-=PS_BUG_WALK_SPEED;
  else if (dx>PS_BUG_WALK_SPEED) spr->x+=PS_BUG_WALK_SPEED;
  else {
    spr->x=SPR->dstx;
    xok=1;
  }
  double dy=SPR->dsty-spr->y;
  if (dy<-PS_BUG_WALK_SPEED) spr->y-=PS_BUG_WALK_SPEED;
  else if (dy>PS_BUG_WALK_SPEED) spr->y+=PS_BUG_WALK_SPEED;
  else {
    spr->y=SPR->dsty;
    yok=1;
  }

  if (xok&&yok) {
    return ps_bug_begin_wait(spr,game);
  }

  return 0;
}

/* Update.
 */

static int _ps_bug_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Animation. */
  if (++(SPR->animtimer)>=PS_BUG_FRAME_TIME) {
    SPR->animtimer=0;
    if (++(SPR->animframe)>=PS_BUG_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  /* A weird and cumbersome hack: If we are being hookshotted, don't update normally.
   * Instead, force the next regular update to be a 'begin_walk'.
   */
  if (spr->master&&(spr->master->sprc>0)&&(spr->master->sprv[0]->type==&ps_sprtype_hookshot)) {
    SPR->phasetime=0;
    SPR->phase=PS_BUG_PHASE_WAIT;
    return 0;
  }

  /* Phase change? */
  if (--(SPR->phasetime)<=0) {
    switch (SPR->phase) {
      case PS_BUG_PHASE_WAIT: if (ps_bug_begin_walk(spr,game)<0) return -1; break;
      default: if (ps_bug_begin_wait(spr,game)<0) return -1; break;
    }
  }

  /* Update based on phase. */
  switch (SPR->phase) {
    case PS_BUG_PHASE_WAIT: break;
    case PS_BUG_PHASE_WALK: if (ps_bug_update_walk(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Draw.
 */

static int _ps_bug_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=spr->tileid+SPR->animframe;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->ta=0x00;
  vtxv->t=0x00;
  vtxv->xform=AKGL_XFORM_NONE;
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_bug={
  .name="bug",
  .objlen=sizeof(struct ps_sprite_bug),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_bug_init,
  .del=_ps_bug_del,
  .configure=_ps_bug_configure,
  .update=_ps_bug_update,
  .draw=_ps_bug_draw,

};
