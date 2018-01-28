/* ps_sprite_friedheart.c
 * Small visual effect when a prize or healmissile hits the kill-o-zap beam.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "akgl/akgl.h"

#define PS_FRIEDHEART_TTL 60

/* Private sprite object.
 */

struct ps_sprite_friedheart {
  struct ps_sprite hdr;
  int ttl;
};

#define SPR ((struct ps_sprite_friedheart*)spr)

/* Delete.
 */

static void _ps_friedheart_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_friedheart_init(struct ps_sprite *spr) {
  SPR->ttl=PS_FRIEDHEART_TTL;
  spr->tsid=0x04;
  spr->tileid=0x8e;
  return 0;
}

/* Configure.
 */

static int _ps_friedheart_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_friedheart_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_friedheart_configure(), for editor.
  return 0;
}

/* Update.
 */

static int _ps_friedheart_update(struct ps_sprite *spr,struct ps_game *game) {
  if (--(SPR->ttl)<=0) return ps_sprite_kill_later(spr,game);
  return 0;
}

/* Draw.
 */

static int _ps_friedheart_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].tileid=spr->tileid;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=AKGL_XFORM_NONE;

  if ((SPR->ttl/10)&1) {
    vtxv[0].pr=0xff; vtxv[0].pg=0xff; vtxv[0].pb=0x00;
  } else {
    vtxv[0].pr=0x00; vtxv[0].pg=0x00; vtxv[0].pb=0x00;
  }

  if (SPR->ttl<20) {
    vtxv[0].a=(SPR->ttl*20)/255;
  }
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_friedheart={
  .name="friedheart",
  .objlen=sizeof(struct ps_sprite_friedheart),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=120,

  .init=_ps_friedheart_init,
  .del=_ps_friedheart_del,
  .configure=_ps_friedheart_configure,
  .get_configure_argument_name=_ps_friedheart_get_configure_argument_name,
  .update=_ps_friedheart_update,
  .draw=_ps_friedheart_draw,

};
