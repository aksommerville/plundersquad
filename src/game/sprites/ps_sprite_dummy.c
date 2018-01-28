#include "ps.h"
#include "game/ps_sprite.h"

/* Private sprite object.
 */

struct ps_sprite_dummy {
  struct ps_sprite hdr;
};

#define SPR ((struct ps_sprite_dummy*)spr)

/* Delete.
 */

static void _ps_dummy_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_dummy_init(struct ps_sprite *spr) {
  return 0;
}

/* Configure.
 */

static int _ps_dummy_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_dummy_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_dummy_configure(), for editor.
  return 0;
}

/* Update.
 */

static int _ps_dummy_update(struct ps_sprite *spr,struct ps_game *game) {
  return 0;
}

/* Draw.
 */

static int _ps_dummy_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  return 0;
}

/* Hurt.
 */

static int _ps_dummy_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return 0;
}

/* Set switch.
 */

static int _ps_dummy_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_dummy={
  .name="dummy",
  .objlen=sizeof(struct ps_sprite_dummy),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_dummy_init,
  .del=_ps_dummy_del,
  .configure=_ps_dummy_configure,
  .get_configure_argument_name=_ps_dummy_get_configure_argument_name,
  .update=_ps_dummy_update,
  //.draw=_ps_dummy_draw,
  
  //.hurt=_ps_dummy_hurt,
  //.set_switch=_ps_dummy_set_switch,

};
