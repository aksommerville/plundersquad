/* ps_sprite_toast.c
 * Runs through a course of animation once, then kills self.
 * Tiles must be sequential.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "akgl/akgl.h"

/* Private sprite object.
 */

struct ps_sprite_toast {
  struct ps_sprite hdr;
  int framep,framec;
  int framecounter;
  int frametime;
};

#define SPR ((struct ps_sprite_toast*)spr)

/* Delete.
 */

static void _ps_toast_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_toast_init(struct ps_sprite *spr) {
  SPR->framec=1;
  SPR->frametime=10;
  return 0;
}

/* Configure.
 */

static int _ps_toast_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  SPR->framec=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_framec,SPR->framec);
  SPR->frametime=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_frametime,SPR->frametime);
  return 0;
}

static const char *_ps_toast_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_toast_configure(), for editor.
  return 0;
}

/* Update.
 */

static int _ps_toast_update(struct ps_sprite *spr,struct ps_game *game) {
  if (++(SPR->framecounter)>=SPR->frametime) {
    SPR->framecounter=0;
    if (++(SPR->framep)>=SPR->framec) {
      SPR->framep=SPR->framec-1;
      return ps_sprite_kill_later(spr,game);
    }
  }
  return 0;
}

/* Draw.
 */

static int _ps_toast_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid+SPR->framep;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->xform=AKGL_XFORM_NONE;
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_toast={
  .name="toast",
  .objlen=sizeof(struct ps_sprite_toast),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_toast_init,
  .del=_ps_toast_del,
  .configure=_ps_toast_configure,
  .get_configure_argument_name=_ps_toast_get_configure_argument_name,
  .update=_ps_toast_update,
  .draw=_ps_toast_draw,
  
};
