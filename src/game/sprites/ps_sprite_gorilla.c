#include "ps.h"
#include "ps_sprite_hookshot.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "res/ps_resmgr.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

/* The gorilla's body is symmetric.
 * It has two independent states.
 */
#define PS_GORILLA_BODY_STATE_IDLE     0 /* Normal state. */
#define PS_GORILLA_BODY_STATE_FIST     1 /* Fist in the air (3 tiles). */
#define PS_GORILLA_BODY_STATE_WALK     2 /* Foot raised. */
#define PS_GORILLA_BODY_STATE_PUNCH    3 /* Fist to the side (3 tiles). */

#define PS_GORILLA_HEAD_STATE_IDLE    0
#define PS_GORILLA_HEAD_STATE_HOWL    1

#define PS_GORILLA_PHASE_IDLE      0
#define PS_GORILLA_PHASE_WALK      1
#define PS_GORILLA_PHASE_HOWL      2
#define PS_GORILLA_PHASE_SPIT      3
#define PS_GORILLA_PHASE_PUNCH     4

#define PS_GORILLA_IDLE_TIME_MIN     30
#define PS_GORILLA_IDLE_TIME_MAX     50
#define PS_GORILLA_WALK_TIME_MIN     30
#define PS_GORILLA_WALK_TIME_MAX    130
#define PS_GORILLA_HOWL_TIME_MIN     90
#define PS_GORILLA_HOWL_TIME_MAX    140
#define PS_GORILLA_SPIT_TIME         90
#define PS_GORILLA_PUNCH_TIME        60

#define PS_GORILLA_WALK_SPEED 0.5

#define PS_GORILLA_WALK_FRAME_TIME  7
#define PS_GORILLA_WALK_FRAME_COUNT 4

#define PS_GORILLA_HOWL_FRAME_TIME 15
#define PS_GORILLA_HOWL_FRAME_COUNT 4

#define PS_BANANA_SPRDEF_ID 35

#define PS_GORILLA_INVINCIBLE_TIME 60

/* Private sprite object.
 */

struct ps_sprite_gorilla {
  struct ps_sprite hdr;

  int phase;
  int phasetime;
  int animtime;
  int animframe;

  // See STATE constants above. These are outputs from the controller and inputs to draw().
  int state_left;
  int state_right;
  int state_head;

  double walkdx;
  double walkdy;

  int hp;
  int invincible;
  
};

#define SPR ((struct ps_sprite_gorilla*)spr)

/* Delete.
 */

static void _ps_gorilla_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_gorilla_init(struct ps_sprite *spr) {

  SPR->phase=PS_GORILLA_PHASE_IDLE;
  SPR->phasetime=PS_GORILLA_IDLE_TIME_MIN+rand()%(PS_GORILLA_IDLE_TIME_MAX-PS_GORILLA_IDLE_TIME_MIN);
  SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
  SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
  SPR->state_head=PS_GORILLA_HEAD_STATE_IDLE;
  SPR->hp=5;

  return 0;
}

/* Configure.
 */

static int _ps_gorilla_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_gorilla_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_gorilla_configure(), for editor.
  return 0;
}

/* Enter IDLE phase.
 */

static int ps_gorilla_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GORILLA_PHASE_IDLE;
  SPR->phasetime=PS_GORILLA_IDLE_TIME_MIN+rand()%(PS_GORILLA_IDLE_TIME_MAX-PS_GORILLA_IDLE_TIME_MIN);
  SPR->state_head=PS_GORILLA_HEAD_STATE_IDLE;
  SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
  SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
  //ps_log(GAME,DEBUG,"gorilla IDLE %d",SPR->phasetime);
  return 0;
}

/* Enter WALK phase.
 */

static int ps_gorilla_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GORILLA_PHASE_WALK;
  SPR->phasetime=PS_GORILLA_WALK_TIME_MIN+rand()%(PS_GORILLA_WALK_TIME_MAX-PS_GORILLA_WALK_TIME_MIN);
  SPR->animtime=0;
  SPR->animframe=0;
  SPR->state_head=PS_GORILLA_HEAD_STATE_IDLE;
  SPR->state_left=PS_GORILLA_BODY_STATE_WALK;
  SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
  //ps_log(GAME,DEBUG,"gorilla WALK %d",SPR->phasetime);

  double t=(rand()%6282)/1000.0;
  SPR->walkdx=cos(t)*PS_GORILLA_WALK_SPEED;
  SPR->walkdy=-sin(t)*PS_GORILLA_WALK_SPEED;

  const int xmargin=(PS_TILESIZE*3);
  const int ymargin=(PS_TILESIZE*2);
  if (spr->x<xmargin) {
    if (SPR->walkdx<0.0) SPR->walkdx=-SPR->walkdx;
  } else if (spr->x>PS_SCREENW-xmargin) {
    if (SPR->walkdx>0.0) SPR->walkdx=-SPR->walkdx;
  }
  if (spr->y<ymargin) {
    if (SPR->walkdy<0.0) SPR->walkdy=-SPR->walkdy;
  } else if (spr->y>PS_SCREENH-ymargin) {
    if (SPR->walkdy>0.0) SPR->walkdy=-SPR->walkdy;
  }

  return 0;
}

/* Enter HOWL phase.
 */

static int ps_gorilla_begin_HOWL(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GORILLA_PHASE_HOWL;
  SPR->phasetime=PS_GORILLA_HOWL_TIME_MIN+rand()%(PS_GORILLA_HOWL_TIME_MAX-PS_GORILLA_HOWL_TIME_MIN);
  SPR->animtime=0;
  SPR->animframe=0;
  SPR->state_head=PS_GORILLA_HEAD_STATE_HOWL;
  SPR->state_left=PS_GORILLA_BODY_STATE_FIST;
  SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
  //ps_log(GAME,DEBUG,"gorilla HOWL %d",SPR->phasetime);
  return 0;
}

/* Enter SPIT phase.
 */

static int ps_gorilla_begin_SPIT(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_GORILLA_PHASE_SPIT;
  SPR->phasetime=PS_GORILLA_SPIT_TIME;
  SPR->animtime=0;
  SPR->animframe=0;
  SPR->state_head=PS_GORILLA_HEAD_STATE_IDLE;
  SPR->state_left=PS_GORILLA_BODY_STATE_FIST;
  SPR->state_right=PS_GORILLA_BODY_STATE_FIST;
  //ps_log(GAME,DEBUG,"gorilla SPIT %d",SPR->phasetime);
  return 0;
}

/* Look for fragile sprites and hurt them.
 */

static int ps_gorilla_hurt_victims(struct ps_sprite *spr,struct ps_game *game,int x,int y) {
  int w=(PS_TILESIZE>>1);
  int h=(PS_TILESIZE>>1);
  struct ps_fbox hurtbox=ps_fbox(x-(w>>1),x+(w>>1),y-(h>>1),y+(h>>1));
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (ps_sprite_collide_fbox(victim,&hurtbox)) {
      if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
    }
  }
  return 0;
}

/* Enter PUNCH phase.
 */

static int ps_gorilla_begin_PUNCH(struct ps_sprite *spr,struct ps_game *game,int dx) {
  SPR->phase=PS_GORILLA_PHASE_PUNCH;
  SPR->phasetime=PS_GORILLA_PUNCH_TIME;
  SPR->animtime=0;
  SPR->animframe=0;
  SPR->state_head=PS_GORILLA_HEAD_STATE_HOWL;
  if (dx<0) {
    SPR->state_left=PS_GORILLA_BODY_STATE_PUNCH;
    SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
  } else {
    SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
    SPR->state_right=PS_GORILLA_BODY_STATE_PUNCH;
  }
  if (ps_gorilla_hurt_victims(spr,game,spr->x+dx*((PS_TILESIZE*3)/2),spr->y)<0) return -1;
  return 0;
}

/* Update, IDLE phase.
 */

static int ps_gorilla_update_IDLE(struct ps_sprite *spr,struct ps_game *game) {

  if (!(SPR->phasetime%10)) { // Don't check every single frame.
    struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
    int i=grp->sprc; while (i-->0) {
      struct ps_sprite *victim=grp->sprv[i];
      if (victim->type==&ps_sprtype_flames) continue; // Don't punch the magic flame; the futility of this protest bespeaks your pathetic desperation.
      double dy=victim->y-spr->y;
      if (dy>victim->radius) continue;
      if (dy<-victim->radius) continue;
      double dx=victim->x-spr->x;
      double tolerance=spr->radius+victim->radius*2;
      if (dx>tolerance) continue;
      if (dx<-tolerance) continue;

      if (ps_gorilla_begin_PUNCH(spr,game,(dx>0.0)?1:-1)<0) return -1;
      return 0;
    }
  }
  
  return 0;
}

/* Update, WALK phase.
 */

static int ps_gorilla_update_WALK(struct ps_sprite *spr,struct ps_game *game) {

  /* Animate. */
  if (++(SPR->animtime)>=PS_GORILLA_HOWL_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_GORILLA_HOWL_FRAME_COUNT) {
      SPR->animframe=0;
    }
    switch (SPR->animframe) {
      case 1: case 3: {
          SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
          SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
        } break;
      case 0: {
          SPR->state_left=PS_GORILLA_BODY_STATE_WALK;
          SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
        } break;
      case 2: {
          SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
          SPR->state_right=PS_GORILLA_BODY_STATE_WALK;
        } break;
    }
  }

  /* Move. */
  spr->x+=SPR->walkdx;
  spr->y+=SPR->walkdy;
  
  return 0;
}

/* Update, HOWL phase.
 */

static int ps_gorilla_update_HOWL(struct ps_sprite *spr,struct ps_game *game) {
  if (++(SPR->animtime)>=PS_GORILLA_HOWL_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_GORILLA_HOWL_FRAME_COUNT) {
      SPR->animframe=0;
    }
    switch (SPR->animframe) {
      case 1: case 3: {
          SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
          SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
        } break;
      case 0: {
          SPR->state_left=PS_GORILLA_BODY_STATE_FIST;
          SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
        } break;
      case 2: {
          SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
          SPR->state_right=PS_GORILLA_BODY_STATE_FIST;
        } break;
    }
  }
  return 0;
}

/* Choose where to spit my banana.
 */

static int ps_gorilla_select_banana_destination(int *dstx,int *dsty,struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  if (grp->sprc>0) {
    int panic=10;
    while (panic-->0) {
      int sprp=rand()%grp->sprc;
      struct ps_sprite *target=grp->sprv[sprp];
      if (target->type!=&ps_sprtype_hero) continue;
      *dstx=target->x;
      *dsty=target->y;
      return 0;
    }
  }

  *dstx=rand()%PS_SCREENW;
  *dsty=rand()%PS_SCREENH;

  return 0;
}

/* Update, SPIT phase.
 */

static int ps_gorilla_update_SPIT(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->phasetime==PS_GORILLA_SPIT_TIME>>1) {
    SPR->state_left=PS_GORILLA_BODY_STATE_IDLE;
    SPR->state_right=PS_GORILLA_BODY_STATE_IDLE;
    SPR->state_head=PS_GORILLA_HEAD_STATE_HOWL;

    int dstx,dsty;
    if (ps_gorilla_select_banana_destination(&dstx,&dsty,spr,game)<0) return -1;

    struct ps_sprite *banana=ps_sprite_missile_new(PS_BANANA_SPRDEF_ID,spr,dstx,dsty,game);
    if (!banana) return -1;
    banana->y-=PS_TILESIZE;
  
  }
  return 0;
}

/* Select a new phase.
 */

static int ps_gorilla_select_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_GORILLA_PHASE_IDLE: {
        int choice=rand()%10;
        if (choice<4) {
          if (ps_gorilla_begin_WALK(spr,game)<0) return -1;
        } else if (choice<8) {
          if (ps_gorilla_begin_SPIT(spr,game)<0) return -1;
        } else {
          if (ps_gorilla_begin_HOWL(spr,game)<0) return -1;
        }
      } break;
    case PS_GORILLA_PHASE_WALK:
    case PS_GORILLA_PHASE_HOWL:
    case PS_GORILLA_PHASE_SPIT:
    default: {
        if (ps_gorilla_begin_IDLE(spr,game)<0) return -1;
      }
  }
  return 0;
}

/* Update.
 */

static int _ps_gorilla_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->invincible>0) SPR->invincible--;

  if (--(SPR->phasetime)<=0) {
    if (ps_gorilla_select_phase(spr,game)<0) return -1;
  } else switch (SPR->phase) {
    case PS_GORILLA_PHASE_IDLE: if (ps_gorilla_update_IDLE(spr,game)<0) return -1; break;
    case PS_GORILLA_PHASE_WALK: if (ps_gorilla_update_WALK(spr,game)<0) return -1; break;
    case PS_GORILLA_PHASE_HOWL: if (ps_gorilla_update_HOWL(spr,game)<0) return -1; break;
    case PS_GORILLA_PHASE_SPIT: if (ps_gorilla_update_SPIT(spr,game)<0) return -1; break;
  }
  return 0;
}

/* Draw components.
 */

static void ps_gorilla_draw_body(struct akgl_vtx_maxtile *vtxv,struct ps_sprite *spr,int state,int dx) {
  switch (state) {
  
    case PS_GORILLA_BODY_STATE_IDLE: {
        vtxv[0].x+=dx;
        vtxv[1].x+=dx;
        vtxv[0].y-=PS_TILESIZE>>1;
        vtxv[1].y+=PS_TILESIZE>>1;
        if (dx>0) {
          vtxv[0].xform=AKGL_XFORM_FLOP;
          vtxv[1].xform=AKGL_XFORM_FLOP;
        }
        vtxv[0].tileid+=0x10;
        vtxv[1].tileid+=0x20;
      } break;
      
    case PS_GORILLA_BODY_STATE_FIST: {
        vtxv[0].x+=dx;
        vtxv[1].x+=dx;
        vtxv[2].x+=dx;
        vtxv[0].y-=PS_TILESIZE+(PS_TILESIZE>>1);
        vtxv[1].y-=PS_TILESIZE>>1;
        vtxv[2].y+=PS_TILESIZE>>1;
        if (dx>0) {
          vtxv[0].xform=AKGL_XFORM_FLOP;
          vtxv[1].xform=AKGL_XFORM_FLOP;
          vtxv[2].xform=AKGL_XFORM_FLOP;
        }
        vtxv[0].tileid+=0x01;
        vtxv[1].tileid+=0x11;
        vtxv[2].tileid+=0x21;
      } break;
      
    case PS_GORILLA_BODY_STATE_PUNCH: {
        vtxv[0].x+=dx+((dx<0)?-PS_TILESIZE:PS_TILESIZE);
        vtxv[1].x+=dx;
        vtxv[2].x+=dx;
        vtxv[0].y-=PS_TILESIZE>>1;
        vtxv[1].y-=PS_TILESIZE>>1;
        vtxv[2].y+=PS_TILESIZE>>1;
        if (dx>0) {
          vtxv[0].xform=AKGL_XFORM_FLOP;
          vtxv[1].xform=AKGL_XFORM_FLOP;
          vtxv[2].xform=AKGL_XFORM_FLOP;
        }
        vtxv[0].tileid+=0x13;
        vtxv[1].tileid+=0x14;
        vtxv[2].tileid+=0x24;
      } break;
      
    case PS_GORILLA_BODY_STATE_WALK: {
        vtxv[0].x+=dx;
        vtxv[1].x+=dx;
        vtxv[0].y-=PS_TILESIZE>>1;
        vtxv[1].y+=PS_TILESIZE>>1;
        if (dx>0) {
          vtxv[0].xform=AKGL_XFORM_FLOP;
          vtxv[1].xform=AKGL_XFORM_FLOP;
        }
        vtxv[0].tileid+=0x12;
        vtxv[1].tileid+=0x22;
      } break;
  }
}

static void ps_gorilla_draw_head(struct akgl_vtx_maxtile *vtxv,struct ps_sprite *spr) {
  vtxv->y-=PS_TILESIZE;
  switch (SPR->state_head) {
    case PS_GORILLA_HEAD_STATE_IDLE: {
      } break;
    case PS_GORILLA_HEAD_STATE_HOWL: {
        vtxv->tileid+=0x23;
      } break;
  }
}

/* Draw, main entry point.
 */

static int _ps_gorilla_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int vtxc=5; // Typically 2 tiles for each body half, plus 1 for the head.
  if (SPR->state_left==PS_GORILLA_BODY_STATE_FIST) vtxc++;
  else if (SPR->state_left==PS_GORILLA_BODY_STATE_PUNCH) vtxc++;
  if (SPR->state_right==PS_GORILLA_BODY_STATE_FIST) vtxc++;
  else if (SPR->state_right==PS_GORILLA_BODY_STATE_PUNCH) vtxc++;
  if (vtxc>vtxa) return vtxc;

  /* Fill in defaults and copy to all. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  if (SPR->invincible) {
    vtxv->tr=0xff;
    vtxv->tg=0x00;
    vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*255)/PS_GORILLA_INVINCIBLE_TIME;
  }
  int i=vtxc; while (i-->1) memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));

  /* Select pointers to the 3 components. */
  struct akgl_vtx_maxtile *vtxv_left,*vtxv_right,*vtxv_head;
  vtxv_left=vtxv;
  if (SPR->state_left==PS_GORILLA_BODY_STATE_FIST) vtxv_right=vtxv_left+3;
  else if (SPR->state_left==PS_GORILLA_BODY_STATE_PUNCH) vtxv_right=vtxv_left+3;
  else vtxv_right=vtxv_left+2;
  if (SPR->state_right==PS_GORILLA_BODY_STATE_FIST) vtxv_head=vtxv_right+3;
  else if (SPR->state_right==PS_GORILLA_BODY_STATE_PUNCH) vtxv_head=vtxv_right+3;
  else vtxv_head=vtxv_right+2;

  /* Draw everything. */
  ps_gorilla_draw_body(vtxv_left,spr,SPR->state_left,-(PS_TILESIZE>>1));
  ps_gorilla_draw_body(vtxv_right,spr,SPR->state_right,PS_TILESIZE>>1);
  ps_gorilla_draw_head(vtxv_head,spr);

  return vtxc;
}

/* Hurt.
 */

static int _ps_gorilla_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  if (SPR->invincible) return 0;

  if (--(SPR->hp)>0) {
    PS_SFX_MONSTER_HURT
    SPR->invincible=PS_GORILLA_INVINCIBLE_TIME;
    return 0;
  }

  PS_SFX_MONSTER_DEAD
  SPR->invincible=INT_MAX;  
  if (ps_game_create_fireworks(game,spr->x,spr->y)<0) return -1;
  if (ps_game_create_prize(game,spr->x,spr->y)<0) return -1;
  if (ps_sprite_kill_later(spr,game)<0) return -1;
  if (ps_game_check_deathgate(game)<0) return -1;
  if (ps_game_report_kill(game,assailant,spr)<0) return -1;

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_gorilla={
  .name="gorilla",
  .objlen=sizeof(struct ps_sprite_gorilla),

  .radius=PS_TILESIZE,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_gorilla_init,
  .del=_ps_gorilla_del,
  .configure=_ps_gorilla_configure,
  .get_configure_argument_name=_ps_gorilla_get_configure_argument_name,
  .update=_ps_gorilla_update,
  .draw=_ps_gorilla_draw,
  
  .hurt=_ps_gorilla_hurt,

};
