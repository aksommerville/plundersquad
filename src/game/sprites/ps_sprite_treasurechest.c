#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/sprites/ps_sprite_hero.h"
#include "akgl/akgl.h"

#define PS_TREASURECHEST_DAZZLE_LIMIT 20 /* 1/256 circle from center */
#define PS_TREASURECHEST_DAZZLE_RATE   2 /* 1/256 circle per frame */

#define PS_TREASURECHEST_DAZZLE_SIZE_MIN ((PS_TILESIZE*150)/100)
#define PS_TREASURECHEST_DAZZLE_SIZE_MAX ((PS_TILESIZE*200)/100)
#define PS_TREASURECHEST_DAZZLE_SIZE_PERIOD 50

/* Private sprite object.
 */

struct ps_sprite_treasurechest {
  struct ps_sprite hdr;
  int dazzlet;
  int dazzletd;
  int dazzlesizep;
  int dazzlesized;
  int collected;
  int treasureid;
};

#define SPR ((struct ps_sprite_treasurechest*)spr)

/* Delete.
 */

static void _ps_treasurechest_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_treasurechest_init(struct ps_sprite *spr) {
  SPR->dazzlet=0;
  SPR->dazzletd=PS_TREASURECHEST_DAZZLE_RATE;
  SPR->dazzlesized=1;
  SPR->collected=0;
  return 0;
}

/* Configure.
 */

static int _ps_treasurechest_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) SPR->treasureid=argv[0];
  else SPR->treasureid=-1;
  SPR->collected=ps_game_get_treasure_state(game,SPR->treasureid);
  return 0;
}

static const char *_ps_treasurechest_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "Treasure ID";
  }
  return 0;
}

/* Look for collectors.
 */

static struct ps_sprite *ps_treasurechest_find_collector(struct ps_sprite *spr,struct ps_game *game) {
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  int i=0; for (;i<game->grpv[PS_SPRGRP_HERO].sprc;i++) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;
    if (!((struct ps_sprite_hero*)hero)->hp) continue;
    if (hero->x<left) continue;
    if (hero->x>right) continue;
    if (hero->y<top) continue;
    if (hero->y>bottom) continue;
    return hero;
  }
  return 0;
}

/* Update.
 */

static int _ps_treasurechest_update(struct ps_sprite *spr,struct ps_game *game) {

  SPR->dazzlet+=SPR->dazzletd;
  if (SPR->dazzlet>=PS_TREASURECHEST_DAZZLE_LIMIT) {
    SPR->dazzlet=PS_TREASURECHEST_DAZZLE_LIMIT;
    if (SPR->dazzletd>0) SPR->dazzletd=-PS_TREASURECHEST_DAZZLE_RATE;
  } else if (SPR->dazzlet<=-PS_TREASURECHEST_DAZZLE_LIMIT) {
    SPR->dazzlet=-PS_TREASURECHEST_DAZZLE_LIMIT;
    if (SPR->dazzletd<0) SPR->dazzletd=PS_TREASURECHEST_DAZZLE_RATE;
  }

  SPR->dazzlesizep+=SPR->dazzlesized;
  if (SPR->dazzlesizep>=PS_TREASURECHEST_DAZZLE_SIZE_PERIOD) {
    SPR->dazzlesizep=PS_TREASURECHEST_DAZZLE_SIZE_PERIOD;
    if (SPR->dazzlesized>0) SPR->dazzlesized=-SPR->dazzlesized;
  } else if (SPR->dazzlesizep<0) {
    SPR->dazzlesizep=0;
    if (SPR->dazzlesized<0) SPR->dazzlesized=-SPR->dazzlesized;
  }

  if (!SPR->collected) {
    struct ps_sprite *collector=ps_treasurechest_find_collector(spr,game);
    if (collector) {
      SPR->collected=1;
      if (ps_game_collect_treasure(game,collector,SPR->treasureid)<0) return -1;
    }
  }

  return 0;
}

/* Draw.
 */

static int _ps_treasurechest_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int vtxc=1;
  if (!SPR->collected) vtxc+=1;
  if (vtxc>vtxa) return vtxc;

  struct akgl_vtx_maxtile *vtx_chest,*vtx_dazzle;
  if (SPR->collected) {
    vtx_chest=vtxv;
    vtx_dazzle=0;
  } else {
    vtx_dazzle=vtxv;
    vtx_chest=vtxv+1;
  }

  vtx_chest->x=spr->x;
  vtx_chest->y=spr->y;
  vtx_chest->tileid=spr->tileid;
  if (SPR->collected) vtx_chest->tileid+=2;
  vtx_chest->size=PS_TILESIZE;
  vtx_chest->ta=0x00;
  vtx_chest->pr=vtx_chest->pg=vtx_chest->pb=0x80;
  vtx_chest->a=0xff;
  vtx_chest->t=0;
  vtx_chest->xform=AKGL_XFORM_NONE;

  if (vtx_dazzle) {
    memcpy(vtx_dazzle,vtx_chest,sizeof(struct akgl_vtx_maxtile));
    vtx_dazzle->tileid=spr->tileid+1;
    vtx_dazzle->t=SPR->dazzlet;
    vtx_dazzle->size=PS_TREASURECHEST_DAZZLE_SIZE_MIN+(SPR->dazzlesizep*(PS_TREASURECHEST_DAZZLE_SIZE_MAX-PS_TREASURECHEST_DAZZLE_SIZE_MIN))/PS_TREASURECHEST_DAZZLE_SIZE_PERIOD;
  }

  return vtxc;
}

/* Hurt.
 */

static int _ps_treasurechest_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_treasurechest={
  .name="treasurechest",
  .objlen=sizeof(struct ps_sprite_treasurechest),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,

  .init=_ps_treasurechest_init,
  .del=_ps_treasurechest_del,
  .configure=_ps_treasurechest_configure,
  .get_configure_argument_name=_ps_treasurechest_get_configure_argument_name,
  .update=_ps_treasurechest_update,
  .draw=_ps_treasurechest_draw,

};
