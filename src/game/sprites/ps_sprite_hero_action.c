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
#include "res/ps_resmgr.h"
#include "scenario/ps_blueprint.h"

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

  //TODO Is it worth considering circle shapes? That might be unnecessary.

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

/* Look for a swordswitch and activate it if warranted.
 */

static int ps_hero_check_swordswitch(struct ps_sprite *spr,struct ps_game *game,struct ps_fbox hazardbox,int force) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=0; for (;i<grp->sprc;i++) {
    struct ps_sprite *fragile=grp->sprv[i];
    if (fragile->type!=&ps_sprtype_swordswitch) continue;
    
    struct ps_fbox fragilebox=ps_fbox(fragile->x-fragile->radius,fragile->x+fragile->radius,fragile->y-fragile->radius,fragile->y+fragile->radius);
    if (hazardbox.s<=fragilebox.n) return 0;
    if (hazardbox.n>=fragilebox.s) return 0;
    if (hazardbox.e<=fragilebox.w) return 0;
    if (hazardbox.w>=fragilebox.e) return 0;

    if (ps_swordswitch_activate(fragile,game,spr,force)<0) return -1;

    if (SPR->player&&(SPR->player->playerid>=1)&&(SPR->player->playerid<=PS_PLAYER_LIMIT)) {
      struct ps_stats_player *pstats=game->stats->playerv+SPR->player->playerid-1;
      pstats->switchc++;
    }
    
  }
  return 0;
}

/* Sword.
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

static int ps_hero_sword_begin(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (SPR->sword_in_progress) return 0;
  PS_SFX_SWORD
  SPR->sword_in_progress=1;
  struct ps_fbox swordbounds=ps_hero_get_sword_bounds(spr);
  if (ps_hero_assess_damage_to_others(spr,game,swordbounds)<0) return -1;
  if (ps_hero_check_swordswitch(spr,game,swordbounds,1)<0) return -1;
  return 0;
}

static int ps_hero_sword_end(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (!SPR->sword_in_progress) return 0;
  SPR->sword_in_progress=0;
  return 0;
}

static int ps_hero_sword_continue(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (!SPR->sword_in_progress) return 0;
  struct ps_fbox bounds=ps_hero_get_sword_bounds(spr);

  /* Hurt fragile things. */
  if (ps_hero_assess_damage_to_others(spr,game,bounds)<0) return -1;
  if (ps_hero_check_swordswitch(spr,game,bounds,0)<0) return -1;

  /* Fling prizes. */
  struct ps_sprgrp *prizes=game->grpv+PS_SPRGRP_PRIZE;
  int i=prizes->sprc; while (i-->0) {
    struct ps_sprite *prize=prizes->sprv[i];
    if (ps_sprite_collide_fbox(prize,&bounds)) {
      if (ps_prize_fling(prize,SPR->facedir)<0) return -1;
    }
  }
  
  return 0;
}

/* Arrow.
 */

static int ps_hero_arrow(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *arrow=ps_sprite_arrow_new(spr,game);
  if (!arrow) return -1;
  PS_SFX_ARROW
  //TODO hero animation when firing an arrow
  return 0;
}

/* Hookshot.
 */

static int ps_hero_hookshot_begin(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (SPR->hookshot_in_progress) return 0;
  PS_SFX_HOOKSHOT_BEGIN
  SPR->hookshot_in_progress=1;
  struct ps_sprite *hookshot=ps_sprite_hookshot_new(spr,game);
  if (!hookshot) return 0;
  return 0;
}

static int ps_hero_hookshot_end(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->hookshot_in_progress) return 0;
  SPR->hookshot_in_progress=0;

  // Movement and direction are inhibited during hook -- ensure that we treat any dpad action fresh.
  SPR->reexamine_dpad=1;
  
  return 0;
}

static int ps_hero_hookshot_continue(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (!SPR->hookshot_in_progress) return 0;
  return 0;
}

int ps_hero_abort_hookshot(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hero)) return -1;

  struct ps_sprite *master=ps_sprite_get_master(spr);
  if (master&&(master->type==&ps_sprtype_hookshot)) {
    if (ps_sprite_set_master(spr,0,game)<0) return -1;
  }
  
  if (SPR->hp) {
  
    // Hookshot may be aborting because we just died -- in that case, do not restore HOLE collisions.
    // Also, we might be on the back of a turtle. Detect that case by looking for (impassable==0).
    // I'm not crazy about any of this.

    if (spr->impassable) {
      spr->impassable|=1<<PS_BLUEPRINT_CELL_HOLE;
    }
  }
  return ps_hero_hookshot_end(spr,game);
}

/* Flame.
 */

static int ps_hero_flame_begin(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->flame_in_progress) return 0;
  PS_SFX_FLAME
  SPR->flame_in_progress=1;
  SPR->flame_counter=0;
  return 0;
}

static int ps_hero_flame_end(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->flame_in_progress) return 0;
  SPR->flame_in_progress=0;
  //TODO flames draw-down?
  return 0;
}

static int ps_hero_flame_continue(struct ps_sprite *spr,struct ps_game *game) {

  double distance;
  if (SPR->flame_counter<PS_HERO_FLAMES_RAMP_UP_TIME) {
    distance=(double)(SPR->flame_counter*PS_HERO_FLAMES_ORBIT_DISTANCE)/(double)PS_HERO_FLAMES_RAMP_UP_TIME;
  } else {
    distance=PS_HERO_FLAMES_ORBIT_DISTANCE;
  }
  distance+=PS_HERO_FLAMES_MARGIN;
  if (distance<=spr->radius) return 0;

  double restore_radius=spr->radius;
  spr->radius=distance;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_FRAGILE].sprc;i++) {
    struct ps_sprite *victim=game->grpv[PS_SPRGRP_FRAGILE].sprv[i];
    if (victim==spr) continue;
    if (!ps_sprites_collide(spr,victim)) continue;
    if (ps_sprite_receive_damage(game,victim,spr)<0) {
      spr->radius=restore_radius;
      return -1;
    }
  }
  spr->radius=restore_radius;
  
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

static int ps_hero_fly_begin(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->fly_in_progress) return 0;
  PS_SFX_TRANSFORM
  SPR->fly_in_progress=1;
  SPR->fly_counter=0;
  //TODO I don't like this willy-nilly modification of (impassable). Does it conflict with hookshot, or something else?
  spr->impassable&=~(1<<PS_BLUEPRINT_CELL_HOLE);
  return 0;
}

static int ps_hero_fly_end(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->fly_in_progress) return 0;
  PS_SFX_UNTRANSFORM
  SPR->fly_in_progress=0;
  spr->impassable|=1<<PS_BLUEPRINT_CELL_HOLE;
  return 0;
}

static int ps_hero_fly_continue(struct ps_sprite *spr,struct ps_game *game) {
  SPR->fly_counter++;
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
  switch (SPR->facedir) {
    case PS_DIRECTION_NORTH: y-=PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: y+=PS_TILESIZE; break;
    case PS_DIRECTION_WEST: x-=PS_TILESIZE; break;
    case PS_DIRECTION_EAST: x+=PS_TILESIZE; break;
  }
  
  struct ps_sprite *bomb=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
  if (!bomb) return -1;

  if (ps_sprite_bomb_throw(bomb,SPR->facedir,30)<0) return -1;

  return 0;
}

/* Frog.
 * TODO Frog skill probably doesn't need to exist.
 */

static int ps_hero_frog(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,TRACE,"%s",__func__);
  return 0;
}

/* Dispatchers.
 */

static int ps_hero_action_begin_1(struct ps_sprite *spr,struct ps_game *game,int action) {

  /* If we're dead, there are no actions.
   * Whatever this would have been, now all it can do is Charge the Dragon.
   */
  if (!SPR->hp) {
    if (ps_dragoncharger_charge(game->dragoncharger,game,SPR->player->playerid)<0) return -1;
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
    case PS_HERO_ACTION_FROG: return ps_hero_frog(spr,game);
  }
  return 0;
}

static int ps_hero_action_end_1(struct ps_sprite *spr,struct ps_game *game,int action) {
  switch (action) {
    case PS_HERO_ACTION_SWORD: return ps_hero_sword_end(spr,game);
    case PS_HERO_ACTION_HOOKSHOT: return ps_hero_hookshot_end(spr,game);
    case PS_HERO_ACTION_FLAME: return ps_hero_flame_end(spr,game);
    case PS_HERO_ACTION_FLY: return ps_hero_fly_end(spr,game);
  }
  return 0;
}

static int ps_hero_action_continue_1(struct ps_sprite *spr,struct ps_game *game,int action) {
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
  PS_SKILL_FROG| \
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
    case PS_SKILL_FROG: return PS_HERO_ACTION_FROG;
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
    case PS_SKILL_FROG: return PS_HERO_ACTION_FROG;
  }
  return PS_HERO_ACTION_NONE;
}
