#include "ps.h"
#include "ps_sprite_hero.h"
#include "game/ps_sprite.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_game.h"
#include "game/ps_stats.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "akgl/akgl.h"
#include <math.h>

int ps_hero_fly_end(struct ps_sprite *spr,struct ps_game *game);

#define PS_HERO_HEAD_OFFSET       ((PS_TILESIZE*3)/4)
#define PS_HERO_HEAD_OFFSET_SWORD ((PS_TILESIZE*2)/4) /* For south face only */
#define PS_HERO_BLINKTIME_SHUT      10
#define PS_HERO_BLINKTIME_OPEN_MIN  60
#define PS_HERO_BLINKTIME_OPEN_MAX 240
#define PS_HERO_WALK_FRAME_DURATION 10
#define PS_HERO_WALK_TOTAL_DURATION (PS_HERO_WALK_FRAME_DURATION*4)
#define PS_HERO_FLY_FRAME_TIME 10

#define PS_HERO_IMPASSABLE_DEFAULT ( \
  (1<<PS_BLUEPRINT_CELL_SOLID)| \
  (1<<PS_BLUEPRINT_CELL_LATCH)| \
  (1<<PS_BLUEPRINT_CELL_HOLE)| \
0)

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

  SPR->facedir=PS_DIRECTION_SOUTH;

  SPR->hp=PS_HERO_DEFAULT_HP;

  ps_hero_reset_blinktime(spr);

  return 0;
}

/* Configure.
 */

static int _ps_hero_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
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
  //ps_log(GAME,DEBUG,"%s %d",__func__,SPR->player->playerid);

  if (!input) {
    if (ps_hero_remove_state(spr,PS_HERO_STATE_STOPINPUT,game)<0) return -1;
  } else if (SPR->state&PS_HERO_STATE_STOPINPUT) {
    return 0;
  }
  
  if (SPR->reexamine_dpad) {
    SPR->input&=~(PS_PLRBTN_LEFT|PS_PLRBTN_RIGHT|PS_PLRBTN_UP|PS_PLRBTN_DOWN);
    SPR->reexamine_dpad=0;
  }
  if (SPR->input==input) return 0; // No change.

  /* If offscreen, the only thing we can do is go back onscreen. 
   * UPDATE: We also permit starting auxaction, when b-to-swap is enabled.
   */
  if (SPR->state&PS_HERO_STATE_OFFSCREEN) {
    if (spr->x<=0.0) {
      if (input&PS_PLRBTN_RIGHT) return ps_hero_remove_state(spr,PS_HERO_STATE_OFFSCREEN,game);
    } else if (spr->y<=0.0) {
      if (input&PS_PLRBTN_DOWN) return ps_hero_remove_state(spr,PS_HERO_STATE_OFFSCREEN,game);
    } else if (spr->x>=PS_SCREENW) {
      if (input&PS_PLRBTN_LEFT) return ps_hero_remove_state(spr,PS_HERO_STATE_OFFSCREEN,game);
    } else if (spr->y>=PS_SCREENH) {
      if (input&PS_PLRBTN_UP) return ps_hero_remove_state(spr,PS_HERO_STATE_OFFSCREEN,game);
    } else {
      // If none of those cases is true, he's not actually offscreen.
      // It's hard (but possible) to make this happen without cheating, so we have just this quick and dirty fix.
      ps_log(GAME,WARN,
        "Player %d was marked offscreen at position (%.0f,%.0f), forcing onscreen.",
        SPR->player->playerid,spr->x,spr->y
      );
      return ps_hero_remove_state(spr,PS_HERO_STATE_OFFSCREEN,game);
    }
    if (PS_B_TO_SWAP_INPUT) {
      if ((input&PS_PLRBTN_B)&&!(SPR->input&PS_PLRBTN_B)) {
        if (ps_hero_auxaction_begin(spr,game)<0) return -1;
      }
    }
    SPR->input=input;
    return 0;
  }

  if (SPR->state&PS_HERO_STATE_HOOKSHOT) { //TODO generalize with a state query
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

  /* Dragon charger. */
  if (SPR->dragoncharge>0) {
    SPR->dragoncharge--;
  }

  return 0;
}

/* Walking.
 */

static int ps_hero_walk_inhibited_by_actions(struct ps_sprite *spr) {//TODO generalize with state query
  //if (SPR->sword_in_progress) return 1;
  //if (SPR->hookshot_in_progress) return 1;
  if (SPR->state&(PS_HERO_STATE_SWORD|PS_HERO_STATE_HOOKSHOT)) return 1;
  return 0;
}

static int ps_hero_walk(struct ps_sprite *spr,struct ps_game *game) {


  if (!SPR->indx&&!SPR->indy) {
    if (ps_hero_remove_state(spr,PS_HERO_STATE_WALK,game)<0) return -1;
    return 0;
  }
  if (ps_hero_walk_inhibited_by_actions(spr)) {
    if (ps_hero_remove_state(spr,PS_HERO_STATE_WALK,game)<0) return -1;
    return 0;
  }

  if (ps_hero_add_state(spr,PS_HERO_STATE_WALK,game)<0) return -1;
  spr->x+=SPR->indx;
  spr->y+=SPR->indy;

  
  return 0;
}

/* Look at the grid cell we are standing on (just one) and take appropriate action.
 */

static int ps_hero_check_grid(struct ps_sprite *spr,struct ps_game *game) {

  if ((spr->x<0.0)||(spr->y<0.0)||(spr->x>PS_SCREENW)||(spr->y>PS_SCREENH)) {
    return ps_hero_add_state(spr,PS_HERO_STATE_OFFSCREEN,game);
  }

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
        if (ps_hero_add_state(spr,PS_HERO_STATE_HEAL,game)<0) return -1;
      } break;
    case PS_BLUEPRINT_CELL_HOLE: {
        // Landing on a hole is unusal, but can easily be done with a hookshot.
        if (spr->impassable&(1<<PS_BLUEPRINT_CELL_HOLE)) {
          return ps_hero_force_kill(spr,game);
        }
      } break;
  }

  if (SPR->defer_fly_end) {
    if (physics!=PS_BLUEPRINT_CELL_HOLE) {
      if (ps_hero_remove_state(spr,PS_HERO_STATE_FLY,game)<0) return -1;
    } else {
      SPR->fly_counter++;
    }
  }
  
  return 0;
}

/* Update.
 */

static int _ps_hero_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Update counters. */
  if (SPR->hurttime>0) {
    if (!--(SPR->hurttime)) {
      if (ps_hero_remove_state(spr,PS_HERO_STATE_HURT,game)<0) return -1;
    }
  }
  if (SPR->healtime>0) {
    if (!--(SPR->healtime)) {
      if (ps_hero_remove_state(spr,PS_HERO_STATE_HEAL,game)<0) return -1;
    }
  }
  if (SPR->highlighttime>0) {
    if (!--(SPR->highlighttime)) {
      if (ps_hero_remove_state(spr,PS_HERO_STATE_HIGHLIGHT,game)<0) return -1;
    }
  }

  /* Receive input. */
  if (SPR->ignore_input>0) {
    // pass
  } else if (SPR->player) {
    if (ps_hero_rcvinput(spr,ps_get_player_buttons(SPR->player->playerid),game)<0) return -1;
  } else {
    if (ps_hero_rcvinput(spr,0,game)<0) return -1;
  }

  /* If we are offscreen, do nothing more. */
  if (SPR->state&PS_HERO_STATE_OFFSCREEN) return 0;

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

/* Accept fake input.
 */
 
int ps_hero_accept_fake_input(struct ps_sprite *spr,uint16_t input,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (ps_hero_rcvinput(spr,input,game)<0) return -1;
  SPR->ignore_input=1;
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
  
  uint32_t rgba=0x808080ff;
  if (SPR->player&&SPR->player->plrdef) {
    rgba=SPR->player->plrdef->palettev[SPR->player->palette].rgba_body;
  }
  vtxv->pr=rgba>>24;
  vtxv->pg=rgba>>16;
  vtxv->pb=rgba>>8;

  if (SPR->dragoncharge) {
    vtxv->tr=0xff;
    vtxv->tg=0xff;
    vtxv->tb=0xff;
    if (SPR->dragoncharge<0x30) vtxv->ta=0x30;
    else vtxv->ta=SPR->dragoncharge;
  }

  if (SPR->state&PS_HERO_STATE_HIGHLIGHT) {
    vtxv->tr=0xff;
    vtxv->tg=0xff;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->highlighttime*0xff)/PS_HERO_HIGHLIGHT_TIME;
  }
  
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
  
  uint32_t rgba=0x808080ff;
  if (SPR->player&&SPR->player->plrdef) {
    rgba=SPR->player->plrdef->palettev[SPR->player->palette].rgba_body;
  }
  vtxv->pr=rgba>>24;
  vtxv->pg=rgba>>16;
  vtxv->pb=rgba>>8;
  
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
  if (SPR->state&PS_HERO_STATE_HEAL) {
    vtxv->tr=0x00;
    vtxv->tg=0xff;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->healtime*0xff)/PS_HERO_HEAL_TIME;
  } else if (SPR->state&PS_HERO_STATE_HURT) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->hurttime*0xff)/PS_HERO_HURT_TIME;
  } else if (SPR->state&PS_HERO_STATE_HIGHLIGHT) {
    vtxv->tr=0xff;
    vtxv->tg=0xff;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->highlighttime*0xff)/PS_HERO_HIGHLIGHT_TIME;
  }
  
  return 1;
}

/* Draw.
 */

static int _ps_hero_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  /* Ghosts and bats are a whole different ballgame, so treat them separately. */
  if (SPR->state&PS_HERO_STATE_GHOST) return ps_hero_draw_ghost(vtxv,vtxa,spr);
  if (SPR->state&PS_HERO_STATE_FLY) return ps_hero_draw_fly(vtxv,vtxa,spr);

  /* For normal rendering, an attached player is required. */
  if (!SPR->player||!SPR->player->plrdef) return 0;

  /* Required vertex count depends on the actions in flight.
   */
  int vtxc=2;
  if (SPR->state&PS_HERO_STATE_SWORD) vtxc=3;
  if (vtxa<vtxc) return vtxc;
  
  /* Enumerate vertices. */
  struct akgl_vtx_maxtile *vtx_body=vtxv+0;
  struct akgl_vtx_maxtile *vtx_head=vtxv+1;
  struct akgl_vtx_maxtile *vtx_sword=0;
  if (SPR->state&PS_HERO_STATE_SWORD) {
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
  // Extra: If (head_on_top_always) and not so, swap body and head.
  if (SPR->player&&SPR->player->plrdef&&SPR->player->plrdef->head_on_top_always) {
    if (vtx_head<vtx_body) {
      void *tmp=vtx_body;
      vtx_body=vtx_head;
      vtx_head=tmp;
    }
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
  uint32_t rgba_head=SPR->player->plrdef->palettev[SPR->player->palette].rgba_head;
  uint32_t rgba_body=SPR->player->plrdef->palettev[SPR->player->palette].rgba_body;
  vtx_body->pr=rgba_body>>24;
  vtx_body->pg=rgba_body>>16;
  vtx_body->pb=rgba_body>>8;
  vtx_head->pr=rgba_head>>24;
  vtx_head->pg=rgba_head>>16;
  vtx_head->pb=rgba_head>>8;

  /* Tint. */
  vtx_body->ta=0;
  vtx_head->ta=0;
  if (SPR->state&PS_HERO_STATE_HEAL) {
    vtx_body->tr=vtx_head->tr=0x00;
    vtx_body->tg=vtx_head->tg=0xff;
    vtx_body->tb=vtx_head->tb=0x00;
    vtx_body->ta=vtx_head->ta=(SPR->healtime*0xff)/PS_HERO_HEAL_TIME;
  } else if (SPR->state&PS_HERO_STATE_HURT) {
    vtx_body->tr=vtx_head->tr=0xff;
    vtx_body->tg=vtx_head->tg=0x00;
    vtx_body->tb=vtx_head->tb=0x00;
    vtx_body->ta=vtx_head->ta=(SPR->hurttime*0xff)/PS_HERO_HURT_TIME;
  } else if (SPR->state&PS_HERO_STATE_HIGHLIGHT) {
    vtx_body->tr=vtx_head->tr=0xff;
    vtx_body->tg=vtx_head->tg=0xff;
    vtx_body->tb=vtx_head->tb=0x00;
    vtx_body->ta=vtx_head->ta=(SPR->highlighttime*0xff)/PS_HERO_HIGHLIGHT_TIME;
  }

  /* Set undirectioned vertex tiles. (spr->tileid) is ignored; we use player properties instead. */
  vtx_head->tileid=SPR->player->plrdef->tileid_head;
  vtx_body->tileid=SPR->player->plrdef->tileid_body;

  /* Select variant for head. */
  if (SPR->state&PS_HERO_STATE_HURT) vtx_head->tileid+=0x20;
  else if (SPR->blinktime<=PS_HERO_BLINKTIME_SHUT) vtx_head->tileid+=0x10;

  /* Select variant for body. */
  if (SPR->state&PS_HERO_STATE_WALK) switch (SPR->walktime/PS_HERO_WALK_FRAME_DURATION) {
    case 0: vtx_body->tileid+=0x10; break;
    case 1: break;
    case 2: vtx_body->tileid+=0x20; break;
    case 3: break;
  } else if (SPR->state&(PS_HERO_STATE_SWORD|PS_HERO_STATE_HOOKSHOT)) {
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
  
  return vtxc;
}

/* Receive damage.
 */

static int _ps_hero_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->state&PS_HERO_STATE_GHOST) return 0; // Already dead, can't hurt me.

  if (assailant&&(assailant->type==&ps_sprtype_killozap)) {
    // The kill-o-zap is special: It doesn't respect heros' invincibility.
    // Without this special case, heroes can grab a heart and sneak through the kill-o-zap.
  } else {
    if (SPR->state&(PS_HERO_STATE_HEAL|PS_HERO_STATE_HURT)) return 0;
  }

  int fragile=ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr);
  if (!fragile) return 0;

  /* If we have the IMMORTAL skill, forget about it. */
  if (SPR->player&&SPR->player->plrdef&&(SPR->player->plrdef->skills&PS_SKILL_IMMORTAL)) {
    PS_SFX_HERO_HURT
    SPR->hurttime=PS_HERO_HURT_TIME; // Fake pain as a way of mocking pathetic mortals.
    return 0;
  }

  SPR->hp--;
  if (SPR->hp<=0) {
    PS_SFX_HERO_DEAD
    SPR->hp=0;
    if (ps_game_create_fireworks(game,spr->x,spr->y)<0) return -1;
    if (ps_game_report_kill(game,assailant,spr)<0) return -1;
    if (ps_hero_add_state(spr,PS_HERO_STATE_GHOST,game)<0) return -1;
    return 0;
  }

  // Max HP is 1, so this never actually happens:
  PS_SFX_HERO_HURT
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
  .configure=_ps_hero_configure,
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

int ps_hero_get_playerid(const struct ps_sprite *spr) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (!SPR->player) return 0;
  return SPR->player->playerid;
}

/* Set dragon charge.
 */
 
int ps_hero_set_dragon_charge(struct ps_sprite *spr,int p,int c) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (c<1) {
    SPR->dragoncharge=0;
  } else {
    if (p<0) p=0;
    if (p>=c) p=c-1;
    SPR->dragoncharge=(p*255)/c;
  }
  return 0;
}

/* Force kill.
 */
 
int ps_hero_force_kill(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;
  if (!game) return -1;
  if (ps_hero_remove_state(spr,PS_HERO_STATE_HEAL,game)<0) return -1;
  if (ps_hero_remove_state(spr,PS_HERO_STATE_HURT,game)<0) return -1;
  if (ps_sprite_receive_damage(game,spr,0)<0) return -1;
  return 0;
}
