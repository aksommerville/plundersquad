#include "ps.h"
#include "ps_sprite_hero.h"
#include "ps_sprite_hookshot.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "util/ps_geometry.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"

#define PS_HERO_MARTYR_EXPLOSION_SPRDEFID 7

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

/* Sword.
 */

static struct ps_fbox ps_hero_get_sword_bounds(const struct ps_sprite *spr) {
  const double width=PS_TILESIZE/4.0;
  const double length=(PS_TILESIZE*7.0)/8.0;
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
  SPR->sword_in_progress=1;
  if (ps_hero_assess_damage_to_others(spr,game,ps_hero_get_sword_bounds(spr))<0) return -1;
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
  //TODO hero animation when firing an arrow
  return 0;
}

/* Hookshot.
 */

static int ps_hero_hookshot_begin(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
  if (SPR->hookshot_in_progress) return 0;
  SPR->hookshot_in_progress=1;
  struct ps_sprite *hookshot=ps_sprite_hookshot_new(spr,game);
  if (!hookshot) return 0;
  return 0;
}

static int ps_hero_hookshot_end(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s",__func__);
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
  spr->collide_hole=1;
  return ps_hero_hookshot_end(spr,game);
}

/* Flame.
 */

static int ps_hero_flame_begin(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->flame_in_progress) return 0;
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
  SPR->fly_in_progress=1;
  SPR->fly_counter=0;
  spr->collide_hole=0;//TODO conflict with hookshot pumpkin? I think there is a narrow error case there, but not sure how to find it.
  return 0;
}

static int ps_hero_fly_end(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->fly_in_progress) return 0;
  SPR->fly_in_progress=0;
  spr->collide_hole=1;
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
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_HERO_MARTYR_EXPLOSION_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for martyr explosion",PS_HERO_MARTYR_EXPLOSION_SPRDEFID);
    return -1;
  }
  struct ps_sprite *explosion=ps_sprdef_instantiate(game,sprdef,0,0,spr->x,spr->y);
  if (!explosion) return -1;

  return 0;
}

/* Frog.
 * TODO This probably doesn't need to exist.
 */

static int ps_hero_frog(struct ps_sprite *spr,struct ps_game *game) {
  ps_log(GAME,TRACE,"%s",__func__);
  return 0;
}

/* Dispatchers.
 */

static int ps_hero_action_begin_1(struct ps_sprite *spr,struct ps_game *game,int action) {
  if (!SPR->hp) return 0; // Ghosts can't do anything.
  switch (action) {
    case PS_HERO_ACTION_SWORD: return ps_hero_sword_begin(spr,game);
    case PS_HERO_ACTION_ARROW: return ps_hero_arrow(spr,game);
    case PS_HERO_ACTION_HOOKSHOT: return ps_hero_hookshot_begin(spr,game);
    case PS_HERO_ACTION_FLAME: return ps_hero_flame_begin(spr,game);
    case PS_HERO_ACTION_HEAL: return ps_hero_action_heal(spr,game);
    case PS_HERO_ACTION_CARRY: return ps_hero_carry_begin(spr,game);
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
    case PS_HERO_ACTION_CARRY: return ps_hero_carry_end(spr,game);
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
  PS_SKILL_CARRY| \
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
    case PS_SKILL_CARRY: return PS_HERO_ACTION_CARRY;
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
    case PS_SKILL_CARRY: return PS_HERO_ACTION_CARRY;
    case PS_SKILL_FLY: return PS_HERO_ACTION_FLY;
    case PS_SKILL_MARTYR: return PS_HERO_ACTION_MARTYR;
    case PS_SKILL_FROG: return PS_HERO_ACTION_FROG;
  }
  return PS_HERO_ACTION_NONE;
}
