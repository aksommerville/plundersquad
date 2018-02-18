#include "ps.h"
#include "ps_sprite.h"
#include "ps_sound_effects.h"
#include "akgl/akgl.h"
#include "ps_game.h"

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
  if (ps_game_decorate_monster_death(game,victim->x,victim->y)<0) return -1;
  ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,victim);
  if (ps_sprite_kill_later(victim,game)<0) return -1;
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

  ps_log(GAME,ERROR,"ps_sprite_release_from_master() is not aware of type '%s'.",master->type->name);
  return -1;
}
