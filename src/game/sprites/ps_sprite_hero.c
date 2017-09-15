#include "ps.h"
#include "ps_sprite_hero.h"
#include "game/ps_sprite.h"
#include "game/ps_player.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_HERO_HEAD_OFFSET       ((PS_TILESIZE*3)/4)
#define PS_HERO_HEAD_OFFSET_SWORD ((PS_TILESIZE*2)/4) /* For south face only */
#define PS_HERO_BLINKTIME_SHUT      10
#define PS_HERO_BLINKTIME_OPEN_MIN  60
#define PS_HERO_BLINKTIME_OPEN_MAX 240
#define PS_HERO_WALK_FRAME_DURATION 10
#define PS_HERO_WALK_TOTAL_DURATION (PS_HERO_WALK_FRAME_DURATION*4)
#define PS_HERO_HURT_TIME 60
#define PS_HERO_DEFAULT_HP 3
#define PS_HERO_HEAL_TIME 60
#define PS_HERO_FLY_FRAME_TIME 10

#define SPR ((struct ps_sprite_hero*)spr)

/* Reset blink time.
 */

static void ps_hero_reset_blinktime(struct ps_sprite *spr) {
  SPR->blinktime=PS_HERO_BLINKTIME_SHUT+PS_HERO_BLINKTIME_OPEN_MIN;
  SPR->blinktime+=rand()%(PS_HERO_BLINKTIME_OPEN_MAX-PS_HERO_BLINKTIME_OPEN_MIN);
}

/* Delete.
 */

static void _ps_hero_del(struct ps_sprite *spr) {
  ps_player_del(SPR->player);
}

/* Initialize.
 */

static int _ps_hero_init(struct ps_sprite *spr) {

  SPR->input=0;

  SPR->rgba_body=0xff8000ff;
  SPR->rgba_body=rand()<<1;
  SPR->rgba_head=0x806020ff;

  SPR->facedir=PS_DIRECTION_SOUTH;

  SPR->hp=PS_HERO_DEFAULT_HP;

  ps_hero_reset_blinktime(spr);

  return 0;
}

/* Setup walk animation.
 */

static int ps_hero_begin_walking(struct ps_sprite *spr) {
  SPR->walktime=0;
  return 0;
}

static int ps_hero_end_walking(struct ps_sprite *spr) {
  return 0;
}

/* Receive D-pad input, update facedir and indx,indy.
 */

static int ps_hero_rcvinput_dpad(struct ps_sprite *spr,uint16_t input) {

  /* D-pad: Establish movement direction (indx,indy). */
  int dx,dy;
  switch (input&(PS_PLRBTN_LEFT|PS_PLRBTN_RIGHT)) {
    case PS_PLRBTN_LEFT: dx=-1; break;
    case PS_PLRBTN_RIGHT: dx=1; break;
    default: dx=0;
  }
  switch (input&(PS_PLRBTN_UP|PS_PLRBTN_DOWN)) {
    case PS_PLRBTN_UP: dy=-1; break;
    case PS_PLRBTN_DOWN: dy=1; break;
    default: dy=0;
  }
  if ((dx!=SPR->indx)||(dy!=SPR->indy)) {
    if (!SPR->indx&&!SPR->indy) {
      if (ps_hero_begin_walking(spr)<0) return -1;
    } else if (!dx&&!dy) {
      if (ps_hero_end_walking(spr)<0) return -1;
    }
    SPR->indx=dx;
    SPR->indy=dy;
  }

  /* D-pad: Apply new button presses to (facedir) */
  if ((input&PS_PLRBTN_UP)&&!(SPR->input&PS_PLRBTN_UP)) SPR->facedir=PS_DIRECTION_NORTH;
  if ((input&PS_PLRBTN_DOWN)&&!(SPR->input&PS_PLRBTN_DOWN)) SPR->facedir=PS_DIRECTION_SOUTH;
  if ((input&PS_PLRBTN_LEFT)&&!(SPR->input&PS_PLRBTN_LEFT)) SPR->facedir=PS_DIRECTION_WEST;
  if ((input&PS_PLRBTN_RIGHT)&&!(SPR->input&PS_PLRBTN_RIGHT)) SPR->facedir=PS_DIRECTION_EAST;

  /* D-pad: Finally, ensure that (facedir) is compatible with (indx,indy). */
  if (ps_direction_is_vertical(SPR->facedir)&&!SPR->indy) switch (SPR->indx) {
    case -1: SPR->facedir=PS_DIRECTION_WEST; break;
    case 1: SPR->facedir=PS_DIRECTION_EAST; break;
  } else if (ps_direction_is_horizontal(SPR->facedir)&&!SPR->indx) switch (SPR->indy) {
    case -1: SPR->facedir=PS_DIRECTION_NORTH; break;
    case 1: SPR->facedir=PS_DIRECTION_SOUTH; break;
  }
       if ((SPR->indx<0)&&(SPR->facedir==PS_DIRECTION_EAST)) SPR->facedir=PS_DIRECTION_WEST;
  else if ((SPR->indx>0)&&(SPR->facedir==PS_DIRECTION_WEST)) SPR->facedir=PS_DIRECTION_EAST;
  else if ((SPR->indy<0)&&(SPR->facedir==PS_DIRECTION_SOUTH)) SPR->facedir=PS_DIRECTION_NORTH;
  else if ((SPR->indy>0)&&(SPR->facedir==PS_DIRECTION_NORTH)) SPR->facedir=PS_DIRECTION_SOUTH;

  return 0;
}

/* Receive input.
 */

static int ps_hero_rcvinput(struct ps_sprite *spr,uint16_t input,struct ps_game *game) {
  if (SPR->input==input) return 0; // No change.

  if (SPR->hookshot_in_progress) {
    SPR->indx=0;
    SPR->indy=0;
  } else {
    if (ps_hero_rcvinput_dpad(spr,input)<0) return -1;
  }

  /* Buttons. */
  if ((input&PS_PLRBTN_A)&&!(SPR->input&PS_PLRBTN_A)) {
    if (ps_hero_action_begin(spr,game)<0) return -1;
  } else if (!(input&PS_PLRBTN_A)&&(SPR->input&PS_PLRBTN_A)) {
    if (ps_hero_action_end(spr,game)<0) return -1;
  }
  if ((input&PS_PLRBTN_B)&&!(SPR->input&PS_PLRBTN_B)) {
    if (ps_hero_auxaction_begin(spr,game)<0) return -1;
  } else if (!(input&PS_PLRBTN_B)&&(SPR->input&PS_PLRBTN_B)) {
    if (ps_hero_auxaction_end(spr,game)<0) return -1;
  }
  if ((input&PS_PLRBTN_PAUSE)&&!(SPR->input&PS_PLRBTN_PAUSE)) {
    if (ps_game_toggle_pause(game)<0) return -1;
  }

  SPR->input=input;
  if (SPR->reexamine_dpad) {
    SPR->input&=~(PS_PLRBTN_LEFT|PS_PLRBTN_RIGHT|PS_PLRBTN_UP|PS_PLRBTN_DOWN);
    SPR->reexamine_dpad=0;
  }
  return 0;
}

/* Update animation.
 */

static int ps_hero_animate(struct ps_sprite *spr) {

  /* Blink. */
  if (--(SPR->blinktime)<=0) {
    ps_hero_reset_blinktime(spr);
  }

  /* Walk. */
  if (SPR->indx||SPR->indy) {
    SPR->walktime++;
    if (SPR->walktime>=PS_HERO_WALK_TOTAL_DURATION) SPR->walktime=0;
  }

  /* Hurt timer. */
  if (SPR->hurttime>0) {
    SPR->hurttime--;
  }

  /* Heal timer. */
  if (SPR->healtime>0) {
    SPR->healtime--;
  }

  /* Flames. */
  if (SPR->flame_in_progress) {
    SPR->flame_counter++;
  }

  return 0;
}

/* Walking.
 */

static int ps_hero_walk_inhibited_by_actions(struct ps_sprite *spr) {
  if (SPR->sword_in_progress) return 1;
  if (SPR->hookshot_in_progress) return 1;
  return 0;
}

static int ps_hero_walk(struct ps_sprite *spr,struct ps_game *game) {

  if (!SPR->indx&&!SPR->indy) {
    SPR->walk_in_progress=0;
    return 0;
  }
  if (ps_hero_walk_inhibited_by_actions(spr)) {
    SPR->walk_in_progress=0;
    return 0;
  }

  SPR->walk_in_progress=1;
  spr->x+=SPR->indx;
  spr->y+=SPR->indy;
  
  return 0;
}

/* Look at the grid cell we are standing on (just one) and take appropriate action.
 */

static int ps_hero_check_grid(struct ps_sprite *spr,struct ps_game *game) {
  if (!game->grid) return 0;
  if (spr->x<0.0) return 0;
  if (spr->y<0.0) return 0;
  int col=(int)spr->x/PS_TILESIZE;
  int row=(int)spr->y/PS_TILESIZE;
  if (col>=PS_GRID_COLC) return 0;
  if (row>=PS_GRID_ROWC) return 0;
  uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;

  switch (physics) {
    case PS_BLUEPRINT_CELL_HEAL: {
        if (ps_hero_heal(spr,game)<0) return -1;
      } break;
    case PS_BLUEPRINT_CELL_HOLE: {
        // Landing on a hole is unusal, but can easily be done with a hookshot.
        if (spr->collide_hole) {
          return ps_hero_become_ghost(game,spr);
        }
      } break;
  }
  return 0;
}

/* Update.
 */

static int _ps_hero_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Receive input. */
  if (SPR->player) {
    if (ps_hero_rcvinput(spr,ps_get_player_buttons(SPR->player->playerid),game)<0) return -1;
  } else {
    if (ps_hero_rcvinput(spr,0,game)<0) return -1;
  }

  /* Continue running actions. */
  if (SPR->input&PS_PLRBTN_A) {
    if (ps_hero_action_continue(spr,game)<0) return -1;
  }
  if (SPR->input&PS_PLRBTN_B) {
    if (ps_hero_auxaction_continue(spr,game)<0) return -1;
  }

  /* Update animation. */
  if (ps_hero_animate(spr)<0) return -1;

  /* Update movement. */
  if (ps_hero_walk(spr,game)<0) return -1;

  /* Take actions based on grid cell. */
  if (ps_hero_check_grid(spr,game)<0) return -1;

  return 0;
}

/* Draw ghost.
 */

static int ps_hero_draw_ghost(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=0x5f;
  vtxv->ta=0;
  vtxv->pr=SPR->rgba_body>>24;
  vtxv->pg=SPR->rgba_body>>16;
  vtxv->pb=SPR->rgba_body>>8;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  return 1;
}

static int ps_hero_draw_fly(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=SPR->rgba_body>>24;
  vtxv->pg=SPR->rgba_body>>16;
  vtxv->pb=SPR->rgba_body>>8;
  vtxv->a=0xff;
  vtxv->xform=AKGL_XFORM_NONE;

  /* Tile. */
  if ((SPR->fly_counter/PS_HERO_FLY_FRAME_TIME)&1) {
    vtxv->tileid=0xcf;
  } else {
    vtxv->tileid=0xbf;
  }

  /* Tint. */
  vtxv->t=0;
  if (SPR->healtime>0) {
    vtxv->tr=0x00;
    vtxv->tg=0xff;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->healtime*0xff)/PS_HERO_HEAL_TIME;
  } else if (SPR->hurttime>0) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->hurttime*0xff)/PS_HERO_HURT_TIME;
  }
  
  return 1;
}

/* Draw flames.
 */

static void ps_hero_draw_flames(struct ps_sprite *spr,struct akgl_vtx_maxtile *vtxv) {

  double distance;
  if (SPR->flame_counter<PS_HERO_FLAMES_RAMP_UP_TIME) {
    distance=(double)(SPR->flame_counter*PS_HERO_FLAMES_ORBIT_DISTANCE)/(double)PS_HERO_FLAMES_RAMP_UP_TIME;
  } else {
    distance=PS_HERO_FLAMES_ORBIT_DISTANCE;
  }

  double angle=(SPR->flame_counter*M_PI*2.0)/PS_HERO_FLAMES_ORBIT_TIME;
  double dx=cos(angle)*distance;
  double dy=-sin(angle)*distance;
  vtxv->x=spr->x+dx;
  vtxv->y=spr->y+dy;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=0x6f;
  vtxv->pr=0x80;
  vtxv->pg=0x80;
  vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->ta=0;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  int i; for (i=1;i<4;i++) {
    memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtxv[i].tileid+=i*0x10;
  }

  vtxv[1].x=spr->x+dy;
  vtxv[1].y=spr->y-dx;
  vtxv[2].x=spr->x-dx;
  vtxv[2].y=spr->y-dy;
  vtxv[3].x=spr->x-dy;
  vtxv[3].y=spr->y+dx;
}

/* Draw.
 */

static int _ps_hero_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  /* Ghosts and bats are a whole different ballgame, so treat them separately. */
  if (!SPR->hp) return ps_hero_draw_ghost(vtxv,vtxa,spr);
  if (SPR->fly_in_progress) return ps_hero_draw_fly(vtxv,vtxa,spr);

  /* Required vertex count depends on the actions in flight.
   */
  int vtxc=2;
  if (SPR->sword_in_progress) vtxc=3;
  if (SPR->flame_in_progress) vtxc+=4;
  if (vtxa<vtxc) return vtxc;
  
  /* Enumerate vertices. */
  struct akgl_vtx_maxtile *vtx_body=vtxv+0;
  struct akgl_vtx_maxtile *vtx_head=vtxv+1;
  struct akgl_vtx_maxtile *vtx_sword=0;
  if (SPR->sword_in_progress) {
    if (SPR->facedir==PS_DIRECTION_NORTH) {
      vtx_sword=vtxv+0;
      vtx_head=vtxv+1;
      vtx_body=vtxv+2;
    } else {
      vtx_sword=vtxv+2;
    }
  } else if (SPR->facedir==PS_DIRECTION_NORTH) {
    vtx_head=vtxv+0;
    vtx_body=vtxv+1;
  }

  /* Position vertices. Body is the sprite's (x,y), head is a little north. */
  vtx_body->x=spr->x;
  vtx_body->y=spr->y;
  vtx_head->x=vtx_body->x;
  vtx_head->y=vtx_body->y-PS_HERO_HEAD_OFFSET;

  /* Size, alpha, and rotation are constant. */
  vtx_body->size=PS_TILESIZE;
  vtx_head->size=PS_TILESIZE;
  vtx_body->a=0xff;
  vtx_head->a=0xff;
  vtx_body->t=0;
  vtx_head->t=0;

  /* Principal colors. */
  vtx_body->pr=SPR->rgba_body>>24;
  vtx_body->pg=SPR->rgba_body>>16;
  vtx_body->pb=SPR->rgba_body>>8;
  vtx_head->pr=SPR->rgba_head>>24;
  vtx_head->pg=SPR->rgba_head>>16;
  vtx_head->pb=SPR->rgba_head>>8;

  /* Tint. */
  vtx_body->ta=0;
  vtx_head->ta=0;
  if (SPR->healtime>0) {
    vtx_body->tr=vtx_head->tr=0x00;
    vtx_body->tg=vtx_head->tg=0xff;
    vtx_body->tb=vtx_head->tb=0x00;
    vtx_body->ta=vtx_head->ta=(SPR->healtime*0xff)/PS_HERO_HEAL_TIME;
  } else if (SPR->hurttime>0) {
    vtx_body->tr=vtx_head->tr=0xff;
    vtx_body->tg=vtx_head->tg=0x00;
    vtx_body->tb=vtx_head->tb=0x00;
    vtx_body->ta=vtx_head->ta=(SPR->hurttime*0xff)/PS_HERO_HURT_TIME;
  }

  /* Set undirectioned vertex tiles. (spr->tileid) is the head's base. Body base is 0x80 beyond. */
  vtx_head->tileid=spr->tileid;
  vtx_body->tileid=spr->tileid+0x80;

  /* Select variant for head. */
  if (SPR->blinktime<=PS_HERO_BLINKTIME_SHUT) vtx_head->tileid+=0x10;

  /* Select variant for body. */
  if (SPR->walk_in_progress) switch (SPR->walktime/PS_HERO_WALK_FRAME_DURATION) {
    case 0: vtx_body->tileid+=0x10; break;
    case 1: break;
    case 2: vtx_body->tileid+=0x20; break;
    case 3: break;
  } else if (SPR->sword_in_progress||SPR->hookshot_in_progress) {
    vtx_body->tileid+=0x30;
    if (SPR->facedir==PS_DIRECTION_SOUTH) {
      vtx_head->y=vtx_body->y-PS_HERO_HEAD_OFFSET_SWORD;
    }
  }

  /* Initialize sword if present.
   * Position against the body; we'll adjust later.
   */
  if (vtx_sword) {
    memcpy(vtx_sword,vtx_body,sizeof(struct akgl_vtx_maxtile));
    vtx_sword->tileid=0x0f;
    vtx_sword->ta=0x00;
  }

  /* Apply direction to vertices.
   * Tiles are arranged horizontally: south, north, west.
   * East is just west with AKGL_XFORM_FLOP.
   */
  switch (SPR->facedir) {
  
    case PS_DIRECTION_SOUTH: {
        vtx_head->xform=AKGL_XFORM_NONE;
        vtx_body->xform=AKGL_XFORM_NONE;
        if (vtx_sword) {
          vtx_sword->xform=AKGL_XFORM_NONE;
          vtx_sword->y+=PS_TILESIZE;
        }
      } break;
      
    case PS_DIRECTION_NORTH: {
        vtx_head->xform=AKGL_XFORM_NONE;
        vtx_body->xform=AKGL_XFORM_NONE;
        vtx_head->tileid+=1;
        vtx_body->tileid+=1;
        if (vtx_sword) {
          vtx_sword->xform=AKGL_XFORM_FLIP;
          vtx_sword->y-=PS_TILESIZE;
        }
      } break;
      
    case PS_DIRECTION_WEST: {
        vtx_head->xform=AKGL_XFORM_NONE;
        vtx_body->xform=AKGL_XFORM_NONE;
        vtx_head->tileid+=2;
        vtx_body->tileid+=2;
        if (vtx_sword) {
          vtx_sword->xform=AKGL_XFORM_90;
          vtx_sword->x-=PS_TILESIZE;
        }
      } break;
      
    case PS_DIRECTION_EAST: {
        vtx_head->xform=AKGL_XFORM_FLOP;
        vtx_body->xform=AKGL_XFORM_FLOP;
        vtx_head->tileid+=2;
        vtx_body->tileid+=2;
        if (vtx_sword) {
          vtx_sword->xform=AKGL_XFORM_270;
          vtx_sword->x+=PS_TILESIZE;
        }
      } break;
  }

  /* Make flame sprites. */
  if (SPR->flame_in_progress) {
    ps_hero_draw_flames(spr,vtxv+vtxc-4);
  }
  
  return vtxc;
}

/* Become ghost.
 */

int ps_hero_become_ghost(struct ps_game *game,struct ps_sprite *spr) {
  if (!game||!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  //ps_log(GAME,INFO,"Hero %p becomes ghost.",spr);

  if (ps_hero_action_end(spr,game)<0) return -1;
  if (ps_hero_auxaction_end(spr,game)<0) return -1;

  SPR->hp=0;
  spr->collide_sprites=0;
  spr->collide_hole=0;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,spr)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_LATCH,spr)<0) return -1;

  return 0;
}

/* Resurrect.
 */

int ps_hero_become_living(struct ps_game *game,struct ps_sprite *spr) {
  if (!game||!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  //ps_log(GAME,INFO,"Hero %p resurrects.",spr);
  
  SPR->healtime=PS_HERO_HEAL_TIME;
  SPR->hp=PS_HERO_DEFAULT_HP;
  spr->collide_sprites=1;
  spr->collide_hole=1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_LATCH,spr)<0) return -1;

  return 0;
}

int ps_hero_heal(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (!game) return -1;
  if (SPR->hp==PS_HERO_DEFAULT_HP) return 0;
  SPR->healtime=PS_HERO_HEAL_TIME;
  if (SPR->hp) {
    SPR->hp=PS_HERO_DEFAULT_HP;
  } else {
    return ps_hero_become_living(game,spr);
  }
  return 0;
}

/* Receive damage.
 */

static int _ps_hero_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->hurttime) return 0;
  if (SPR->healtime) return 0;
  if (!SPR->hp) return 0; // Ghost.

  SPR->hp--;
  if (SPR->hp<=0) {
    SPR->hp=0;
    return ps_hero_become_ghost(game,spr);
  }

  SPR->hurttime=PS_HERO_HURT_TIME;
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_hero={
  .name="hero",
  .objlen=sizeof(struct ps_sprite_hero),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_hero_init,
  .del=_ps_hero_del,
  .update=_ps_hero_update,
  .draw=_ps_hero_draw,

  .hurt=_ps_hero_hurt,

};

/* Attach player.
 */
 
int ps_hero_set_player(struct ps_sprite *spr,struct ps_player *player) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (player==SPR->player) return 0;
  if (player&&(ps_player_ref(player)<0)) return -1;
  ps_player_del(SPR->player);
  SPR->player=player;
  return 0;
}
