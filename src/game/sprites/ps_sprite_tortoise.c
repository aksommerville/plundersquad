#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "res/ps_resmgr.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_TORTOISE_ROLE_INIT      0 /* First update only. */
#define PS_TORTOISE_ROLE_SHELL     1 /* Shell, main controller. Also draws feet. */
#define PS_TORTOISE_ROLE_HEAD      2 /* Projected head, the only vulnerable sprite. */
#define PS_TORTOISE_ROLE_CUDGEL    3 /* Projected cudgel for smashing heroes. */

#define PS_TORTOISE_PHASE_INIT     0 /* First update only. */
#define PS_TORTOISE_PHASE_COWER    1 /* Hiding in shell. */
#define PS_TORTOISE_PHASE_WALK     2 /* Head out, walking around. */
#define PS_TORTOISE_PHASE_CUDGEL   3 /* Swinging cudgel. */
#define PS_TORTOISE_PHASE_SCRAM    4 /* Walking with head retracted. */
#define PS_TORTOISE_PHASE_ADVANCE  5 /* (HEAD,CUDGEL) Moving away from body. */
#define PS_TORTOISE_PHASE_RELAX    6 /* (HEAD,CUDGEL) Hold in place. */
#define PS_TORTOISE_PHASE_RETREAT  7 /* (HEAD,CUDGEL) Moving back to body. */

#define PS_TORTOISE_HP_DEFAULT 3
#define PS_TORTOISE_HURT_TIME 60

#define PS_TORTOISE_COWER_TIME_MIN    60
#define PS_TORTOISE_COWER_TIME_MAX   120

#define PS_TORTOISE_WALK_SPEED 0.6

#define PS_TORTOISE_FOOT_PERIOD 40
#define PS_TORTOISE_FOOT_RANGE   4
#define PS_TORTOISE_FOOT_Y       4
#define PS_TORTOISE_FOOT_X       4

#define PS_TORTOISE_HEAD_RADIUS  4
#define PS_TORTOISE_HEAD_X      12
#define PS_TORTOISE_HEAD_Y      -6
#define PS_TORTOISE_HEAD_ADVANCE_SPEED  0.5
#define PS_TORTOISE_HEAD_RETREAT_SPEED  0.8

#define PS_TORTOISE_CUDGEL_TARGET_WIDTH (PS_TILESIZE*3) /* We look for targets in a 90-degree triangle extending forward, for so far. */
#define PS_TORTOISE_CUDGEL_ADVANCE_SPEED   1.5
#define PS_TORTOISE_CUDGEL_RETREAT_SPEED   1.2
#define PS_TORTOISE_CUDGEL_EXTENDED_TIME 15
#define PS_TORTOISE_CUDGEL_LINK_COUNT 8
#define PS_TORTOISE_CUDGEL_RADIUS 6

#define PS_TORTOISE_BOMB_SPRDEFID 25
#define PS_TORTOISE_BOMB_ODDS 6 /* 1:x that we will throw a bomb instead of walking or cudgelling. */
#define PS_TORTOISE_BOMB_DELAY 230 /* Minimum frame count between bomb tosses -- time against bomb and explosion so we don't throw one into our own explosion. */

#define PS_TORTOISE_ACQUIRE_SHELL \
  struct ps_sprite *shell=0; \
  if (spr->master&&(spr->master->sprc>0)) { \
    shell=spr->master->sprv[0]; \
    if (shell->type!=&ps_sprtype_tortoise) shell=0; \
  } \
  struct ps_sprite_tortoise *SHELL=(struct ps_sprite_tortoise*)shell;

/* Private sprite object.
 */

struct ps_sprite_tortoise {
  struct ps_sprite hdr;
  int role;
  int phase;
  int phasetime;
  int facedir; // (-1,1) == (left,right)
  int hurt;
// PS_TORTOISE_ROLE_SHELL only:
  int hp;
  double dx,dy;
  int bombdelay;
// HEAD,CUDGEL:
  double dstx,dsty;
  double advance_speed;
  double retreat_speed;
};

#define SPR ((struct ps_sprite_tortoise*)spr)

/* Delete.
 */

static void _ps_tortoise_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_tortoise_init(struct ps_sprite *spr) {
  SPR->role=PS_TORTOISE_ROLE_INIT;
  SPR->phase=PS_TORTOISE_PHASE_INIT;
  SPR->hp=PS_TORTOISE_HP_DEFAULT;
  SPR->facedir=(rand()&1)?-1:1;
  return 0;
}

/* Configure.
 */

static int _ps_tortoise_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_tortoise_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_tortoise_configure(), for editor.
  return 0;
}

/* First frame setup.
 */

static int ps_tortoise_setup(struct ps_sprite *spr,struct ps_game *game) {
  SPR->role=PS_TORTOISE_ROLE_SHELL;
  SPR->phase=PS_TORTOISE_PHASE_COWER;
  SPR->phasetime=PS_TORTOISE_COWER_TIME_MIN+rand()%(PS_TORTOISE_COWER_TIME_MAX-PS_TORTOISE_COWER_TIME_MIN+1);
  return 0;
}

/* Find head or cudgel.
 */

static struct ps_sprite *ps_tortoise_find_head(const struct ps_sprite *spr,const struct ps_game *game) {
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *head=grp->sprv[i];
    if (head->type!=&ps_sprtype_tortoise) continue;
    if (!head->master) continue;
    if (head->master->sprc<1) continue;
    if (head->master->sprv[0]!=spr) continue;
    struct ps_sprite_tortoise *HEAD=(struct ps_sprite_tortoise*)head;
    if (HEAD->role==PS_TORTOISE_ROLE_HEAD) return head;
    if (HEAD->role==PS_TORTOISE_ROLE_CUDGEL) return head;
  }
  return 0;
}

/* Create a new head.
 */

static struct ps_sprite *ps_tortoise_create_head(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *head=ps_sprdef_instantiate(game,spr->def,0,0,spr->x,spr->y);
  if (!head) return 0;
  struct ps_sprite_tortoise *HEAD=(struct ps_sprite_tortoise*)head;

  head->layer--;
  if (ps_sprite_set_master(head,spr,game)<0) return 0;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_SOLID,head)<0) return 0;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_PHYSICS,head)<0) return 0;

  return head;
}

/* Extend my head.
 */

static int ps_tortoise_begin_head(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *head=ps_tortoise_find_head(spr,game);
  if (!head) {
    if (!(head=ps_tortoise_create_head(spr,game))) return -1;
  }
  struct ps_sprite_tortoise *HEAD=(struct ps_sprite_tortoise*)head;

  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_FRAGILE,head)<0) return 0;
  HEAD->dx=0;
  HEAD->dy=0;
  HEAD->role=PS_TORTOISE_ROLE_HEAD;
  HEAD->phase=PS_TORTOISE_PHASE_ADVANCE;
  HEAD->phasetime=60;
  head->radius=PS_TORTOISE_HEAD_RADIUS;
  HEAD->dstx=PS_TORTOISE_HEAD_X;
  HEAD->dsty=PS_TORTOISE_HEAD_Y;
  HEAD->advance_speed=PS_TORTOISE_HEAD_ADVANCE_SPEED;
  HEAD->retreat_speed=PS_TORTOISE_HEAD_RETREAT_SPEED;

  return 0;
}

/* Locate my head and begin withdrawing it.
 */

static int ps_tortoise_retract_head(struct ps_sprite *spr,struct ps_game *game) {
  struct ps_sprite *head=ps_tortoise_find_head(spr,game);
  if (!head) return 0;
  struct ps_sprite_tortoise *HEAD=(struct ps_sprite_tortoise*)head;

  HEAD->phase=PS_TORTOISE_PHASE_RETREAT;
  HEAD->phasetime=99999;
  
  return 0;
}

/* Enter WALK phase.
 */

static int ps_tortoise_shell_begin_WALK(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_TORTOISE_PHASE_WALK;
  SPR->phasetime=PS_TORTOISE_COWER_TIME_MIN+rand()%(PS_TORTOISE_COWER_TIME_MAX-PS_TORTOISE_COWER_TIME_MIN+1);

  if (ps_game_select_random_travel_vector(&SPR->dx,&SPR->dy,game,spr->x,spr->y,PS_TORTOISE_WALK_SPEED,spr->impassable)<0) return -1;

  /* Face the direction we are walking. */
  if (SPR->dx<0.0) SPR->facedir=-1;
  else SPR->facedir=1;

  /* Extend the head. */
  if (ps_tortoise_begin_head(spr,game)<0) return -1;
  
  return 0;
}

/* Enter COWER phase.
 */

static int ps_tortoise_shell_begin_COWER(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_TORTOISE_PHASE_COWER;
  SPR->phasetime=PS_TORTOISE_COWER_TIME_MIN+rand()%(PS_TORTOISE_COWER_TIME_MAX-PS_TORTOISE_COWER_TIME_MIN+1);
  if (ps_tortoise_retract_head(spr,game)<0) return -1;
  return 0;
}

/* Test point in a clockwise triangle.
 */

static int ps_tortoise_point_in_triangle(double x,double y,const double *ptv) {
  int i=0; for (;i<6;i+=2) {
    double ax=x-ptv[i];
    double ay=y-ptv[i+1];
    double bx=ptv[(i+2)%6]-ptv[i];
    double by=ptv[(i+3)%6]-ptv[i+1];
    double cp=(ax*by)-(bx*ay);
    if (cp>=0.0) return 0;
  }
  return 1;
}

/* Identify target for CUDGEL phase.
 */

static struct ps_sprite *ps_tortoise_locate_cudgel_target(const struct ps_sprite *spr,const struct ps_game *game) {

  /* The three points of my target triangle, arranged clockwise.
   */
  double ptv[6]={
    spr->x,spr->y,
  };
  if (SPR->facedir<0) {
    ptv[2]=spr->x-PS_TORTOISE_CUDGEL_TARGET_WIDTH;
    ptv[3]=spr->y+(PS_TORTOISE_CUDGEL_TARGET_WIDTH>>1);
    ptv[4]=spr->x-PS_TORTOISE_CUDGEL_TARGET_WIDTH;
    ptv[5]=spr->y-(PS_TORTOISE_CUDGEL_TARGET_WIDTH>>1);
  } else {
    ptv[2]=spr->x+PS_TORTOISE_CUDGEL_TARGET_WIDTH;
    ptv[3]=spr->y-(PS_TORTOISE_CUDGEL_TARGET_WIDTH>>1);
    ptv[4]=spr->x+PS_TORTOISE_CUDGEL_TARGET_WIDTH;
    ptv[5]=spr->y+(PS_TORTOISE_CUDGEL_TARGET_WIDTH>>1);
  }

  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->type==&ps_sprtype_tortoise) continue;
    if (victim->type==&ps_sprtype_bomb) continue;
    if (ps_tortoise_point_in_triangle(victim->x,victim->y,ptv)) {
      return victim;
    }
  }
  
  return 0;
}

/* Enter CUDGEL phase.
 */

static int ps_tortoise_shell_begin_CUDGEL(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *target) {

  struct ps_sprite *cudgel=ps_tortoise_find_head(spr,game);
  if (!cudgel) {
    if (!(cudgel=ps_tortoise_create_head(spr,game))) return -1;
  }
  struct ps_sprite_tortoise *CUDGEL=(struct ps_sprite_tortoise*)cudgel;

  CUDGEL->dx=0;
  CUDGEL->dy=0;
  CUDGEL->role=PS_TORTOISE_ROLE_CUDGEL;
  CUDGEL->phase=PS_TORTOISE_PHASE_ADVANCE;
  CUDGEL->phasetime=60;
  cudgel->radius=PS_TORTOISE_CUDGEL_RADIUS;
  CUDGEL->dstx=target->x-spr->x;
  if (CUDGEL->dstx<0.0) CUDGEL->dstx=-CUDGEL->dstx;
  CUDGEL->dsty=target->y-spr->y;
  CUDGEL->advance_speed=PS_TORTOISE_CUDGEL_ADVANCE_SPEED;
  CUDGEL->retreat_speed=PS_TORTOISE_CUDGEL_RETREAT_SPEED;

  SPR->phase=PS_TORTOISE_PHASE_CUDGEL;
  SPR->phasetime=100;//TODO
  
  return 0;
}

/* Throw bomb.
 * Return >0 if thrown, 0 if rejected, or <0 for real errors.
 */

static int ps_tortoise_throw_bomb(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->bombdelay) return 0;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_TORTOISE_BOMB_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for bomb",PS_TORTOISE_BOMB_SPRDEFID);
    return -1;
  }
  
  int x=spr->x;
  int y=spr->y;
  int offset=(PS_TILESIZE*2)/3;
  if (SPR->facedir<0) {
    x-=offset;
  } else {
    x+=offset;
  }

  /* Quick check to ensure we're not throwing it into a wall or hole.
   */
  if ((x>=0)&&(y>=0)&&(x<PS_SCREENW)&&(y<PS_SCREENH)) {
    const struct ps_grid_cell *cell=ps_grid_get_cell(game->grid,x/PS_TILESIZE,y/PS_TILESIZE);
    if (cell) switch (cell->physics) {
      case PS_BLUEPRINT_CELL_SOLID:
      case PS_BLUEPRINT_CELL_LATCH:
      case PS_BLUEPRINT_CELL_HOLE: {
        } return 0;
    }
  }

  PS_SFX_BOMB_THROW
  
  struct ps_sprite *bomb=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
  if (!bomb) return -1;

  if (ps_sprite_bomb_throw(bomb,(SPR->facedir<0)?PS_DIRECTION_WEST:PS_DIRECTION_EAST,30)<0) return -1;
  SPR->bombdelay=PS_TORTOISE_BOMB_DELAY;
  
  return 1;
}

/* Select new phase for shell.
 */

static int ps_tortoise_shell_select_phase(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->phase==PS_TORTOISE_PHASE_CUDGEL) return ps_tortoise_shell_begin_COWER(spr,game);
  if (SPR->phase==PS_TORTOISE_PHASE_WALK) return ps_tortoise_shell_begin_COWER(spr,game);

  /* Sometimes, throw a bomb and remain in COWER phase. */
  if (!(rand()%PS_TORTOISE_BOMB_ODDS)) {
    int err=ps_tortoise_throw_bomb(spr,game);
    if (err<0) return err;
    if (err>0) return ps_tortoise_shell_begin_COWER(spr,game);
  }

  /* If a hero is nearby, point towards it and begin the cudgel. */
  struct ps_sprite *target=ps_tortoise_locate_cudgel_target(spr,game);
  if (target) {
    return ps_tortoise_shell_begin_CUDGEL(spr,game,target);
  }

  /* WALK or COWER, randomly. */
  if (rand()&1) {
    return ps_tortoise_shell_begin_WALK(spr,game);
  } else {
    return ps_tortoise_shell_begin_COWER(spr,game);
  }
}

/* Update in WALK phase.
 */

static int ps_tortoise_shell_update_WALK(struct ps_sprite *spr,struct ps_game *game) {
  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  return 0;
}

/* Update in SCRAM phase.
 */

static int ps_tortoise_shell_update_SCRAM(struct ps_sprite *spr,struct ps_game *game) {
  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  return 0;
}

/* Update shell.
 */

static int ps_tortoise_update_SHELL(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->hurt>0) SPR->hurt--;
  if (SPR->bombdelay>0) SPR->bombdelay--;

  if (SPR->phasetime--<=0) {
    return ps_tortoise_shell_select_phase(spr,game);
  } else switch (SPR->phase) {
    case PS_TORTOISE_PHASE_COWER: return 0;
    case PS_TORTOISE_PHASE_WALK: return ps_tortoise_shell_update_WALK(spr,game);
    case PS_TORTOISE_PHASE_CUDGEL: return 0;
    case PS_TORTOISE_PHASE_SCRAM: return ps_tortoise_shell_update_SCRAM(spr,game);
    default: return ps_tortoise_shell_select_phase(spr,game);
  }
  return 0;
}

/* Update head.
 */

static int ps_tortoise_update_HEAD(struct ps_sprite *spr,struct ps_game *game) {
  PS_TORTOISE_ACQUIRE_SHELL
  if (!shell) return ps_sprite_kill_later(spr,game);

  SPR->hurt=SHELL->hurt;
  
  if (SPR->phasetime--<=0) switch (SPR->phase) {
  
    case PS_TORTOISE_PHASE_ADVANCE: {
        if (SPR->role==PS_TORTOISE_ROLE_CUDGEL) {
          SPR->phasetime=PS_TORTOISE_CUDGEL_EXTENDED_TIME;
        } else {
          SPR->phasetime=99999;
        }
        SPR->phase=PS_TORTOISE_PHASE_RELAX;
      } break;

    case PS_TORTOISE_PHASE_RELAX: {
        SPR->phasetime=99999;
        SPR->phase=PS_TORTOISE_PHASE_RETREAT;
      } break;

    case PS_TORTOISE_PHASE_RETREAT: {
        return ps_sprite_kill_later(spr,game);
      } 

  } else switch (SPR->phase) {
  
    case PS_TORTOISE_PHASE_ADVANCE: {
        SPR->facedir=SHELL->facedir;
        if (SPR->dx<SPR->dstx) SPR->dx+=SPR->advance_speed;
        if (SPR->dsty>0.0) {
          if (SPR->dy<SPR->dsty) SPR->dy+=SPR->advance_speed;
        } else {
          if (SPR->dy>SPR->dsty) SPR->dy-=SPR->advance_speed;
        }
        if (SPR->facedir<0) {
          spr->x=shell->x-SPR->dx;
        } else {
          spr->x=shell->x+SPR->dx;
        }
        spr->y=shell->y+SPR->dy;
      } break;
      
    case PS_TORTOISE_PHASE_RELAX: {
        SPR->facedir=SHELL->facedir;
        if (SPR->facedir<0) {
          spr->x=shell->x-SPR->dx;
        } else {
          spr->x=shell->x+SPR->dx;
        }
        spr->y=shell->y+SPR->dy;
      } break;
    
    case PS_TORTOISE_PHASE_RETREAT: {
        SPR->facedir=SHELL->facedir;
        if (SPR->dx>0.0) SPR->dx-=SPR->retreat_speed;
        if (SPR->dy>0.0) {
          if ((SPR->dy-=SPR->retreat_speed)<0.0) SPR->dy=0.0;
        } else {
          if ((SPR->dy+=SPR->retreat_speed)>0.0) SPR->dy=0.0;
        }
        if ((SPR->dx<=0.0)&&(SPR->dy==0.0)) return ps_sprite_kill_later(spr,game);
        if (SPR->facedir<0) {
          spr->x=shell->x-SPR->dx;
        } else {
          spr->x=shell->x+SPR->dx;
        }
        spr->y=shell->y+SPR->dy;
      } break;
  }
  return 0;
}

/* Update, dispatch on role.
 */

static int _ps_tortoise_update(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->role) {
    case PS_TORTOISE_ROLE_INIT: return ps_tortoise_setup(spr,game);
    case PS_TORTOISE_ROLE_SHELL: return ps_tortoise_update_SHELL(spr,game);
    case PS_TORTOISE_ROLE_HEAD: return ps_tortoise_update_HEAD(spr,game);
    case PS_TORTOISE_ROLE_CUDGEL: return ps_tortoise_update_HEAD(spr,game);
  }
  return -1;
}

/* Draw shell and feet.
 */

static int ps_tortoise_draw_shell(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=3;
  if (vtxc>vtxa) return vtxc;

  // Shell
  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].tileid=spr->tileid;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  // Left foot
  int footphase;
  if ((SPR->phase==PS_TORTOISE_PHASE_COWER)||(SPR->phase==PS_TORTOISE_PHASE_CUDGEL)) {
    footphase=PS_TORTOISE_FOOT_PERIOD>>1;
  } else {
    footphase=SPR->phasetime%PS_TORTOISE_FOOT_PERIOD;
    if (footphase>=PS_TORTOISE_FOOT_PERIOD>>1) {
      footphase=PS_TORTOISE_FOOT_PERIOD-((footphase-(PS_TORTOISE_FOOT_PERIOD>>1))<<1);
    } else {
      footphase<<=1;
    }
  }
  memcpy(vtxv+1,vtxv,sizeof(struct akgl_vtx_maxtile));
  vtxv[1].x+=-PS_TORTOISE_FOOT_X;
  vtxv[1].y+= PS_TORTOISE_FOOT_Y+(footphase*PS_TORTOISE_FOOT_RANGE)/PS_TORTOISE_FOOT_PERIOD;
  vtxv[1].tileid+=0x20;

  // Right foot
  if ((SPR->phase==PS_TORTOISE_PHASE_COWER)||(SPR->phase==PS_TORTOISE_PHASE_CUDGEL)) {
    footphase=PS_TORTOISE_FOOT_PERIOD>>1;
  } else {
    footphase=(SPR->phasetime+(PS_TORTOISE_FOOT_PERIOD>>1))%PS_TORTOISE_FOOT_PERIOD;
    if (footphase>=PS_TORTOISE_FOOT_PERIOD>>1) {
      footphase=PS_TORTOISE_FOOT_PERIOD-((footphase-(PS_TORTOISE_FOOT_PERIOD>>1))<<1);
    } else {
      footphase<<=1;
    }
  }
  memcpy(vtxv+2,vtxv,sizeof(struct akgl_vtx_maxtile));
  vtxv[2].x+= PS_TORTOISE_FOOT_X;
  vtxv[2].y+= PS_TORTOISE_FOOT_Y+(footphase*PS_TORTOISE_FOOT_RANGE)/PS_TORTOISE_FOOT_PERIOD;
  vtxv[2].tileid+=0x20;
  
  return vtxc;
}

/* Draw head.
 */

static int ps_tortoise_draw_head(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=1;
  if (vtxc>vtxa) return vtxc;
  
  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].tileid=spr->tileid+0x01;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  return vtxc;
}

/* Draw cudgel.
 */

static int ps_tortoise_draw_cudgel(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=PS_TORTOISE_CUDGEL_LINK_COUNT+1;
  if (vtxc>vtxa) return vtxc;
  
  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].tileid=spr->tileid+0x11;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  int i=vtxc; while (i-->1) memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));

  PS_TORTOISE_ACQUIRE_SHELL
  if (!shell) return -1;
  double dx=spr->x-shell->x;
  double dy=spr->y-shell->y;
  for (i=0;i<vtxc-1;i++) {
    vtxv[i].x=shell->x+(dx*i)/vtxc;
    vtxv[i].y=shell->y+(dy*i)/vtxc;
    vtxv[i].tileid-=1;
  }

  return vtxc;
}

/* Draw, dispatch on role.
 */

static int _ps_tortoise_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=0;
  switch (SPR->role) {
    case PS_TORTOISE_ROLE_SHELL: vtxc=ps_tortoise_draw_shell(vtxv,vtxa,spr); break;
    case PS_TORTOISE_ROLE_HEAD: vtxc=ps_tortoise_draw_head(vtxv,vtxa,spr); break;
    case PS_TORTOISE_ROLE_CUDGEL: vtxc=ps_tortoise_draw_cudgel(vtxv,vtxa,spr); break;
  }
  if ((vtxc<1)||(vtxc>vtxa)) return vtxc;

  if (SPR->hurt) {
    vtxv[0].tr=0xff;
    vtxv[0].tg=0x00;
    vtxv[0].tb=0x00;
    vtxv[0].ta=30+(SPR->hurt*225)/PS_TORTOISE_HURT_TIME;
    int i=vtxc; while (i-->1) {
      vtxv[i].tr=vtxv[0].tr;
      vtxv[i].tg=vtxv[0].tg;
      vtxv[i].tb=vtxv[0].tb;
      vtxv[i].ta=vtxv[0].ta;
    }
  }
  
  return vtxc;
}

/* Hurt.
 */

static int _ps_tortoise_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

   // Only the head is actually vulnerable, and it requires a shell.
  if (SPR->role!=PS_TORTOISE_ROLE_HEAD) return 0;
  PS_TORTOISE_ACQUIRE_SHELL
  if (!shell) return 0;

  // No double-dipping.
  if (SPR->hurt) return 0;

  // The crushing blow?
  if (--(SHELL->hp)<=0) {
    PS_SFX_MONSTER_DEAD
    SPR->hurt=INT_MAX;
    SHELL->hurt=INT_MAX;
    if (ps_game_create_fireworks(game,shell->x,shell->y)<0) return -1;
    if (ps_game_create_prize(game,shell->x,shell->y)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    if (ps_sprite_kill_later(shell,game)<0) return -1;
    if (ps_game_check_deathgate(game)<0) return -1;
    if (ps_game_report_kill(game,assailant,shell)<0) return -1;
    return 0;
  }

  // Ouch
  PS_SFX_MONSTER_HURT
  SPR->hurt=PS_TORTOISE_HURT_TIME;
  SHELL->hurt=SPR->hurt;
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_tortoise={
  .name="tortoise",
  .objlen=sizeof(struct ps_sprite_tortoise),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_tortoise_init,
  .del=_ps_tortoise_del,
  .configure=_ps_tortoise_configure,
  .get_configure_argument_name=_ps_tortoise_get_configure_argument_name,
  .update=_ps_tortoise_update,
  .draw=_ps_tortoise_draw,
  
  .hurt=_ps_tortoise_hurt,

};
