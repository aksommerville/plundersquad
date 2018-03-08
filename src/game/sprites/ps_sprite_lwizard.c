/* ps_sprite_lwizard.c
 * Part lizard, part wizard.
 * We take control of the renderer's "fade" option, which may conflict with other sprites.
 * It particular, it would be a problem to have multiple lwizards on screen at once.
 * You also shouldn't let them appear in one-player games, because swapping input doesn't do anything then.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_game_renderer.h"
#include "scenario/ps_grid.h"
#include "input/ps_input.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_LWIZARD_PHASE_IDLE    0
#define PS_LWIZARD_PHASE_WALK    1
#define PS_LWIZARD_PHASE_SPELL   2

#define PS_LWIZARD_IDLE_TIME_MIN  30
#define PS_LWIZARD_IDLE_TIME_MAX 120
#define PS_LWIZARD_WALK_TIME_MAX 180
#define PS_LWIZARD_SPELL_TIME    180
#define PS_LWIZARD_WALK_FRAME_COUNT 2
#define PS_LWIZARD_WALK_FRAME_TIME 7
#define PS_LWIZARD_WALK_SPEED 1.0
#define PS_LWIZARD_SPELL_FRAME_COUNT 2
#define PS_LWIZARD_SPELL_FRAME_TIME 15
#define PS_LWIZARD_SPELL_FADE_COLOR 0xffff00c0

/* Private sprite object.
 */

struct ps_sprite_lwizard {
  struct ps_sprite hdr;
  int phase;
  int phasetime;
  int animframe;
  int animtime;
  int animktime;
  int animframec;
  struct ps_game_renderer *renderer;
  double walkdstx,walkdsty;
};

#define SPR ((struct ps_sprite_lwizard*)spr)

/* Delete.
 */

static void _ps_lwizard_del(struct ps_sprite *spr) {
  ps_game_renderer_cancel_fade(SPR->renderer);
  ps_game_renderer_del(SPR->renderer);
}

/* Initialize.
 */

static int _ps_lwizard_init(struct ps_sprite *spr) {
  return 0;
}

/* Configure.
 */

static int _ps_lwizard_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_lwizard_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_lwizard_configure(), for editor.
  return 0;
}

/* IDLE phase.
 */

static int ps_lwizard_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_LWIZARD_PHASE_IDLE;
  SPR->phasetime=PS_LWIZARD_IDLE_TIME_MIN+rand()%(PS_LWIZARD_IDLE_TIME_MAX-PS_LWIZARD_IDLE_TIME_MIN);
  return 0;
}

/* WALK phase.
 */

static int ps_lwizard_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_LWIZARD_PHASE_WALK;
  SPR->phasetime=PS_LWIZARD_WALK_TIME_MAX;
  SPR->animktime=PS_LWIZARD_WALK_FRAME_TIME;
  SPR->animframec=PS_LWIZARD_WALK_FRAME_COUNT;
  SPR->animtime=0;
  SPR->animframe=0;

  /* Compose a list of valid neighbor cells. */
  if (game->grid) {
    int col=spr->x/PS_TILESIZE;
    int row=spr->y/PS_TILESIZE;
    if (col<0) col=0; else if (col>=PS_GRID_COLC) col=PS_GRID_COLC-1;
    if (row<0) row=0; else if (row>=PS_GRID_ROWC) row=PS_GRID_ROWC-1;
    struct ps_vector candidatev[8];
    int candidatec=0;
    int dx=-1; for (;dx<=1;dx++) {
      int x=col+dx;
      if ((x<0)||(x>=PS_GRID_COLC)) continue;
      int dy=-1; for (;dy<=1;dy++) {
        if (!dx&&!dy) continue;
        int y=row+dy;
        if ((y<0)||(y>=PS_GRID_ROWC)) continue;
        uint8_t physics=game->grid->cellv[y*PS_GRID_COLC+x].physics;
        if (spr->impassable&(1<<physics)) continue;
        candidatev[candidatec].dx=x;
        candidatev[candidatec].dy=y;
        candidatec++;
      }
    }
    if (candidatec) {
      int candidatep=rand()%candidatec;
      int x=candidatev[candidatep].dx;
      int y=candidatev[candidatep].dy;
      SPR->walkdstx=x*PS_TILESIZE+(PS_TILESIZE>>1);
      SPR->walkdsty=y*PS_TILESIZE+(PS_TILESIZE>>1);
      return 0;
    }
  }

  /* Didn't find a destination? Whatever, return to IDLE. */
  ps_log(GAME,DEBUG,"lwizard can't find destination.");
  return ps_lwizard_begin_IDLE(spr,game);
}

static int ps_lwizard_update_WALK(struct ps_sprite *spr,struct ps_game *game) {
  double dx=SPR->walkdstx-spr->x;
  double dy=SPR->walkdsty-spr->y;
  int xok=0,yok=0;
       if (dx<=-PS_LWIZARD_WALK_SPEED) spr->x-=PS_LWIZARD_WALK_SPEED;
  else if (dx>= PS_LWIZARD_WALK_SPEED) spr->x+=PS_LWIZARD_WALK_SPEED;
  else { xok=1; spr->x=SPR->walkdstx; }
       if (dy<=-PS_LWIZARD_WALK_SPEED) spr->y-=PS_LWIZARD_WALK_SPEED;
  else if (dy>= PS_LWIZARD_WALK_SPEED) spr->y+=PS_LWIZARD_WALK_SPEED;
  else { yok=1; spr->y=SPR->walkdsty; }
  if (xok&&yok) return ps_lwizard_begin_IDLE(spr,game);
  return 0;
}

/* SPELL phase.
 */

static int ps_lwizard_begin_SPELL(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_LWIZARD_PHASE_SPELL;
  SPR->phasetime=PS_LWIZARD_SPELL_TIME;
  SPR->animktime=PS_LWIZARD_SPELL_FRAME_TIME;
  SPR->animframec=PS_LWIZARD_SPELL_FRAME_COUNT;
  SPR->animtime=0;
  SPR->animframe=0;
  if (!SPR->renderer) {
    if (ps_game_renderer_ref(game->renderer)<0) return -1;
    SPR->renderer=game->renderer;
  }
  if (ps_game_renderer_begin_fade(SPR->renderer,PS_LWIZARD_SPELL_FADE_COLOR,PS_LWIZARD_SPELL_TIME)<0) return -1;
  return 0;
}

static int ps_lwizard_update_SPELL(struct ps_sprite *spr,struct ps_game *game) {
  return 0;
}

static int ps_lwizard_end_SPELL(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_input_swap_assignments()<0) return -1;
  if (ps_game_renderer_cancel_fade(SPR->renderer)<0) return -1;
  return ps_lwizard_begin_IDLE(spr,game);
}

/* From IDLE, select a new phase randomly.
 */

static int ps_lwizard_select_phase(struct ps_sprite *spr,struct ps_game *game) {
  int selection=rand()%20;
  if (selection<=14) return ps_lwizard_begin_WALK(spr,game);
  if (selection<=18) return ps_lwizard_begin_SPELL(spr,game);
  return ps_lwizard_begin_IDLE(spr,game);
}

/* Update.
 */

static int _ps_lwizard_update(struct ps_sprite *spr,struct ps_game *game) {

  if (--(SPR->phasetime)<=0) switch (SPR->phase) {
    case PS_LWIZARD_PHASE_IDLE: if (ps_lwizard_select_phase(spr,game)<0) return -1; break;
    case PS_LWIZARD_PHASE_WALK: if (ps_lwizard_begin_IDLE(spr,game)<0) return -1; break;
    case PS_LWIZARD_PHASE_SPELL: if (ps_lwizard_end_SPELL(spr,game)<0) return -1; break;
    default: if (ps_lwizard_begin_IDLE(spr,game)<0) return -1; break;
  }

  if (++(SPR->animtime)>=SPR->animktime) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=SPR->animframec) {
      SPR->animframe=0;
    }
  }

  switch (SPR->phase) {
    case PS_LWIZARD_PHASE_WALK: if (ps_lwizard_update_WALK(spr,game)<0) return -1; break;
    case PS_LWIZARD_PHASE_SPELL: if (ps_lwizard_update_SPELL(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Draw.
 */

static int _ps_lwizard_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  switch (SPR->phase) {
    case PS_LWIZARD_PHASE_WALK: switch (SPR->animframe) {
        case 0: vtxv->tileid+=0x01; break;
      } break;
    case PS_LWIZARD_PHASE_SPELL: switch (SPR->animframe) {
        case 0: vtxv->tileid+=0x10; break;
        case 1: vtxv->tileid+=0x11; break;
      } break;
  }
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_lwizard={
  .name="lwizard",
  .objlen=sizeof(struct ps_sprite_lwizard),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_lwizard_init,
  .del=_ps_lwizard_del,
  .configure=_ps_lwizard_configure,
  .get_configure_argument_name=_ps_lwizard_get_configure_argument_name,
  .update=_ps_lwizard_update,
  .draw=_ps_lwizard_draw,
  
};
