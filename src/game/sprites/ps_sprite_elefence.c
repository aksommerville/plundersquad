#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "akgl/akgl.h"

#define PS_ELEFENCE_FRAME_TIME     9
#define PS_ELEFENCE_FRAME_COUNT    4
#define PS_ELEFENCE_HEAD_OFFSET_X  7
#define PS_ELEFENCE_HEAD_OFFSET_Y -6
#define PS_ELEFENCE_WALK_SPEED     0.8
#define PS_ELEFENCE_EPSILON        0.4
#define PS_ELEFENCE_TURN_TIME      10

#define PS_ELEFENCE_ROLE_UNSET    0 /* At init only; decide whether to become master or slave. */
#define PS_ELEFENCE_ROLE_MASTER   1 /* Controls a herd. */
#define PS_ELEFENCE_ROLE_SLAVE    2 /* Controlled by master (general 'master' must be set). */
#define PS_ELEFENCE_ROLE_COWBOY   3 /* Last of its herd, self-controlled. */

/* Private sprite object.
 */

struct ps_sprite_elefence {
  struct ps_sprite hdr;
  int animtime;
  int animframe;
  int facedx;
  int role;
  int homey; // If I stray from this, move back to it. MASTER,SLAVE
  int wallw,walle; // Firm limits of movement left and right. MASTER
  struct ps_sprgrp *slaves; // MASTER
  double pvx; // COWBOY
  int delay;
};

#define SPR ((struct ps_sprite_elefence*)spr)

/* Delete.
 */

static void _ps_elefence_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->slaves);
  ps_sprgrp_del(SPR->slaves);
}

/* Initialize.
 */

static int _ps_elefence_init(struct ps_sprite *spr) {
  SPR->facedx=(rand()&1)?-1:1;
  SPR->role=PS_ELEFENCE_ROLE_UNSET;
  return 0;
}

/* Assume MASTER role if I haven't already.
 */
 
static int ps_elefence_become_master(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->role==PS_ELEFENCE_ROLE_MASTER) return 0;
  if (!SPR->slaves) {
    if (!(SPR->slaves=ps_sprgrp_new())) return -1;
    SPR->slaves->order=PS_SPRGRP_ORDER_EXPLICIT;
  }
  SPR->homey=spr->y;
  SPR->role=PS_ELEFENCE_ROLE_MASTER;
  
  SPR->wallw=0;
  SPR->walle=PS_SCREENW;
  if (game&&game->grid) {
    int colmid=spr->x/PS_TILESIZE;
    if ((colmid>=0)&&(colmid<PS_GRID_COLC)) {
      int row=spr->y/PS_TILESIZE;
      if ((row>=0)&&(row<PS_GRID_ROWC)) {
        const struct ps_grid_cell *cellv=game->grid->cellv+row*PS_GRID_COLC;
        int x;
        for (x=colmid;(x>=0)&&!(spr->impassable&(1<<cellv[x].physics));x--) ;
        SPR->wallw=x*PS_TILESIZE+PS_TILESIZE;
        for (x=colmid;(x<PS_GRID_COLC)&&!(spr->impassable&(1<<cellv[x].physics));x++) ;
        SPR->walle=x*PS_TILESIZE;
        if (SPR->wallw>=SPR->walle) {
          SPR->wallw=spr->x-PS_TILESIZE;
          SPR->walle=spr->x+PS_TILESIZE;
        }
      }
    }
  }
  
  return 0;
}

/* Attach (spr) to (master) and assign its role.
 */
 
static int ps_elefence_attach_herd_member(struct ps_sprite *master,struct ps_sprite *spr,struct ps_game *game) {
  if (!master||(master->type!=&ps_sprtype_elefence)) return -1;
  struct ps_sprite_elefence *MASTER=(struct ps_sprite_elefence*)master;
  if (MASTER->role!=PS_ELEFENCE_ROLE_MASTER) return -1;
  if (ps_sprgrp_add_sprite(MASTER->slaves,spr)<0) return -1;
  if (ps_sprite_set_master(spr,master,game)<0) return -1;
  SPR->role=PS_ELEFENCE_ROLE_SLAVE;
  SPR->homey=MASTER->homey;
  return 0;
}

/* Select role at init.
 */
 
static int ps_elefence_select_role(struct ps_sprite *spr,struct ps_game *game) {
  SPR->role=PS_ELEFENCE_ROLE_UNSET;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *peer=grp->sprv[i];
    if (peer==spr) continue;
    if (peer->type!=&ps_sprtype_elefence) continue;
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_DEATHROW,peer)) continue;
    if (peer->y<spr->y-1.0) continue;
    if (peer->y>spr->y+1.0) continue;
    struct ps_sprite_elefence *PEER=(struct ps_sprite_elefence*)peer;
    if ((PEER->role==PS_ELEFENCE_ROLE_MASTER)&&(SPR->role==PS_ELEFENCE_ROLE_UNSET)) {
      return ps_elefence_attach_herd_member(peer,spr,game);
    } else if (PEER->role==PS_ELEFENCE_ROLE_UNSET) {
      if (ps_elefence_become_master(spr,game)<0) return -1;
      if (ps_elefence_attach_herd_member(spr,peer,game)<0) return -1;
    }
  }
  if (SPR->role==PS_ELEFENCE_ROLE_UNSET) {
    SPR->role=PS_ELEFENCE_ROLE_COWBOY;
  }
  return 0;
}

/* Sort slaves by (x).
 */
 
static int ps_elefence_sort_slaves_horizontally(struct ps_sprite *spr) {
  if (!SPR->slaves) return -1;
  int lo=0,hi=SPR->slaves->sprc-1,d=1;
  while (lo<hi) {
    int first,last,i,done=1;
    if (d==1) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      struct ps_sprite *a=SPR->slaves->sprv[i];
      struct ps_sprite *b=SPR->slaves->sprv[i+d];
      int cmp;
      if (a->x<b->x) cmp=-1;
      else cmp=1;
      if (cmp==d) {
        SPR->slaves->sprv[i]=b;
        SPR->slaves->sprv[i+d]=a;
        done=0;
      }
    }
    if (done) break;
    if (d==1) { hi--; d=-1; }
    else { lo++; d=1; }
  }
  return 0;
}

/* Randomize speed to keep things from getting too machiney.
 */
 
static double ps_elefence_speed() {
  return PS_ELEFENCE_WALK_SPEED+((rand()%200-100)/1000.0);
}

/* Update master.
 */
 
static int ps_elefence_update_master(struct ps_sprite *spr,struct ps_game *game) {

  /* If my herd is depleted, become a cowboy. */
  if (!SPR->slaves||!SPR->slaves->sprc) {
    SPR->role=PS_ELEFENCE_ROLE_COWBOY;
    return 0;
  }
  
  /* Set direction of my slaves and myself. */
  int doneself=0;
  int spacing=(SPR->walle-SPR->wallw)/(SPR->slaves->sprc+1);
  int tolerance=spacing/5;
  if (ps_elefence_sort_slaves_horizontally(spr)<0) return -1;
  int x=SPR->wallw+(spacing>>1);
  int i=0; for (;i<SPR->slaves->sprc;i++,x+=spacing) {
    struct ps_sprite *slave=SPR->slaves->sprv[i];
    
    // Insert this extra iteration to place myself.
    if (!doneself&&(slave->x>spr->x)) {
      if (spr->x<x-tolerance) {
        if ((SPR->facedx<0)&&!SPR->delay) {
          SPR->delay=PS_ELEFENCE_TURN_TIME;
        }
      } else if (spr->x>x+tolerance) {
        if ((SPR->facedx>0)&&!SPR->delay) {
          SPR->delay=PS_ELEFENCE_TURN_TIME;
        }
      }
      x+=spacing;
      doneself=1;
    }
    
    struct ps_sprite_elefence *SLAVE=(struct ps_sprite_elefence*)slave;
    if (slave->x<x-tolerance) {
      if ((SLAVE->facedx<0)&&!SPR->delay) {
        SLAVE->delay=PS_ELEFENCE_TURN_TIME;
      }
    } else if (slave->x>x+tolerance) {
      if ((SLAVE->facedx>0)&&!SPR->delay) {
        SLAVE->delay=PS_ELEFENCE_TURN_TIME;
      }
    }
  }
  if (!doneself) {
    if (spr->x<x-tolerance) {
      if ((SPR->facedx<0)&&!SPR->delay) {
        SPR->delay=PS_ELEFENCE_TURN_TIME;
      }
    } else if (spr->x>x+tolerance) {
      if ((SPR->facedx>0)&&!SPR->delay) {
        SPR->delay=PS_ELEFENCE_TURN_TIME;
      }
    }
  }

  if (SPR->delay>0) {
    SPR->delay--;
    SPR->animframe=0;
    if (!SPR->delay) {
      SPR->facedx=-SPR->facedx;
    } else {
      return 0;
    }
  }

  if (SPR->facedx<0) {
    spr->x-=ps_elefence_speed();
  } else {
    spr->x+=ps_elefence_speed();
  }
  
  if (spr->y<SPR->homey) {
    if ((spr->y+=PS_ELEFENCE_WALK_SPEED)>SPR->homey) spr->y=SPR->homey;
  } else if (spr->y>SPR->homey) {
    if ((spr->y-=PS_ELEFENCE_WALK_SPEED)<SPR->homey) spr->y=SPR->homey;
  }

  return 0;
}

/* Update slave.
 */
 
static int ps_elefence_update_slave(struct ps_sprite *spr,struct ps_game *game) {

  /* If my master vanished, drop role.
   * Don't become COWBOY right away: There might be other orphans.
   * Then one of us becomes MASTER at next update.
   */
  if (!spr->master||!spr->master->sprc) {
    SPR->role=PS_ELEFENCE_ROLE_UNSET;
    return 0;
  }

  if (SPR->delay>0) {
    SPR->delay--;
    SPR->animframe=0;
    if (!SPR->delay) {
      SPR->facedx=-SPR->facedx;
    } else {
      return 0;
    }
  }

  if (SPR->facedx<0) {
    spr->x-=ps_elefence_speed();
  } else {
    spr->x+=ps_elefence_speed();
  }
  
  if (spr->y<SPR->homey) {
    if ((spr->y+=PS_ELEFENCE_WALK_SPEED)>SPR->homey) spr->y=SPR->homey;
  } else if (spr->y>SPR->homey) {
    if ((spr->y-=PS_ELEFENCE_WALK_SPEED)<SPR->homey) spr->y=SPR->homey;
  }
  
  return 0;
}

/* Update cowboy.
 */
 
static int ps_elefence_update_cowboy(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->delay>0) {
    SPR->delay--;
    SPR->animframe=0;
    if (!SPR->delay) {
      SPR->facedx=-SPR->facedx;
      SPR->pvx=spr->x-10.0*SPR->facedx;
    }
    return 0;
  }

  double dx=spr->x-SPR->pvx;
  if ((dx>=-PS_ELEFENCE_EPSILON)&&(dx<=PS_ELEFENCE_EPSILON)) {
    SPR->delay=PS_ELEFENCE_TURN_TIME;
    return 0;
  } else if ((SPR->facedx>0)&&(dx<0.0)) {
    SPR->delay=PS_ELEFENCE_TURN_TIME;
    return 0;
  } else if ((SPR->facedx<0)&&(dx>0.0)) {
    SPR->delay=PS_ELEFENCE_TURN_TIME;
    return 0;
  }

  SPR->pvx=spr->x;
  if (SPR->facedx<0) {
    spr->x-=PS_ELEFENCE_WALK_SPEED;
  } else {
    spr->x+=PS_ELEFENCE_WALK_SPEED;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_elefence_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animtime)>=PS_ELEFENCE_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_ELEFENCE_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }
  
  switch (SPR->role) {
    case PS_ELEFENCE_ROLE_MASTER: return ps_elefence_update_master(spr,game);
    case PS_ELEFENCE_ROLE_SLAVE: return ps_elefence_update_slave(spr,game);
    case PS_ELEFENCE_ROLE_COWBOY: return ps_elefence_update_cowboy(spr,game);
    default: return ps_elefence_select_role(spr,game);
  }
}

/* Draw.
 */

static int _ps_elefence_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<2) return 2;
  struct akgl_vtx_maxtile *body=vtxv+0;
  struct akgl_vtx_maxtile *head=vtxv+1;
  
  body->x=spr->x;
  body->y=spr->y;
  body->size=PS_TILESIZE;
  body->tileid=spr->tileid;
  body->pr=body->pg=body->pb=0x80;
  body->ta=0;
  body->a=0xff;
  body->t=0;
  body->xform=AKGL_XFORM_NONE;
  memcpy(head,body,sizeof(struct akgl_vtx_maxtile));
  
  head->tileid=spr->tileid+0x10;
  head->x+=PS_ELEFENCE_HEAD_OFFSET_X*SPR->facedx;
  head->y+=PS_ELEFENCE_HEAD_OFFSET_Y;
  if (SPR->facedx>0) {
    head->xform=AKGL_XFORM_FLOP;
  }
  
  switch (SPR->animframe) {
    case 0: break;
    case 1: body->tileid++; break;
    case 2: break;
    case 3: body->tileid++; body->xform=AKGL_XFORM_FLOP; break;
  }
  
  return 2;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_elefence={
  .name="elefence",
  .objlen=sizeof(struct ps_sprite_elefence),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_elefence_init,
  .del=_ps_elefence_del,
  .update=_ps_elefence_update,
  .draw=_ps_elefence_draw,
  
};
