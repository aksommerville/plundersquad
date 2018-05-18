/* ps_sprite_teleporter.h
 * Waits for a sprite to come in range, then zaps it over to another matching teleporter.
 * This is actually three sprites: base, zapper, and bell.
 * We use different sprites and not just tiles, because they want to be on different layers.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"

#define PS_TELEPORTER_ROLE_INIT    0 /* Special flag telling update() to create two more sprites and assign roles. */
#define PS_TELEPORTER_ROLE_BASE    1 /* The main one. */
#define PS_TELEPORTER_ROLE_BELL    2
#define PS_TELEPORTER_ROLE_ZAPPER  3

struct ps_sprite *ps_teleffect_new(struct ps_game *game,double srcx,double srcy,double dstx,double dsty);

/* Private sprite object.
 */

struct ps_sprite_teleporter {
  struct ps_sprite hdr;
  int telelinkid; // When we zap a sprite, we send it to any teleporter with the same telelinkid. Hopefully there are only two.
  int role;
  int active;
  int animtime;
  int animframe;
  int hold; // If nonzero, wait to become clear before teleporting anything.
  struct ps_sprite *owner_do_not_dereference;
};

#define SPR ((struct ps_sprite_teleporter*)spr)

/* Delete.
 */

static void _ps_teleporter_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_teleporter_init(struct ps_sprite *spr) {
  SPR->role=PS_TELEPORTER_ROLE_INIT;
  SPR->telelinkid=0;
  SPR->active=0;
  return 0;
}

/* Configure.
 */

static int _ps_teleporter_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    SPR->telelinkid=argv[0];
    if (argc>=2) {
      spr->switchid=argv[1];
      if (argc>=3) {
        // If you start a teleporter active, make sure it's not attached to a switch.
        SPR->active=argv[2];
      }
    }
  }
  return 0;
}

static const char *_ps_teleporter_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "telelinkid";
    case 1: return "switchid";
    case 2: return "active";
  }
  return 0;
}

/* Deferred initialization.
 */

static int ps_teleporter_create_peers(struct ps_sprite *spr,struct ps_game *game) {

  /* If switchid was not set, start it off active. */
  if (!spr->switchid) SPR->active=1;

  struct ps_sprite *bell=ps_sprite_new(&ps_sprtype_teleporter);
  if (!bell) return -1;
  if (ps_game_set_group_mask_for_sprite(game,bell,(1<<PS_SPRGRP_KEEPALIVE)|(1<<PS_SPRGRP_VISIBLE))<0) {
    ps_sprite_del(bell);
    return -1;
  }
  ps_sprite_del(bell);
  struct ps_sprite_teleporter *BELL=(struct ps_sprite_teleporter*)bell;

  struct ps_sprite *zapper=ps_sprite_new(&ps_sprtype_teleporter);
  if (!zapper) return -1;
  if (ps_game_set_group_mask_for_sprite(game,zapper,(1<<PS_SPRGRP_KEEPALIVE)|(1<<PS_SPRGRP_VISIBLE)|(1<<PS_SPRGRP_UPDATE))<0) {
    ps_sprite_del(zapper);
    return -1;
  }
  ps_sprite_del(zapper);
  struct ps_sprite_teleporter *ZAPPER=(struct ps_sprite_teleporter*)zapper;

  BELL->telelinkid=ZAPPER->telelinkid=SPR->telelinkid;
  BELL->active=ZAPPER->active=SPR->active;

  BELL->role=PS_TELEPORTER_ROLE_BELL;
  ZAPPER->role=PS_TELEPORTER_ROLE_ZAPPER;

  bell->layer=110;
  zapper->layer=100;

  bell->tileid=spr->tileid-0x10;
  zapper->tileid=spr->tileid-0x11;
  bell->tsid=zapper->tsid=spr->tsid;

  bell->x=zapper->x=spr->x;
  bell->y=spr->y-PS_TILESIZE;
  zapper->y=spr->y-6;

  BELL->owner_do_not_dereference=spr;
  ZAPPER->owner_do_not_dereference=spr;

  SPR->role=PS_TELEPORTER_ROLE_BASE;

  return 0;
}

/* Find the partner for a BASE teleporter.
 */

static struct ps_sprite *ps_teleporter_find_partner(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_BARRIER;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *partner=grp->sprv[i];
    if (partner->type!=&ps_sprtype_teleporter) continue;
    if (spr==partner) continue;
    struct ps_sprite_teleporter *PARTNER=(struct ps_sprite_teleporter*)partner;
    if (PARTNER->role!=PS_TELEPORTER_ROLE_BASE) continue;
    if (PARTNER->telelinkid!=SPR->telelinkid) continue;

    // Skip partners that aren't ready to accept a pumpkin.
    if (!PARTNER->active) continue;
    if (PARTNER->hold) continue;

    return partner;
  }
  return 0;
}

/* Create special visual effect while teleporting something.
 */

static int ps_teleporter_create_visual_effect(struct ps_game *game,double srcx,double srcy,double dstx,double dsty) {
  struct ps_sprite *teleffect=ps_teleffect_new(game,srcx,srcy,dstx,dsty);
  return 0;
}

/* Teleport something.
 */

static int ps_teleporter_teleport(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *pumpkin) {
  if (SPR->hold) return 0;
  struct ps_sprite *dst=ps_teleporter_find_partner(spr,game);
  if (!dst) return 0;
  struct ps_sprite_teleporter *DST=(struct ps_sprite_teleporter*)dst;
  if (!DST->active) return 0;
  if (DST->hold) return 0;
  PS_SFX_TELEPORT
  pumpkin->x=dst->x;
  pumpkin->y=dst->y;
  DST->hold=1;
  if (ps_teleporter_create_visual_effect(game,spr->x,spr->y,dst->x,dst->y)<0) return -1;
  return 0;
}

/* Look for teleportation.
 */

static int ps_teleporter_check_pumpkins(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->active) return 0;

  double west=spr->x-6.0;
  double east=spr->x+6.0;
  double north=spr->y-10.0;
  double south=spr->y+2.0;
  
  int pumpkinc=0;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_TELEPORT;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=grp->sprv[i];
    if (pumpkin->x<west) continue;
    if (pumpkin->x>east) continue;
    if (pumpkin->y<north) continue;
    if (pumpkin->y>south) continue;
    if (ps_sprites_collide(spr,pumpkin)) {
      if (ps_teleporter_teleport(spr,game,pumpkin)<0) return -1;
      pumpkinc++;
    }
  }
  if (!pumpkinc) {
    SPR->hold=0;
  }
  return 0;
}

/* Animate zapper.
 */

static int ps_teleporter_animate_zapper(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->active) {

    if (SPR->animtime<0) {
      if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_VISIBLE,spr)<0) return -1;
      SPR->animtime=0;
    }

    if (++(SPR->animtime)>=6) {
      SPR->animtime=0;
      if (++(SPR->animframe)>1) {
        SPR->animframe=0;
        spr->tileid-=0x10;
      } else {
        spr->tileid+=0x10;
      }
    }
    
  } else {
    if (SPR->animtime>=0) {
      if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_VISIBLE,spr)<0) return -1;
      SPR->animtime=-1;
    }
  }
  return 0;
}

/* Update.
 */

static int _ps_teleporter_update(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->role) {
    case PS_TELEPORTER_ROLE_INIT: return ps_teleporter_create_peers(spr,game);
    case PS_TELEPORTER_ROLE_BASE: return ps_teleporter_check_pumpkins(spr,game);
    case PS_TELEPORTER_ROLE_ZAPPER: return ps_teleporter_animate_zapper(spr,game);
    default: return ps_sprite_kill_later(spr,game);
  }
  return 0;
}

/* Set switch.
 */

static int _ps_teleporter_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {

  SPR->active=value;
  SPR->hold=0;

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_KEEPALIVE; // Unfortunately, KEEPALIVE is the only group guaranteed to have all the pieces in it.
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *peer=grp->sprv[i];
    if (peer->type!=&ps_sprtype_teleporter) continue;
    struct ps_sprite_teleporter *PEER=(struct ps_sprite_teleporter*)peer;
    if (PEER->owner_do_not_dereference==spr) {
      PEER->active=SPR->active;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_teleporter={
  .name="teleporter",
  .objlen=sizeof(struct ps_sprite_teleporter),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=10,

  .init=_ps_teleporter_init,
  .del=_ps_teleporter_del,
  .configure=_ps_teleporter_configure,
  .get_configure_argument_name=_ps_teleporter_get_configure_argument_name,
  .update=_ps_teleporter_update,
  .set_switch=_ps_teleporter_set_switch,

};
