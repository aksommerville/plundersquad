#include "ps.h"
#include "ps_sprite_hero.h"
#include "ps_sprite_hookshot.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_stats.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_sound_effects.h"
#include "game/ps_dragoncharger.h"
#include "util/ps_geometry.h"
#include "input/ps_input_button.h"
#include "input/ps_input.h"
#include "res/ps_resmgr.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"

#define PS_HERO_MARTYR_EXPLOSION_SPRDEFID 7
#define PS_HERO_BOMB_SPRDEFID 25

#define SPR ((struct ps_sprite_hero*)spr)

/* Damage a sprite.
 */

static int ps_hero_assess_damage_to_other(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *fragile,struct ps_fbox hazardbox) {
  if (spr==fragile) return 0; // Can't hurt self with a weapon.

  /* Do the boxes collide? */
  struct ps_fbox fragilebox=ps_fbox(fragile->x-fragile->radius,fragile->x+fragile->radius,fragile->y-fragile->radius,fragile->y+fragile->radius);
  if (hazardbox.s<=fragilebox.n) return 0;
  if (hazardbox.n>=fragilebox.s) return 0;
  if (hazardbox.e<=fragilebox.w) return 0;
  if (hazardbox.w>=fragilebox.e) return 0;

  // We don't bother recognizing circle shapes. I don't think it matters.

  if (ps_sprite_receive_damage(game,fragile,spr)<0) return -1;

  return 1;
}

static int ps_hero_assess_damage_to_others(struct ps_sprite *spr,struct ps_game *game,struct ps_fbox hazardbox) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=0; for (;i<grp->sprc;i++) {
    struct ps_sprite *fragile=grp->sprv[i];
    if (ps_hero_assess_damage_to_other(spr,game,fragile,hazardbox)<0) return -1;
  }
  return 0;
}

/* Sword active boundaries.
 */

static struct ps_fbox ps_hero_get_sword_bounds(const struct ps_sprite *spr) {
  const double width=PS_TILESIZE/4.0;
  const double length=(PS_TILESIZE*9.0)/8.0;
  const double top=spr->y-(PS_TILESIZE>>1);
  const double left=spr->x-(PS_TILESIZE>>1);
  switch (SPR->facedir) {
    case PS_DIRECTION_NORTH: return ps_fbox(spr->x-width/2.0,spr->x+width/2.0,top-length,top);
    case PS_DIRECTION_SOUTH: return ps_fbox(spr->x-width/2.0,spr->x+width/2.0,top+PS_TILESIZE,top+PS_TILESIZE+length);
    case PS_DIRECTION_WEST: return ps_fbox(left-length,left,spr->y-width/2.0,spr->y+width/2.0);
    case PS_DIRECTION_EAST: return ps_fbox(left+PS_TILESIZE,left+PS_TILESIZE+length,spr->y-width/2.0,spr->y+width/2.0);
  }
  return ps_fbox(0,0,0,0);
}

/* Look for sword-aware sprites intersecting the sword's boundaries, and alert them.
 * (state) is (0=off, 1=on, 2=continue).
 */

static int ps_hero_alert_swordaware(struct ps_sprite *spr,struct ps_game *game,int state) {
  struct ps_fbox bounds=ps_hero_get_sword_bounds(spr);
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_SWORDAWARE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (ps_sprite_collide_fbox(victim,&bounds)) {
      if (ps_sprite_react_to_sword(victim,game,spr,state)<0) return -1;
    }
  }
  return 0;
}

/* Sword.
 */

static int ps_hero_sword_begin(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (ps_hero_add_state(spr,PS_HERO_STATE_SWORD,game)<0) return -1;
  if (ps_hero_alert_swordaware(spr,game,1)<0) return -1;
  return 0;
}

static int ps_hero_sword_end(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (ps_hero_remove_state(spr,PS_HERO_STATE_SWORD,game)<0) return -1;
  if (ps_hero_alert_swordaware(spr,game,0)<0) return -1;
  return 0;
}

static int ps_hero_sword_continue(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (!(SPR->state&PS_HERO_STATE_SWORD)) return 0;
  struct ps_fbox bounds=ps_hero_get_sword_bounds(spr);

  /* Hurt fragile things. */
  if (ps_hero_assess_damage_to_others(spr,game,bounds)<0) return -1;
  if (ps_hero_alert_swordaware(spr,game,2)<0) return -1;
  
  return 0;
}

/* Arrow.
 */

static int ps_hero_arrow(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *arrow=ps_sprite_arrow_new(spr,game);
  if (!arrow) return -1;
  if (arrow==spr) {
    PS_SFX_ARROW_REJECTED
  } else {
    PS_SFX_ARROW
  }
  return 0;
}

/* Hookshot.
 */

static int ps_hero_hookshot_begin(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (ps_hero_add_state(spr,PS_HERO_STATE_HOOKSHOT,game)<0) return -1;
  return 0;
}

static int ps_hero_hookshot_end(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_hero_remove_state(spr,PS_HERO_STATE_HOOKSHOT,game)<0) return -1;
  return 0;
}

static int ps_hero_hookshot_continue(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (!(SPR->state&PS_HERO_STATE_HOOKSHOT)) return 0;
  return 0;
}

/* Flame.
 */

static int ps_hero_flame_begin(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprite *flames=ps_sprite_flames_find_for_hero(game,spr);
  if (flames) return 0;

  PS_SFX_FLAMES_BEGIN

  if (!(flames=ps_sprite_new(&ps_sprtype_flames))) return -1;

  if (ps_game_set_group_mask_for_sprite(game,flames,
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_HERO)|
  0)<0) {
    ps_sprite_del(flames);
    return -1;
  }
  ps_sprite_del(flames);

  if (ps_sprite_set_master(flames,spr,game)<0) return -1;

  return 0;
}

static int ps_hero_flame_end(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *flames=ps_sprite_flames_find_for_hero(game,spr);
  if (!flames) return 0;
  PS_SFX_FLAMES_THROW
  if (ps_sprite_flames_throw(flames,SPR->facedir)<0) return -1;
  return 0;
}

static int ps_hero_flame_continue(struct ps_sprite *spr,struct ps_game *game) {
  return 0;
}

/* Heal.
 */

static int ps_hero_action_heal(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *missile=ps_sprite_healmissile_new(spr,game);
  if (!missile) return -1;
  PS_SFX_HEALMISSILE
  return 0;
}

/* Carry.
 * XXX Not sure I really want this. We can revisit later, either implement it or remove it.
 * It will take some pretty heavy changes to the base sprite.
 */

static int ps_hero_carry_begin(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,TRACE,"%s",__func__);
  return 0;
}

static int ps_hero_carry_end(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,TRACE,"%s",__func__);
  return 0;
}

/* Fly.
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

static int ps_hero_fly_begin(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_hero_add_state(spr,PS_HERO_STATE_FLY,game)<0) return -1;
  return 0;
}

int ps_hero_fly_end(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_hero_remove_state(spr,PS_HERO_STATE_FLY,game)<0) return -1;
  return 0;
}

static int ps_hero_fly_continue(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->state&PS_HERO_STATE_FLY) {
    SPR->fly_counter++;
  }
  return 0;
}

/* Martyr.
 */

static int ps_hero_martyr(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);

  /* Just create the explosion; it will kill us on its own time. */
  PS_SFX_EXPLODE
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_HERO_MARTYR_EXPLOSION_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for martyr explosion",PS_HERO_MARTYR_EXPLOSION_SPRDEFID);
    return -1;
  }
  struct ps_sprite *explosion=ps_sprdef_instantiate(game,sprdef,0,0,spr->x,spr->y);
  if (!explosion) return -1;

  return 0;
}

/* Bomb.
 */

static int ps_hero_bomb(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_HERO_BOMB_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for bomb",PS_HERO_BOMB_SPRDEFID);
    return -1;
  }
  
  int x=spr->x;
  int y=spr->y;
  int offset=(PS_TILESIZE*2)/3;
  switch (SPR->facedir) {
    case PS_DIRECTION_NORTH: y-=offset; break;
    case PS_DIRECTION_SOUTH: y+=offset; break;
    case PS_DIRECTION_WEST: x-=offset; break;
    case PS_DIRECTION_EAST: x+=offset; break;
  }

  /* Quick check to ensure we're not throwing it into a wall or hole.
   */
  const struct ps_grid_cell *cell=ps_grid_get_cell(game->grid,x/PS_TILESIZE,y/PS_TILESIZE);
  if (cell) switch (cell->physics) {
    case PS_BLUEPRINT_CELL_SOLID:
    case PS_BLUEPRINT_CELL_LATCH: {
        PS_SFX_BOMB_REJECT
      } return 0;
    case PS_BLUEPRINT_CELL_HOLE: {
        if (ps_game_create_splash(game,x,y)<0) return -1;
      } return 0;
  }

  PS_SFX_BOMB_THROW
  
  struct ps_sprite *bomb=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
  if (!bomb) return -1;

  if (ps_sprite_bomb_throw(bomb,SPR->facedir,30)<0) return -1;

  return 0;
}

/* Nitro.
 */

static int ps_hero_nitro_begin(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,DEBUG,"%s",__func__);
  if (ps_hero_add_state(spr,PS_HERO_STATE_NITRO,game)<0) return -1;
  return 0;
}

static int ps_hero_nitro_end(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,DEBUG,"%s",__func__);
  if (ps_hero_remove_state(spr,PS_HERO_STATE_NITRO,game)<0) return -1;
  return 0;
}

/* Dispatchers.
 */

static int ps_hero_action_begin_1(struct ps_sprite *spr,struct ps_game *game,int action) {

  /* If we're dead, there are no actions.
   * Whatever this would have been, now all it can do is Charge the Dragon.
   */
  if (!SPR->hp) {
    if (game->dragoncharger) {
      if (ps_dragoncharger_charge(game->dragoncharger,game,SPR->player->playerid)<0) return -1;
    }
    return 0;
  }

  switch (action) {
    case PS_HERO_ACTION_SWORD: return ps_hero_sword_begin(spr,game);
    case PS_HERO_ACTION_ARROW: return ps_hero_arrow(spr,game);
    case PS_HERO_ACTION_HOOKSHOT: return ps_hero_hookshot_begin(spr,game);
    case PS_HERO_ACTION_FLAME: return ps_hero_flame_begin(spr,game);
    case PS_HERO_ACTION_HEAL: return ps_hero_action_heal(spr,game);
    case PS_HERO_ACTION_BOMB: return ps_hero_bomb(spr,game);
    case PS_HERO_ACTION_FLY: return ps_hero_fly_begin(spr,game);
    case PS_HERO_ACTION_MARTYR: return ps_hero_martyr(spr,game);
    case PS_HERO_ACTION_NITRO: return ps_hero_nitro_begin(spr,game);
  }
  return 0;
}

static int ps_hero_action_end_1(struct ps_sprite *spr,struct ps_game *game,int action) {
  switch (action) {
    case PS_HERO_ACTION_SWORD: return ps_hero_sword_end(spr,game);
    case PS_HERO_ACTION_HOOKSHOT: return ps_hero_hookshot_end(spr,game);
    case PS_HERO_ACTION_FLAME: return ps_hero_flame_end(spr,game);
    case PS_HERO_ACTION_FLY: return ps_hero_fly_end(spr,game);
    case PS_HERO_ACTION_NITRO: return ps_hero_nitro_end(spr,game);
  }
  return 0;
}

static int ps_hero_action_continue_1(struct ps_sprite *spr,struct ps_game *game,int action) {
  if (!SPR->hp) return 0;
  switch (action) {
    case PS_HERO_ACTION_SWORD: return ps_hero_sword_continue(spr,game);
    case PS_HERO_ACTION_HOOKSHOT: return ps_hero_hookshot_continue(spr,game);
    case PS_HERO_ACTION_FLAME: return ps_hero_flame_continue(spr,game);
    case PS_HERO_ACTION_FLY: return ps_hero_fly_continue(spr,game);
  }
  return 0;
}

/* Entry points, in response to input events.
 */
 
int ps_hero_action_begin(struct ps_sprite *spr,struct ps_game *game) {
  return ps_hero_action_begin_1(spr,game,ps_hero_get_principal_action(spr));
}
 
int ps_hero_action_end(struct ps_sprite *spr,struct ps_game *game) {
  return ps_hero_action_end_1(spr,game,ps_hero_get_principal_action(spr));
}

int ps_hero_action_continue(struct ps_sprite *spr,struct ps_game *game) {
  return ps_hero_action_continue_1(spr,game,ps_hero_get_principal_action(spr));
}
 
int ps_hero_auxaction_begin(struct ps_sprite *spr,struct ps_game *game) {

  // Using 'B' button to swap inputs. Enable this only while testing.
  // If we want this feature in the real game, it needs a friendlier implementation.
  if (PS_B_TO_SWAP_INPUT) {
    if (ps_input_swap_assignments()<0) return -1;
    if (ps_game_highlight_enabled_players(game)<0) return -1;
    return 0;
  }

  return ps_hero_action_begin_1(spr,game,ps_hero_get_auxiliary_action(spr));
}
 
int ps_hero_auxaction_end(struct ps_sprite *spr,struct ps_game *game) {
  return ps_hero_action_end_1(spr,game,ps_hero_get_auxiliary_action(spr));
}

int ps_hero_auxaction_continue(struct ps_sprite *spr,struct ps_game *game) {
  return ps_hero_action_continue_1(spr,game,ps_hero_get_auxiliary_action(spr));
}

/* Which action is enabled?
 * We can have double-skilled hero definitions. Then auxiliary will become relevant.
 */

#define PS_HERO_SKILL_MASK ( \
  PS_SKILL_SWORD| \
  PS_SKILL_ARROW| \
  PS_SKILL_HOOKSHOT| \
  PS_SKILL_FLAME| \
  PS_SKILL_HEAL| \
  PS_SKILL_BOMB| \
  PS_SKILL_FLY| \
  PS_SKILL_MARTYR| \
  PS_SKILL_IMMORTAL| \
0)
 
int ps_hero_get_principal_action(const struct ps_sprite *spr) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return PS_HERO_ACTION_NONE;
  if (!SPR->player) return PS_HERO_ACTION_NONE;
  switch (SPR->player->plrdef->skills&PS_HERO_SKILL_MASK) {
    case PS_SKILL_SWORD: return PS_HERO_ACTION_SWORD;
    case PS_SKILL_ARROW: return PS_HERO_ACTION_ARROW;
    case PS_SKILL_HOOKSHOT: return PS_HERO_ACTION_HOOKSHOT;
    case PS_SKILL_FLAME: return PS_HERO_ACTION_FLAME;
    case PS_SKILL_HEAL: return PS_HERO_ACTION_HEAL;
    case PS_SKILL_BOMB: return PS_HERO_ACTION_BOMB;
    case PS_SKILL_FLY: return PS_HERO_ACTION_FLY;
    case PS_SKILL_MARTYR: return PS_HERO_ACTION_MARTYR;
    case PS_SKILL_IMMORTAL: return PS_HERO_ACTION_NITRO;
  }
  return PS_HERO_ACTION_NONE;
}

int ps_hero_get_auxiliary_action(const struct ps_sprite *spr) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return PS_HERO_ACTION_NONE;
  if (!SPR->player) return PS_HERO_ACTION_NONE;
  switch (SPR->player->plrdef->skills&PS_HERO_SKILL_MASK) {
    case PS_SKILL_SWORD: return PS_HERO_ACTION_SWORD;
    case PS_SKILL_ARROW: return PS_HERO_ACTION_ARROW;
    case PS_SKILL_HOOKSHOT: return PS_HERO_ACTION_HOOKSHOT;
    case PS_SKILL_FLAME: return PS_HERO_ACTION_FLAME;
    case PS_SKILL_HEAL: return PS_HERO_ACTION_HEAL;
    case PS_SKILL_BOMB: return PS_HERO_ACTION_BOMB;
    case PS_SKILL_FLY: return PS_HERO_ACTION_FLY;
    case PS_SKILL_MARTYR: return PS_HERO_ACTION_MARTYR;
    case PS_SKILL_IMMORTAL: return PS_HERO_ACTION_NITRO;
  }
  return PS_HERO_ACTION_NONE;
}
