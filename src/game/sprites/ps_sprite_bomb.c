/* ps_sprite_bomb.c
 * Counts down then explodes.
 * Optionally start with an arcing toss.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include "res/ps_resmgr.h"

#define PS_BOMB_DEFAULT_TIME 140
#define PS_BOMB_EXPLOSION_SPRDEFID 7
#define PS_BOMB_SPEED_LIMIT 2.5
#define PS_BOMB_SLOWDOWN_TIME 30

/* Private sprite object.
 */

struct ps_sprite_bomb {
  struct ps_sprite hdr;
  int counter;
  int countermax;
  int finished;
  int dx,dy;
  int throwttl;
};

#define SPR ((struct ps_sprite_bomb*)spr)

/* Delete.
 */

static void _ps_bomb_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_bomb_init(struct ps_sprite *spr) {

  SPR->counter=0;
  SPR->countermax=PS_BOMB_DEFAULT_TIME;

  return 0;
}

/* Configure.
 */

static int _ps_bomb_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_bomb_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_bomb_configure(), for editor.
  return 0;
}

/* Explode.
 */

static int ps_bomb_explode(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->finished) return 0;
  SPR->finished=1;

  PS_SFX_EXPLODE
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_BOMB_EXPLOSION_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for bomb explosion",PS_BOMB_EXPLOSION_SPRDEFID);
    return -1;
  }
  struct ps_sprite *explosion=ps_sprdef_instantiate(game,sprdef,0,0,spr->x,spr->y);
  if (!explosion) return -1;
  
  if (ps_sprite_kill_later(spr,game)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_bomb_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->counter)>=SPR->countermax) {
    return ps_bomb_explode(spr,game);
  }

  if (SPR->throwttl>0) {
    SPR->throwttl--;
    double speed=PS_BOMB_SPEED_LIMIT;
    if (SPR->throwttl<PS_BOMB_SLOWDOWN_TIME) {
      speed=(speed*SPR->throwttl)/PS_BOMB_SLOWDOWN_TIME;
    }
    spr->x+=SPR->dx*speed;
    spr->y+=SPR->dy*speed;
  }

  return 0;
}

/* Draw.
 */

static int _ps_bomb_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  int frame=(SPR->counter*8)/SPR->countermax;
  if (frame<0) frame=0; else if (frame>7) frame=7;
  vtxv->tileid=spr->tileid+frame;
  
  return 1;
}

/* Hurt.
 */

static int _ps_bomb_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return ps_bomb_explode(spr,game);
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_bomb={
  .name="bomb",
  .objlen=sizeof(struct ps_sprite_bomb),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_bomb_init,
  .del=_ps_bomb_del,
  .configure=_ps_bomb_configure,
  .get_configure_argument_name=_ps_bomb_get_configure_argument_name,
  .update=_ps_bomb_update,
  .draw=_ps_bomb_draw,
  
  .hurt=_ps_bomb_hurt,

};

/* Throw.
 */

int ps_sprite_bomb_throw(struct ps_sprite *spr,int direction,int magnitude) {
  if (!spr||(spr->type!=&ps_sprtype_bomb)) return -1;
  switch (direction) {
    case PS_DIRECTION_NORTH: SPR->dx=0; SPR->dy=-1; break;
    case PS_DIRECTION_SOUTH: SPR->dx=0; SPR->dy=1; break;
    case PS_DIRECTION_WEST: SPR->dx=-1; SPR->dy=0; break;
    case PS_DIRECTION_EAST: SPR->dx=1; SPR->dy=0; break;
  }
  SPR->throwttl=magnitude;
  return 0;
}
