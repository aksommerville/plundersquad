#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "res/ps_resmgr.h"
#include <math.h>

#define PS_MISSILE_SPRDEF_ID_DEFAULT 11
#define PS_MISSILE_SPEED 3.0
#define PS_MISSILE_FRAME_TIME 5

/* Private sprite object.
 */

struct ps_sprite_missile {
  struct ps_sprite hdr;
  double dstx,dsty;
  double dx,dy;
  int animtime;
  int animframe;
  int stop_at_walls;
};

#define SPR ((struct ps_sprite_missile*)spr)

/* Delete.
 */

static void _ps_missile_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_missile_init(struct ps_sprite *spr) {
  return 0;
}

/* Set new destination.
 */

static int ps_missile_set_destination(struct ps_sprite *spr,double dstx,double dsty) {
  SPR->dstx=dstx;
  SPR->dsty=dsty;
  double distancex=dstx-spr->x;
  double distancey=dsty-spr->y;
  double distance=sqrt(distancex*distancex+distancey*distancey);
  if (distance>0.0) {
    SPR->dx=(distancex*PS_MISSILE_SPEED)/distance;
    SPR->dy=(distancey*PS_MISSILE_SPEED)/distance;
  } else switch (rand()%3) {
    case 0: SPR->dx=PS_MISSILE_SPEED; SPR->dy=0.0; break;
    case 1: SPR->dx=-PS_MISSILE_SPEED; SPR->dy=0.0; break;
    case 2: SPR->dy=PS_MISSILE_SPEED; SPR->dx=0.0; break;
    case 3: SPR->dy=-PS_MISSILE_SPEED; SPR->dx=0.0; break;
  }
  return 0;
}

/* Configure.
 */

static int _ps_missile_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Are we touching a wall?
 */

static int ps_missile_touches_wall(const struct ps_sprite *spr,const struct ps_game *game) {
  if (!game) return 0;
  
  if (game->grid) {
    if ((spr->x>=0.0)&&(spr->y>=0.0)) {
      int col=spr->x/PS_TILESIZE;
      int row=spr->y/PS_TILESIZE;
      if ((col<PS_GRID_COLC)&&(row<=PS_GRID_ROWC)) {
        uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
        if (spr->impassable&(1<<physics)) return 1;
      }
    }
  }

  return 0;
}

/* Update.
 */

static int _ps_missile_update(struct ps_sprite *spr,struct ps_game *game) {

  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  if (
    (spr->x<-PS_TILESIZE)||(spr->x>PS_SCREENW+PS_TILESIZE)||
    (spr->y<-PS_TILESIZE)||(spr->y>PS_SCREENH+PS_TILESIZE)
  ) return ps_sprite_kill_later(spr,game);

  if (++(SPR->animtime)>=PS_MISSILE_FRAME_TIME) {
    SPR->animtime=0;
    if (SPR->animframe^=1) spr->tileid++; else spr->tileid--;
  }

  if (SPR->stop_at_walls&&ps_missile_touches_wall(spr,game)) {
    return ps_sprite_kill_later(spr,game);
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_missile={
  .name="missile",
  .objlen=sizeof(struct ps_sprite_missile),

  .radius=PS_TILESIZE>>2,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=150,

  .init=_ps_missile_init,
  .del=_ps_missile_del,
  .configure=_ps_missile_configure,
  .update=_ps_missile_update,

};

/* Public ctor.
 */
 
struct ps_sprite *ps_sprite_missile_new(int sprdefid,struct ps_sprite *user,double dstx,double dsty,struct ps_game *game) {
  if (!user||!game) return 0;
  if (sprdefid<=0) sprdefid=PS_MISSILE_SPRDEF_ID_DEFAULT;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found, expected for MISSILE",sprdefid);
    return 0;
  }
  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,user->x,user->y);
  if (!spr) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",sprdefid);
    return 0;
  }

  if (ps_missile_set_destination(spr,dstx,dsty)<0) {
    ps_sprite_kill(spr);
    return 0;
  }
  
  return spr;
}

/* Accessors.
 */
 
int ps_sprite_missile_set_stop_at_walls(struct ps_sprite *spr,int stop) {
  if (!spr||(spr->type!=&ps_sprtype_missile)) return -1;
  SPR->stop_at_walls=stop;
  return 0;
}
