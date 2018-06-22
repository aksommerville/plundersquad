#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "akgl/akgl.h"

/* Private sprite object.
 */

struct ps_sprite_switch {
  struct ps_sprite hdr;
  int state; // Final answer: Am I on or off?
  int stompbox; // Permanent role: Am I a stompbox or a treadle plate?
  int press; // Is someone standing on me right now?
  int updatecount;
};

#define SPR ((struct ps_sprite_switch*)spr)

/* Configure.
 */

static int _ps_switch_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      SPR->stompbox=argv[1];
    }
  }
  return 0;
}

static const char *_ps_switch_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "Stompbox?";
  }
  return 0;
}

/* Poll for desired state.
 */

static int ps_switch_poll_state(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite **presser) {
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_PHYSICS].sprc;i++) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_PHYSICS].sprv[i];
    if (!(hero->impassable&(1<<PS_BLUEPRINT_CELL_HOLE))) continue; // Feet not touching ground.
    if (hero->x<=left) continue;
    if (hero->x>=right) continue;
    if (hero->y<=top) continue;
    if (hero->y>=bottom) continue;
    *presser=hero;
    return 1;
  }
  return 0;
}

/* Toggle.
 */

static int ps_switch_engage(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *presser) {
  if (SPR->state) return 0;
  SPR->state=1;
  if (ps_game_set_switch(game,spr->switchid,1)<0) return -1;
  if (ps_game_report_switch(game,presser)<0) return -1;
  return 0;
}

static int ps_switch_disengage(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->state) return 0;
  SPR->state=0;
  if (ps_game_set_switch(game,spr->switchid,0)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_switch_update(struct ps_sprite *spr,struct ps_game *game) {

  SPR->updatecount++;

  /* Terminate if weight upon me hasn't changed. */
  struct ps_sprite *presser=0;
  int state=ps_switch_poll_state(spr,game,&presser);
  if (state==SPR->press) return 0;

  /* Newly pressed. */
  if (SPR->press=state) {
    if (SPR->updatecount>1) PS_SFX_SWITCH_PRESS
    spr->tileid++;
    if (SPR->stompbox) {
      if (SPR->state) {
        if (ps_switch_disengage(spr,game)<0) return -1;
      } else {
        if (ps_switch_engage(spr,game,presser)<0) return -1;
      }
    } else {
      if (ps_switch_engage(spr,game,presser)<0) return -1;
    }

  /* Newly released. */
  } else {
    if (SPR->updatecount>1) PS_SFX_SWITCH_RELEASE
    spr->tileid--;
    if (!SPR->stompbox) {
      if (ps_switch_disengage(spr,game)<0) return -1;
    }
  }
  
  return 0;
}

/* Draw.
 * 704924 off
 * 03f948 on
 */

static int _ps_switch_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].tileid=spr->tileid;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=AKGL_XFORM_NONE;

  if (SPR->state) {
    vtxv[0].pr=0x03; vtxv[0].pg=0xf9; vtxv[0].pb=0x48;
  } else {
    vtxv[0].pr=0x70; vtxv[0].pg=0x49; vtxv[0].pb=0x24;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_switch_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  if (SPR->stompbox) {
    SPR->state=value;
  } else {
    // Treadle plates can't be actuated this way.
    // If we did, they would notice on the next update that no one is standing here, and turn right off again.
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_switch={
  .name="switch",
  .objlen=sizeof(struct ps_sprite_switch),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .configure=_ps_switch_configure,
  .get_configure_argument_name=_ps_switch_get_configure_argument_name,
  .update=_ps_switch_update,
  .draw=_ps_switch_draw,
  .set_switch=_ps_switch_set_switch,

};
