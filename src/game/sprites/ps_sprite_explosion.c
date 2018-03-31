#include "ps.h"
#include "game/ps_sprite.h"
#include "akgl/akgl.h"

#define PS_EXPLOSION_TTL_DEFAULT 80
#define PS_EXPLOSION_FADE_OUT_TIME  20
#define PS_EXPLOSION_FRESH_TIME 5

/* Private sprite object.
 */

struct ps_sprite_explosion {
  struct ps_sprite hdr;
  int ttl;
};

#define SPR ((struct ps_sprite_explosion*)spr)

/* Initialize.
 */

static int _ps_explosion_init(struct ps_sprite *spr) {
  SPR->ttl=PS_EXPLOSION_TTL_DEFAULT;
  return 0;
}

/* Update.
 */

static int _ps_explosion_update(struct ps_sprite *spr,struct ps_game *game) {
  if (--(SPR->ttl)<=0) return ps_sprite_kill_later(spr,game);
  return 0;
}

/* Draw.
 */

static int _ps_explosion_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<9) return 9;

  /* Initialize the first vertex. */
  vtxv->x=spr->x-PS_TILESIZE;
  vtxv->y=spr->y-PS_TILESIZE;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=spr->tileid-0x11;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->ta=0x00;
  vtxv->t=0x00;
  vtxv->xform=AKGL_XFORM_NONE;

  /* As we approach end of life, fade out. */
  if (SPR->ttl<PS_EXPLOSION_FADE_OUT_TIME) {
    vtxv->a=(SPR->ttl*0xff)/PS_EXPLOSION_FADE_OUT_TIME;
  }

  /* Copy to the other eight and adjust position and tileid. */
  struct akgl_vtx_maxtile *vtx=vtxv;
  int row=0; for (;row<3;row++) {
    int col=0; for (;col<3;col++,vtx++) {
      memcpy(vtx,vtxv,sizeof(struct akgl_vtx_maxtile));
      vtx->x+=col*PS_TILESIZE;
      vtx->y+=row*PS_TILESIZE;
      vtx->tileid+=row*0x10+col;
    }
  }
  
  return 9;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_explosion={
  .name="explosion",
  .objlen=sizeof(struct ps_sprite_explosion),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=200,

  .init=_ps_explosion_init,
  .update=_ps_explosion_update,
  .draw=_ps_explosion_draw,
  
};

/* Report to bomb whether I am a "fresh" explosion, ie don't play another sound effect.
 */

int ps_sprite_is_fresh_explosion(const struct ps_sprite *spr) {
  if (!spr) return 0;
  if (spr->type!=&ps_sprtype_explosion) return 0;
  if (SPR->ttl>=PS_EXPLOSION_TTL_DEFAULT-PS_EXPLOSION_FRESH_TIME) return 1;
  return 0;
}
