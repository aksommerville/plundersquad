/* ps_sprite_teleffect.c
 * Visual effect connecting two active teleporters.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_TELEFFECT_TTL 20
#define PS_TELEFFECT_FADE_TIME 10
#define PS_TELEFFECT_VERTEX_SPACING (PS_TILESIZE>>1)
#define PS_TELEFFECT_SIZE_MIN (PS_TILESIZE>>1)
#define PS_TELEFFECT_SIZE_MAX (PS_TILESIZE<<1)
#define PS_TELEFFECT_SPRDEFID 44

/* Private sprite object.
 */

struct ps_sprite_teleffect {
  struct ps_sprite hdr;
  double srcx,srcy,dstx,dsty;
  int lcp,lcc; // lifecycle
  int vtxc;
};

#define SPR ((struct ps_sprite_teleffect*)spr)

/* Initialize.
 */

static int _ps_teleffect_init(struct ps_sprite *spr) {

  SPR->lcp=0;
  SPR->lcc=PS_TELEFFECT_TTL;

  SPR->srcx=0.0;
  SPR->srcy=0.0;
  SPR->dstx=1.0;
  SPR->dsty=1.0;

  return 0;
}

/* Update.
 */

static int _ps_teleffect_update(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->lcp<0) return ps_sprite_kill_later(spr,game);
  if (++(SPR->lcp)>=SPR->lcc) return ps_sprite_kill_later(spr,game);
  return 0;
}

/* Select color for single vertex.
 */

static void ps_teleffect_select_color(uint8_t *r,uint8_t *g,uint8_t *b,int linep,int linec,int timep,int timec) {
  *r=(linep*255)/linec;
  *g=(timep*255)/timec;
  *b=0;
}

/* Draw.
 */

static int _ps_teleffect_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  /* Calculate vertex count. */
  int vtxc=SPR->vtxc;
  if (vtxc<1) vtxc=1;
  if (vtxc>vtxa) return vtxc;

  /* Set basic vertex properties. */
  vtxv->x=SPR->srcx;
  vtxv->y=SPR->srcy;
  vtxv->tileid=spr->tileid;
  vtxv->ta=0;
  vtxv->a=0xff;
  vtxv->t=(SPR->lcp*256)/SPR->lcc;
  vtxv->xform=AKGL_XFORM_NONE;

  /* Fade out if necessary. */
  if (SPR->lcp>SPR->lcc-PS_TELEFFECT_FADE_TIME) {
    int p=SPR->lcc-SPR->lcp;
    vtxv->a=(p*255)/PS_TELEFFECT_FADE_TIME;
  }

  /* Copy to all vertices. */
  int i=vtxc; while (i-->1) memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));

  /* Set position of each vertex, spacing evenly. */
  struct akgl_vtx_maxtile *vtx=vtxv;
  int vtxi=0; for (;vtxi<vtxc;vtxi++,vtx++) {
    vtx->x+=((SPR->dstx-SPR->srcx)*vtxi)/vtxc;
    vtx->y+=((SPR->dsty-SPR->srcy)*vtxi)/vtxc;
    vtx->size=PS_TELEFFECT_SIZE_MIN+(vtxi*(PS_TELEFFECT_SIZE_MAX-PS_TELEFFECT_SIZE_MIN))/vtxc;
    ps_teleffect_select_color(&vtx->pr,&vtx->pg,&vtx->pb,vtxi,vtxc,SPR->lcp,SPR->lcc);
  }
  
  return vtxc;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_teleffect={
  .name="teleffect",
  .objlen=sizeof(struct ps_sprite_teleffect),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_teleffect_init,
  .update=_ps_teleffect_update,
  .draw=_ps_teleffect_draw,
  
};

/* General initializer (public but not declared in headers).
 */

struct ps_sprite *ps_teleffect_new(struct ps_game *game,double srcx,double srcy,double dstx,double dsty) {

  struct ps_sprite *spr=ps_sprdef_instantiate(game,ps_res_get(PS_RESTYPE_SPRDEF,PS_TELEFFECT_SPRDEFID),0,0,srcx,srcy);
  if (!spr) return 0;

  /* Assign endpoints and ensure they are different. */
  SPR->srcx=srcx;
  SPR->srcy=srcy;
  SPR->dstx=dstx;
  SPR->dsty=dsty;
  double dx=dstx-srcx,dy=dsty-srcy;
  if ((dx>=-1.0)&&(dx<=1.0)) SPR->dstx+=1.0;
  if ((dy>=-1.0)&&(dy<=1.0)) SPR->dsty+=1.0;

  /* Calculate vertex count. */
  int distance=sqrt(dx*dx+dy*dy);
  SPR->vtxc=(distance/PS_TELEFFECT_VERTEX_SPACING)+1;
  
  return spr;
}
