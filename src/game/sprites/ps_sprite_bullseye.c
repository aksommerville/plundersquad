#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_sound_effects.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"

#define PS_BULLSEYE_COLOR_ON   0x109030
#define PS_BULLSEYE_COLOR_OFF  0x800000

/* Private sprite object.
 */

struct ps_sprite_bullseye {
  struct ps_sprite hdr;
  int value;
  int finished;
  int updatecount;
};

#define SPR ((struct ps_sprite_bullseye*)spr)

/* Delete.
 */

static void _ps_bullseye_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_bullseye_init(struct ps_sprite *spr) {
  return 0;
}

/* Configure.
 */

static int _ps_bullseye_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
  }
  return 0;
}

static const char *_ps_bullseye_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
  }
  return 0;
}

/* Update.
 */

static int _ps_bullseye_update(struct ps_sprite *spr,struct ps_game *game) {
  SPR->updatecount++;
  return 0;
}

/* Draw.
 */

static int _ps_bullseye_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=spr->tileid;
  vtxv->a=0xff;
  vtxv->ta=0;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  
  if (SPR->value) {
    vtxv->pr=PS_BULLSEYE_COLOR_ON>>16;
    vtxv->pg=PS_BULLSEYE_COLOR_ON>>8;
    vtxv->pb=PS_BULLSEYE_COLOR_ON;
  } else {
    vtxv->pr=PS_BULLSEYE_COLOR_OFF>>16;
    vtxv->pg=PS_BULLSEYE_COLOR_OFF>>8;
    vtxv->pb=PS_BULLSEYE_COLOR_OFF;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_bullseye_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  if (SPR->value==value) {
    if (SPR->updatecount>1) PS_SFX_BULLSEYE_REDUNDANT
    return 0;
  }
  if (value) {
    if (SPR->updatecount>1) PS_SFX_BULLSEYE_ACTIVATE
    SPR->value=value;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_bullseye={
  .name="bullseye",
  .objlen=sizeof(struct ps_sprite_bullseye),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_bullseye_init,
  .del=_ps_bullseye_del,
  .configure=_ps_bullseye_configure,
  .get_configure_argument_name=_ps_bullseye_get_configure_argument_name,
  .update=_ps_bullseye_update,
  .draw=_ps_bullseye_draw,
  
  .set_switch=_ps_bullseye_set_switch,

};
