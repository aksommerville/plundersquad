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

/* Private sprite object.
 */

struct ps_sprite_bug {
  struct ps_sprite hdr;
  int animtimer;
  int animframe;
  int waittimer;
  int walkaborttimer;
  double dstx,dsty; // Aim to walk here.
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

/* List possible moves.
 * Populate (dirv) with PS_DIRECTION_* for all valid options.
 */

static int ps_bug_cell_usable(struct ps_game *game,int col,int row) {
  switch (game->grid->cellv[row*PS_GRID_COLC+col].physics) {
    case PS_BLUEPRINT_CELL_VACANT: return 1;
    case PS_BLUEPRINT_CELL_HEAL: return 1;
  }
  return 0;
}

static int ps_bug_list_possible_moves(int *dirv,struct ps_game *game,int col,int row) {
  int dirc=0;
  if ((col>0)&&ps_bug_cell_usable(game,col-1,row)) dirv[dirc++]=PS_DIRECTION_WEST;
  if ((row>0)&&ps_bug_cell_usable(game,col,row-1)) dirv[dirc++]=PS_DIRECTION_NORTH;
  if ((col<PS_GRID_COLC-1)&&ps_bug_cell_usable(game,col+1,row)) dirv[dirc++]=PS_DIRECTION_EAST;
  if ((row<PS_GRID_ROWC-1)&&ps_bug_cell_usable(game,col,row+1)) dirv[dirc++]=PS_DIRECTION_SOUTH;
  return dirc;
}

/* Calculate next move.
 */

static int ps_bug_calculate_next_move(struct ps_sprite *spr,struct ps_game *game) {

  int col=spr->x/PS_TILESIZE;
  if (col<0) col=0; else if (col>=PS_GRID_COLC) col=PS_GRID_COLC-1;
  int row=spr->y/PS_TILESIZE;
  if (row<0) row=0; else if (row>=PS_GRID_ROWC) row=PS_GRID_ROWC-1;

  int okdirv[4];
  int okdirc=ps_bug_list_possible_moves(okdirv,game,col,row);

  if (okdirc<1) {
    SPR->dstx=spr->x;
    SPR->dsty=spr->y;
  } else {
    int dir=okdirv[rand()%okdirc];
    struct ps_vector vector=ps_vector_from_direction(dir);
    col+=vector.dx;
    row+=vector.dy;
    SPR->dstx=col*PS_TILESIZE+(PS_TILESIZE>>1);
    SPR->dsty=row*PS_TILESIZE+(PS_TILESIZE>>1);
  }

  SPR->waittimer=PS_BUG_WAIT_TIME_MIN+rand()%(PS_BUG_WAIT_TIME_MAX-PS_BUG_WAIT_TIME_MIN);
  SPR->walkaborttimer=PS_BUG_WALK_ABORT_TIME;

  return 0;
}

/* Walk.
 */

static int ps_bug_walk(struct ps_sprite *spr,struct ps_game *game) {

  int done=1;
  double dx=SPR->dstx-spr->x;
  if ((dx>=-PS_BUG_WALK_SPEED)&&(dx<=PS_BUG_WALK_SPEED)) {
    spr->x=SPR->dstx;
  } else if (spr->x>SPR->dstx) {
    done=0;
    spr->x-=PS_BUG_WALK_SPEED;
  } else if (spr->x<SPR->dstx) {
    done=0;
    spr->x+=PS_BUG_WALK_SPEED;
  }
  double dy=SPR->dsty-spr->y;
  if ((dy>=-PS_BUG_WALK_SPEED)&&(dy<=PS_BUG_WALK_SPEED)) {
    spr->y=SPR->dsty;
  } else if (spr->y>SPR->dsty) {
    done=0;
    spr->y-=PS_BUG_WALK_SPEED;
  } else if (spr->y<SPR->dsty) {
    done=0;
    spr->y+=PS_BUG_WALK_SPEED;
  }

  if (--(SPR->walkaborttimer)<=0) done=1;

  if (done) {
    if (ps_bug_calculate_next_move(spr,game)<0) return -1;
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

  /* Movement. */
  if (SPR->waittimer>0) {
    SPR->waittimer--;
  } else {
    if (ps_bug_walk(spr,game)<0) return -1;
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

/* Hurt.
 */

static int _ps_bug_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return ps_sprite_kill_later(spr,game);
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
  
  //.hurt=_ps_bug_hurt,

};
