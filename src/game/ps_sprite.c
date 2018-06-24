#include "ps.h"
#include "ps_sprite.h"
#include "ps_sound_effects.h"
#include "sprites/ps_sprite_hero.h"
#include "akgl/akgl.h"
#include "ps_game.h"
#include "ps_player.h"
#include "ps_stats.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"

/* New.
 */

struct ps_sprite *ps_sprite_new(const struct ps_sprtype *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ps_sprite)) return 0;

  struct ps_sprite *spr=calloc(1,type->objlen);
  if (!spr) return 0;
  
  spr->type=type;
  spr->refc=1;
  spr->layer=type->layer;
  spr->radius=type->radius;
  spr->shape=type->shape;
  spr->collide_sprites=1;
  spr->impassable=0;
  spr->opacity=0xff;

  if (type->init) {
    if (type->init(spr)<0) {
      ps_sprite_del(spr);
      return 0;
    }
  }

  return spr;
}

/* Delete.
 */
 
void ps_sprite_del(struct ps_sprite *spr) {
  if (!spr) return;
  if (spr->refc-->1) return;

  if (spr->type->del) spr->type->del(spr);

  ps_sprdef_del(spr->def);

  ps_sprgrp_clear(spr->master);
  ps_sprgrp_del(spr->master);

  if (spr->grpc) ps_log(SPRITE,ERROR,"Deleting sprite %p (type '%s'), grpc==%d.",spr,spr->type->name,spr->grpc);
  if (spr->grpv) {
    while (spr->grpc-->0) ps_sprgrp_del(spr->grpv[spr->grpc]);
    free(spr->grpv);
  }

  free(spr);
}

/* Retain.
 */
 
int ps_sprite_ref(struct ps_sprite *spr) {
  if (!spr) return -1;
  if (spr->refc<1) return -1;
  if (spr->refc==INT_MAX) return -1;
  spr->refc++;
  return 0;
}

/* Accessors.
 */

int ps_sprite_set_sprdef(struct ps_sprite *spr,struct ps_sprdef *def) {
  if (!spr) return -1;
  if (spr->def==def) return 0;
  if (def&&(ps_sprdef_ref(def)<0)) return -1;
  ps_sprdef_del(spr->def);
  spr->def=def;
  return 0;
}

/* Hooks.
 */

int ps_sprite_update(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||!game) return -1;
  if (!spr->type->update) return 0;
  return spr->type->update(spr,game);
}

int ps_sprite_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if ((vtxa<0)||(vtxa&&!vtxv)||!spr) return -1;
  if (spr->type->draw) {
    return spr->type->draw(vtxv,vtxa,spr);
  } else {
    if (vtxa<1) return 1;
    vtxv[0].x=spr->x;
    vtxv[0].y=spr->y;
    vtxv[0].tileid=spr->tileid;
    vtxv[0].size=PS_TILESIZE;
    vtxv[0].tr=0;
    vtxv[0].tg=0;
    vtxv[0].tb=0;
    vtxv[0].ta=0;
    vtxv[0].pr=0x80;
    vtxv[0].pg=0x80;
    vtxv[0].pb=0x80;
    vtxv[0].a=spr->opacity;
    vtxv[0].t=0;
    vtxv[0].xform=0;
    return 1;
  }
}

/* Receive damage.
 */
 
int ps_sprite_receive_damage(struct ps_game *game,struct ps_sprite *victim,struct ps_sprite *assailant) {
  if (!game) return -1;
  if (!victim) return -1;
  //ps_log(GAME,DEBUG,"Sprite %p(%s) damaged by %p(%s).",victim,victim->type->name,assailant,assailant?assailant->type->name:"null");

  /* Sprite controllers may completely override this. */
  if (victim->type->hurt) {
    return victim->type->hurt(game,victim,assailant);
  }

  /* Default action for fragile sprites. */
  if (ps_sprite_kill_later(victim,game)<0) return -1;
  if (ps_game_decorate_monster_death(game,victim->x,victim->y)<0) return -1;
  ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,victim);
  if (ps_game_report_kill(game,assailant,victim)<0) return -1;

  return 0;
}

/* Master.
 */

int ps_sprite_set_master(struct ps_sprite *slave,struct ps_sprite *master,struct ps_game *game) {
  if (!slave) return -1;
  if (master) {
    if (!slave->master&&!(slave->master=ps_sprgrp_new())) return -1;
    if (slave->master->sprc>0) {
      if (slave->master->sprv[0]==master) return 0;
      if (ps_sprite_release_from_master(slave,game)<0) return -1;
    }
    if (ps_sprgrp_clear(slave->master)<0) return -1;
    if (ps_sprgrp_add_sprite(slave->master,master)<0) return -1;
  } else {
    ps_sprgrp_clear(slave->master);
  }
  return 0;
}

struct ps_sprite *ps_sprite_get_master(const struct ps_sprite *slave) {
  if (!slave) return 0;
  if (!slave->master) return 0;
  if (slave->master->sprc<1) return 0;
  return slave->master->sprv[0];
}

int ps_sprite_release_from_master(struct ps_sprite *slave,struct ps_game *game) {
  if (!slave) return -1;
  if (!slave->master) return 0;
  if (slave->master->sprc<1) return 0;
  struct ps_sprite *master=slave->master->sprv[0];

  if (master->type==&ps_sprtype_turtle) {
    if (ps_sprite_turtle_drop_slave(master,game)<0) return -1;
    return 0;
  }

  if (master->type==&ps_sprtype_rabbit) {
    if (ps_sprite_rabbit_drop_slave(master,game)<0) return -1;
    return 0;
  }

  if (master->type==&ps_sprtype_hookshot) {
    if (ps_sprite_hookshot_drop_slave(master,slave,game)<0) return -1;
    return 0;
  }

  if (master->type==&ps_sprtype_elefence) {
    if (ps_sprite_elefence_drop_slave(master,slave,game)<0) return -1;
    return 0;
  }

  ps_log(GAME,ERROR,"ps_sprite_release_from_master() is not aware of type '%s'.",master->type->name);
  return -1;
}

/* Legalize position.
 * We only care about the grid, and only the single cell principally occupied by this sprite.
 * This is for cases where a sprite's impassable mask has changed.
 * Rely on physics in the near future to push the sprite to a fully-legal position; that's way beyond our scope.
 */
 
int ps_sprite_attempt_legal_position(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||!game) return -1;
  if (!game->grid) return 0;

  int x=spr->x,y=spr->y;  
  if (x<0) return 0;
  if (y<0) return 0;
  int col=x/PS_TILESIZE;
  if (col>=PS_GRID_COLC) return 0;
  int row=y/PS_TILESIZE;
  if (row>=PS_GRID_ROWC) return 0;
  
  uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
  if (!(spr->impassable&(1<<physics))) return 0;

  /* Describe the 4 movement candidates. */
  const int margin=2;
  struct ps_legalization_candidate {
    int distance;
    int dstx,dsty;
    int col,row;
    uint8_t physics;
  } candidatev[4]={
    {y%PS_TILESIZE,x,row*PS_TILESIZE-margin,col,row-1},
    {x%PS_TILESIZE,col*PS_TILESIZE-margin,y,col-1,row},
    {PS_TILESIZE-y%PS_TILESIZE,x,(row+1)*PS_TILESIZE+margin,col,row+1},
    {PS_TILESIZE-x%PS_TILESIZE,(col+1)*PS_TILESIZE+margin,y,col+1,row},
  };

  /* Eliminate anything offscreen or impassable by raising its distance. */
  struct ps_legalization_candidate *candidate=candidatev;
  int i; for (i=0;i<4;i++,candidate++) {
    if ((candidate->col<0)||(candidate->row<0)||(candidate->col>=PS_GRID_COLC)||(candidate->row>=PS_GRID_ROWC)) {
      candidate->distance=INT_MAX;
    } else {
      candidate->physics=game->grid->cellv[candidate->row*PS_GRID_COLC+candidate->col].physics;
      if (spr->impassable&(1<<candidate->physics)) {
        candidate->distance=INT_MAX;
      }
    }
  }

  /* Select the lowest distance. It is possible that nothing remains legal. */
  int dstx,dsty;
  int best_distance=INT_MAX;
  for (i=0,candidate=candidatev;i<4;i++,candidate++) {
    if (candidate->distance<best_distance) {
      best_distance=candidate->distance;
      dstx=candidate->dstx;
      dsty=candidate->dsty;
    }
  }
  if (best_distance>PS_TILESIZE) return 0;

  ps_log(GAME,DEBUG,"Correcting sprite position from (%d,%d) to (%d,%d) to avoid cell 0x%02x.",x,y,dstx,dsty,physics);

  spr->x=dstx;
  spr->y=dsty;

  return 0;
}

/* Actuate a switch.
 */

static int ps_sprite_supports_actuation(const struct ps_sprite *spr) {
  if (spr->switchid<1) return 0;
  if (spr->type==&ps_sprtype_switch) return 1;
  if (spr->type==&ps_sprtype_swordswitch) return 1;
  if (spr->type==&ps_sprtype_multipiston) return 1;
  if (spr->type==&ps_sprtype_bullseye) return 1;
  // piston and motionsensor are timebound, so they do not support actuation this way.
  return 0;
}

int ps_sprite_actuate(struct ps_sprite *spr,struct ps_game *game,int value) {
  if (!spr||!game) return -1;
  if (!ps_sprite_supports_actuation(spr)) return 0;
  int switchid=spr->switchid;
  if (switchid<1) return 0;
  if (value<0) value=!ps_game_get_switch(game,switchid);
  if (ps_game_set_switch(game,switchid,value)<0) return -1;
  return 1;
}

/* SWORDAWARE
 */
 
int ps_sprite_react_to_sword(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero,int state) {
  if (!spr||!game) return -1;

  /* prize */
  if (spr->type==&ps_sprtype_prize) {
    if (state==1) {
      if (hero&&(hero->type==&ps_sprtype_hero)) {
        int dir=((struct ps_sprite_hero*)hero)->facedir;
        if (ps_prize_fling(spr,dir)<0) return -1;
      }
    }

  /* swordswitch */
  } else if (spr->type==&ps_sprtype_swordswitch) {
    if (state==1) {
      if (ps_swordswitch_activate(spr,game,hero)<0) return -1;
      if (hero&&(hero->type==&ps_sprtype_hero)) {
        struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)hero;
        if (HERO->player&&(HERO->player->playerid>=1)&&(HERO->player->playerid<=PS_PLAYER_LIMIT)) {
          struct ps_stats_player *pstats=game->stats->playerv+HERO->player->playerid-1;
          pstats->switchc++;
        }
      }
    }

  /* inert */
  } else if (spr->type==&ps_sprtype_inert) {
    if (state==1) {
      if (hero&&(hero->type==&ps_sprtype_hero)) {
        int dir=((struct ps_sprite_hero*)hero)->facedir;
        if (ps_sprite_inert_fling(spr,game,dir)<0) return -1;
      }
    }
    
  } else {
    ps_log(GAME,WARNING,"ps_sprite_react_to_sword on sprite of type '%s', no registered behavior.",spr->type->name);
  }
  return 0;
}
