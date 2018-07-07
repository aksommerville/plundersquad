#include "test/ps_test.h"
#include "game/ps_game.h"
#include "game/ps_dragoncharger.h"
#include "game/ps_player.h"
#include "game/ps_sprite.h"
#include "game/sprites/ps_sprite_hero.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"

/* Compose a bare-bones game object.
 */
 
static struct ps_game *new_mock_game(int playerc) {
  struct ps_game *game=calloc(1,sizeof(struct ps_game));
  if (!game) return 0;
  
  game->playerc=playerc;
  
  if (!(game->dragoncharger=ps_dragoncharger_new())) return 0;
  
  int i=playerc; while (i-->0) {
  
    struct ps_sprite *spr=ps_sprite_new(&ps_sprtype_hero);
    if (!spr) return 0;
    if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_KEEPALIVE,spr)<0) return 0;
    ps_sprite_del(spr);
    if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_UPDATE,spr)<0) return 0;
    if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_VISIBLE,spr)<0) return 0;
    if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_HERO,spr)<0) return 0;
    
    struct ps_player *player=ps_player_new();
    if (!player) return 0;
    game->playerv[i]=player;
    player->playerid=i+1;
    if (ps_hero_set_player(spr,player)<0) return 0;
  }
  
  return game;
}

/* Initialize the resource manager and install the dragon's sprdef.
 */
 
static int install_mock_sprdef() {
  PS_ASSERT_CALL(ps_resmgr_init("src/test/empty-res",0))
  
  struct ps_sprdef *sprdef=ps_sprdef_new(1);
  PS_ASSERT(sprdef)
  sprdef->type=&ps_sprtype_dragon;
  sprdef->fldv[0].k=PS_SPRDEF_FLD_grpmask;
  sprdef->fldv[0].v=(
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_HERO)|
  0);
  
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_SPRDEF);
  PS_ASSERT(restype)
  PS_ASSERT_CALL(ps_restype_res_insert(restype,0,23,sprdef)) // handoff sprdef
  
  return 0;
}

/* Create a flame sprite (in the hero group)
 */
 
static int create_flame(struct ps_game *game) {
  struct ps_sprite *spr=ps_sprite_new(&ps_sprtype_flames);
  PS_ASSERT(spr)
  PS_ASSERT_CALL(ps_game_set_group_mask_for_sprite(game,spr,(
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_HERO)|
  0)))
  ps_sprite_del(spr);
  return 0;
}

/* Helpers for reading charge.
 */
 
static int get_total_charge(const struct ps_game *game) {
  int total=0,i=PS_PLAYER_LIMIT;
  while (i-->0) total+=game->dragoncharger->charge[i];
  return total;
}

static int get_max_charge(const struct ps_game *game) {
  int max=0,i=PS_PLAYER_LIMIT;
  while (i-->0) {
    if (game->dragoncharger->charge[i]>max) {
      max=game->dragoncharger->charge[i];
    }
  }
  return max;
}

static int get_delta_charge(const struct ps_game *game) {
  int delta=0,i=PS_PLAYER_LIMIT;
  while (i-->0) {
    delta+=game->dragoncharger->charge[i]-game->dragoncharger->pvcharge[i];
  }
  return delta;
}

/* Helpers for analyzing state of sprites.
 */
 
static int count_dragons(const struct ps_game *game) {
  int c=0;
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    const struct ps_sprite *spr=grp->sprv[i];
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_DEATHROW,spr)) continue;
    if (spr->type==&ps_sprtype_dragon) c++;
  }
  return c;
}

static int count_heroes(const struct ps_game *game) {
  int c=0;
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    const struct ps_sprite *spr=grp->sprv[i];
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_DEATHROW,spr)) continue;
    if (spr->type==&ps_sprtype_hero) c++;
  }
  return c;
}

/* In a 1-player game, dragoncharger should be completely impotent.
 */
 
PS_TEST(test_dragoncharger_noop_in_solitaire,dragoncharger) {
  struct ps_game *game=new_mock_game(1);
  PS_ASSERT(game)
  PS_ASSERT_INTS(get_total_charge(game),0)
  PS_ASSERT_INTS(get_delta_charge(game),0)
  
  int i=10;
  while (i-->0) {
    PS_ASSERT_CALL(ps_dragoncharger_charge(game->dragoncharger,game,1))
  }
  PS_ASSERT_INTS_OP(get_total_charge(game),>,100)
  PS_ASSERT_INTS_OP(game->dragoncharger->charge[0],>,100)
  
  // We haven't loaded resources, so if it tries to instantiate the dragon, this will fail:
  PS_ASSERT_CALL(ps_dragoncharger_update(game->dragoncharger,game))
  
  ps_game_del(game);
  return 0;
}

/* Run it normally, create a dragon for two players.
 */
 
PS_TEST(test_dragoncharger_normal_actuation,dragoncharger) {
  PS_ASSERT_CALL(install_mock_sprdef())
  struct ps_game *game=new_mock_game(2);
  PS_ASSERT(game)
  PS_ASSERT_INTS(get_total_charge(game),0)
  PS_ASSERT_INTS(get_delta_charge(game),0)
  PS_ASSERT_INTS(count_heroes(game),2)
  
  int i=10;
  while (i-->0) {
    PS_ASSERT_CALL(ps_dragoncharger_charge(game->dragoncharger,game,1))
    PS_ASSERT_CALL(ps_dragoncharger_charge(game->dragoncharger,game,2))
  }
  PS_ASSERT_INTS_OP(get_total_charge(game),>,200)
  PS_ASSERT_INTS_OP(game->dragoncharger->charge[0],>,100)
  PS_ASSERT_INTS_OP(game->dragoncharger->charge[1],>,100)
  
  PS_ASSERT_CALL(ps_dragoncharger_update(game->dragoncharger,game))
  PS_ASSERT_INTS(count_dragons(game),1)
  PS_ASSERT_INTS(count_heroes(game),0)
  
  ps_game_del(game);
  ps_resmgr_quit();
  return 0;
}

/* Things can be in the HERO group which are not actual heroes.
 * If that happens, we must behave sensibly (7 July 2018 -- we don't).
 * This test fails initially, and I believe this reproduces the defect discovered by Faraan on 5 July.
 */
 
PS_TEST(test_dragoncharger_ignores_changelings_in_hero_group_1,dragoncharger) {
  PS_ASSERT_CALL(install_mock_sprdef())
  struct ps_game *game=new_mock_game(1);
  PS_ASSERT(game)
  PS_ASSERT_CALL(create_flame(game))
  
  int i=10; while (i-->0) {
    PS_ASSERT_CALL(ps_dragoncharger_charge(game->dragoncharger,game,1))
  }
  
  PS_ASSERT_CALL(ps_dragoncharger_update(game->dragoncharger,game))
  PS_ASSERT_INTS(count_dragons(game),0)
  PS_ASSERT_INTS(count_heroes(game),1)
  
  ps_game_del(game);
  ps_resmgr_quit();
  return 0;
}
 
PS_TEST(test_dragoncharger_ignores_changelings_in_hero_group_2,dragoncharger) {
  PS_ASSERT_CALL(install_mock_sprdef())
  struct ps_game *game=new_mock_game(2);
  PS_ASSERT(game)
  PS_ASSERT_CALL(create_flame(game))
  
  int i=10; while (i-->0) {
    PS_ASSERT_CALL(ps_dragoncharger_charge(game->dragoncharger,game,1))
  }
  
  PS_ASSERT_CALL(ps_dragoncharger_update(game->dragoncharger,game))
  PS_ASSERT_INTS(count_dragons(game),0)
  PS_ASSERT_INTS(count_heroes(game),2)
  
  ps_game_del(game);
  ps_resmgr_quit();
  return 0;
}
