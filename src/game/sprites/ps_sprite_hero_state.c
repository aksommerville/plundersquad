#include "ps.h"
#include "game/ps_sprite.h"
#include "game/sprites/ps_sprite_hero.h"
#include "game/sprites/ps_sprite_hookshot.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_sound_effects.h"
#include "game/ps_stats.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"

#define SPR ((struct ps_sprite_hero*)spr)

/* React to any state change by setting groups and similar things, from scratch.
 * We initially did these changes a la carte and it was a giant mess. Better to have one place for this.
 */

static int ps_hero_react_to_changed_state(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"state %08x",SPR->state);
  uint16_t pvimpassable=spr->impassable;

  /* The default set of groups, which we always belong to. */
  uint32_t grpmask=
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_HERO)|
    (1<<PS_SPRGRP_TELEPORT)|
  0;

  /* Almost everything is disabled when offscreen. */
  if (SPR->state&PS_HERO_STATE_OFFSCREEN) {
    // No need to set (impassable) or (collide_sprites) because we are not in PHYSICS.
    grpmask&=~(1<<PS_SPRGRP_VISIBLE);

  /* Ghosts have a decidedly different profile from the living. */
  } else if (SPR->state&PS_HERO_STATE_GHOST) {
    grpmask|=
      (1<<PS_SPRGRP_HEROHAZARD)|
      (1<<PS_SPRGRP_PHYSICS)|
    0;
    spr->impassable=
      (1<<PS_BLUEPRINT_CELL_SOLID)|
      (1<<PS_BLUEPRINT_CELL_LATCH)|
    0;
    spr->collide_sprites=0;

  /* Onscreen and alive. */
  } else {
    grpmask|=
      (1<<PS_SPRGRP_PHYSICS)|
      (1<<PS_SPRGRP_SOLID)|
      (1<<PS_SPRGRP_FRAGILE)|
      (1<<PS_SPRGRP_LATCH)|
    0;
    spr->impassable=
      (1<<PS_BLUEPRINT_CELL_SOLID)|
      (1<<PS_BLUEPRINT_CELL_LATCH)|
    0;
    spr->collide_sprites=1;
    if (!(SPR->state&(PS_HERO_STATE_FERRY|PS_HERO_STATE_PUMPKIN))) {
      spr->impassable|=(1<<PS_BLUEPRINT_CELL_HOLE);
    }
    if (SPR->state&PS_HERO_STATE_PUMPKIN) {
      // No TELEPORT for PUMPKIN, that would get messy.
      grpmask&=~(1<<PS_SPRGRP_TELEPORT);
    }
  }

  /* Commit sprite groups. */
  if (ps_game_set_group_mask_for_sprite(game,spr,grpmask)<0) return -1;

  /* If our impassable mask changed, try to fudge our position onto a legal cell. */
  if (spr->impassable!=pvimpassable) {
    if (ps_sprite_attempt_legal_position(spr,game)<0) return -1;
  }

  return 0;
}

/* GHOST
 */

static int ps_hero_add_GHOST(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Update stats. */
  if (SPR->player&&(SPR->player->playerid>=1)&&(SPR->player->playerid<=PS_PLAYER_LIMIT)) {
    struct ps_stats_player *pstats=game->stats->playerv+SPR->player->playerid-1;
    pstats->deathc++;
  }

  /* End any action in progress. */
  if (ps_hero_action_end(spr,game)<0) return -1;
  if (ps_hero_auxaction_end(spr,game)<0) return -1;

  /* Force HP to zero if it isn't already. */
  SPR->hp=0;

  return 2;
}

static int ps_hero_remove_GHOST(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Force positive HP if zero. */
  if (!SPR->hp) SPR->hp=1;
  
  return 2;
}

/* PUMPKIN
 */

static int ps_hero_add_PUMPKIN(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

static int ps_hero_remove_PUMPKIN(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* FERRY
 */

static int ps_hero_add_FERRY(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

static int ps_hero_remove_FERRY(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* OFFSCREEN
 */

static int ps_hero_add_OFFSCREEN(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Force location all the way off, also confirm that he really is offscreen and reject if not. */
  if (spr->x<=0.0) {
    spr->x=-(PS_TILESIZE>>1);
  } else if (spr->x>=PS_SCREENW) {
    spr->x=PS_SCREENW+(PS_TILESIZE>>1);
  } else if (spr->y<=0.0) {
    spr->y=-(PS_TILESIZE>>1);
  } else if (spr->y>=PS_SCREENH) {
    spr->y=PS_SCREENH+(PS_TILESIZE>>1);
  } else {
    ps_log(HEROSTATE,TRACE,"reject offscreen");
    return 0;
  }
  
  if (ps_game_add_indicator_for_hero(game,spr)<0) return -1;
  
  return 2;
}

static int ps_hero_remove_OFFSCREEN(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Force location in bounds. */
  if (spr->x<=0.0) spr->x=1.0;
  else if (spr->x>=PS_SCREENW) spr->x=PS_SCREENW-1;
  if (spr->y<=0.0) spr->y=1.0;
  else if (spr->y>=PS_SCREENH) spr->y=PS_SCREENH-1;
  
  if (ps_game_remove_indicator_for_hero(game,spr)<0) return -1;

  game->inhibit_screen_switch=0;
  
  return 2;
}

/* WALK
 */

static int ps_hero_add_WALK(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Reject if certain actions in progress. */
  if (SPR->state&PS_HERO_STATE_SWORD) return 0;
  if (SPR->state&PS_HERO_STATE_HOOKSHOT) return 0;
  
  return 2;
}

static int ps_hero_remove_WALK(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* FLY
 */

static int ps_hero_is_over_hole(const struct ps_sprite *spr,const struct ps_game *game) {
  if (!game||!game->grid) return 0;
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col<0)||(col>=PS_GRID_COLC)) return 0;
  if ((row<0)||(row>=PS_GRID_ROWC)) return 0;
  uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
  return (physics==PS_BLUEPRINT_CELL_HOLE)?1:0;
}

static int ps_hero_add_FLY(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  PS_SFX_TRANSFORM
  SPR->fly_counter=0;
  SPR->defer_fly_end=0;
  
  return 2;
}

static int ps_hero_remove_FLY(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Reject and defer if above a hole. */
  if (ps_hero_is_over_hole(spr,game)) {
    SPR->defer_fly_end=1;
    return 0;
  }
  
  PS_SFX_UNTRANSFORM
  
  return 2;
}

/* SWORD
 */

static int ps_hero_add_SWORD(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Reject in certain states. */
  if (SPR->state&PS_HERO_STATE_HOOKSHOT) return 0;
  if (SPR->state&PS_HERO_STATE_FLY) return 0;

  PS_SFX_SWORD
  
  return 2;
}

static int ps_hero_remove_SWORD(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* HURT
 */

static int ps_hero_add_HURT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  if (SPR->hp==1) {
    int err=ps_hero_add_state(spr,PS_HERO_STATE_GHOST,game);
    if (err<0) return -1;
    return 0;
  }

  SPR->hurttime=PS_HERO_HURT_TIME;
  
  return 2;
}

static int ps_hero_remove_HURT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* HEAL
 */

static int ps_hero_add_HEAL(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  if (SPR->state&PS_HERO_STATE_GHOST) {
    SPR->state&=~PS_HERO_STATE_GHOST;
  }

  if (SPR->hp>=PS_HERO_DEFAULT_HP) {
    return 0;
  }
  SPR->hp=PS_HERO_DEFAULT_HP;

  SPR->healtime=PS_HERO_HEAL_TIME;
  
  return 2;
}

static int ps_hero_remove_HEAL(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* HIGHLIGHT
 */

static int ps_hero_add_HIGHLIGHT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  SPR->highlighttime=PS_HERO_HIGHLIGHT_TIME;
  return 2;
}

static int ps_hero_remove_HIGHLIGHT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* STOPINPUT
 */

static int ps_hero_add_STOPINPUT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

static int ps_hero_remove_STOPINPUT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);
  return 2;
}

/* HOOKSHOT
 */

static int ps_hero_add_HOOKSHOT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  /* Inhibited by other state? */
  if (SPR->state&PS_HERO_STATE_SWORD) return 0;
  if (SPR->state&PS_HERO_STATE_FLY) return 0;

  /* Create sprite. */
  PS_SFX_HOOKSHOT_BEGIN
  struct ps_sprite *hookshot=ps_sprite_hookshot_new(spr,game);
  if (!hookshot) return -1;
  
  return 2;
}

static int ps_hero_remove_HOOKSHOT(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(HEROSTATE,TRACE,"%s",__func__);

  struct ps_sprite *master=ps_sprite_get_master(spr);
  if (master&&(master->type==&ps_sprtype_hookshot)) {
    if (ps_sprite_set_master(spr,0,game)<0) return -1;
  }
  
  return 2;
}

/* Add state, public.
 */
 
int ps_hero_add_state(struct ps_sprite *spr,uint32_t incoming,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)||!game) return -1;
  if (SPR->state&incoming) return 1;
  int err=-1;
  switch (incoming) {
    case PS_HERO_STATE_GHOST: err=ps_hero_add_GHOST(spr,game); break;
    case PS_HERO_STATE_PUMPKIN: err=ps_hero_add_PUMPKIN(spr,game); break;
    case PS_HERO_STATE_FERRY: err=ps_hero_add_FERRY(spr,game); break;
    case PS_HERO_STATE_OFFSCREEN: err=ps_hero_add_OFFSCREEN(spr,game); break;
    case PS_HERO_STATE_WALK: err=ps_hero_add_WALK(spr,game); break;
    case PS_HERO_STATE_FLY: err=ps_hero_add_FLY(spr,game); break;
    case PS_HERO_STATE_SWORD: err=ps_hero_add_SWORD(spr,game); break;
    case PS_HERO_STATE_HURT: err=ps_hero_add_HURT(spr,game); break;
    case PS_HERO_STATE_HEAL: err=ps_hero_add_HEAL(spr,game); break;
    case PS_HERO_STATE_HIGHLIGHT: err=ps_hero_add_HIGHLIGHT(spr,game); break;
    case PS_HERO_STATE_STOPINPUT: err=ps_hero_add_STOPINPUT(spr,game); break;
    case PS_HERO_STATE_HOOKSHOT: err=ps_hero_add_HOOKSHOT(spr,game); break;
  }
  if (err<0) return -1;
  if (err>=2) {
    err=2;
    SPR->state|=incoming;
    if (ps_hero_react_to_changed_state(spr,game)<0) return -1;
  }
  return 0;
}

/* Remove state, public.
 */
 
int ps_hero_remove_state(struct ps_sprite *spr,uint32_t outgoing,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)||!game) return -1;
  if (!(SPR->state&outgoing)) return 1;
  int err=-1;
  switch (outgoing) {
    case PS_HERO_STATE_GHOST: err=ps_hero_remove_GHOST(spr,game); break;
    case PS_HERO_STATE_PUMPKIN: err=ps_hero_remove_PUMPKIN(spr,game); break;
    case PS_HERO_STATE_FERRY: err=ps_hero_remove_FERRY(spr,game); break;
    case PS_HERO_STATE_OFFSCREEN: err=ps_hero_remove_OFFSCREEN(spr,game); break;
    case PS_HERO_STATE_WALK: err=ps_hero_remove_WALK(spr,game); break;
    case PS_HERO_STATE_FLY: err=ps_hero_remove_FLY(spr,game); break;
    case PS_HERO_STATE_SWORD: err=ps_hero_remove_SWORD(spr,game); break;
    case PS_HERO_STATE_HURT: err=ps_hero_remove_HURT(spr,game); break;
    case PS_HERO_STATE_HEAL: err=ps_hero_remove_HEAL(spr,game); break;
    case PS_HERO_STATE_HIGHLIGHT: err=ps_hero_remove_HIGHLIGHT(spr,game); break;
    case PS_HERO_STATE_STOPINPUT: err=ps_hero_remove_STOPINPUT(spr,game); break;
    case PS_HERO_STATE_HOOKSHOT: err=ps_hero_remove_HOOKSHOT(spr,game); break;
  }
  if (err<0) return -1;
  if (err>=2) {
    err=2;
    SPR->state&=~outgoing;
    if (ps_hero_react_to_changed_state(spr,game)<0) return -1;
  }
  return 0;
}
