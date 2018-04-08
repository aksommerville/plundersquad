/* ps_sprite_dragon.c
 * A sprite controlled by multiple players.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "input/ps_input.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "util/ps_geometry.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_DRAGON_NECK_BONE_COUNT    6
#define PS_DRAGON_LEG_BONE_COUNT     3

#define PS_DRAGON_WALK_SPEED 1.0
#define PS_DRAGON_FLY_SPEED  2.0

/* Animation timing. */
#define PS_DRAGON_WING_FRAME_TIME  6
#define PS_DRAGON_LEG_ANIM_TIME   30
#define PS_DRAGON_FIRE_FRAME_TIME  5

/* Layout constants for legs, relative to attachment point. */
#define PS_DRAGON_LEG_FLY_X        ((PS_TILESIZE* 0)/8)
#define PS_DRAGON_LEG_FLY_Y        ((PS_TILESIZE* 1)/8)
#define PS_DRAGON_LEG_IDLE_X       ((PS_TILESIZE* 0)/8)
#define PS_DRAGON_LEG_IDLE_Y       ((PS_TILESIZE* 4)/8)
#define PS_DRAGON_LEG_WALK_A_X     ((PS_TILESIZE* 0)/8)
#define PS_DRAGON_LEG_WALK_A_Y     ((PS_TILESIZE* 0)/8)
#define PS_DRAGON_LEG_WALK_Z_X     ((PS_TILESIZE* 2)/8)
#define PS_DRAGON_LEG_WALK_Z_Y     ((PS_TILESIZE* 6)/8)

/* Constants for head motion. */
#define PS_DRAGON_HEAD_ANGLE_STEP        0.03
#define PS_DRAGON_HEAD_MAGNITUDE_STEP    0.50
#define PS_DRAGON_HEAD_MAGNITUDE_MIN     ((PS_TILESIZE* 4)/8)
#define PS_DRAGON_HEAD_MAGNITUDE_MAX     ((PS_TILESIZE*20)/8)
#define PS_DRAGON_HEAD_MAGNITUDE_INITIAL ((PS_TILESIZE* 8)/8)
#define PS_DRAGON_HEAD_ANGLE_MIN         (M_PI*-0.70)
#define PS_DRAGON_HEAD_ANGLE_MAX         (M_PI* 0.20)
#define PS_DRAGON_HEAD_SIZE              ((PS_TILESIZE*12)/8)

/* Constants for head attachment. */
#define PS_DRAGON_NECK_ANGLE_MIN    (M_PI*-0.70)
#define PS_DRAGON_NECK_ANGLE_MAX    (M_PI* 0.10)
#define PS_DRAGON_NECK_DISTANCE     ((PS_TILESIZE* 7)/8)

#define PS_DRAGON_FIRE_RADIUS    ((PS_TILESIZE*12)/8)

static int ps_dragon_add_head(struct ps_sprite *spr,int playerid,struct ps_game *game);
static int ps_dragon_add_leg(struct ps_sprite *spr,int x,int y);

/* Private sprite object.
 */

struct ps_dragon_input {
  int playerid;
  uint16_t btnid;
  uint16_t state;
};
static inline struct ps_dragon_input ps_dragon_input(int playerid,uint16_t btnid) {
  return (struct ps_dragon_input){playerid,btnid,0};
}

struct ps_dragon_head {
  int attachx,attachy; // Position of neck/body attachment relative to sprite. Always unflopped.
  int x,y; // Position of head's terminal tile. Always unflopped. (calculate from angle and magnitude)
  int playerid;
  uint8_t pr,pg,pb;
  int firing;
  double angle,magnitude; // Position of head relative to attachment, in radians and pixels.
  int fireanimcounter,fireanimframe;
  struct ps_dragon_input input_clock;
  struct ps_dragon_input input_counter;
  struct ps_dragon_input input_forward;
  struct ps_dragon_input input_backward;
  struct ps_dragon_input input_fire;
};

struct ps_dragon_leg {
  int attachx,attachy; // Position of neck/body attachment relative to sprite. Always unflopped.
  int x,y; // Position of leg's terminal tile. Always unflopped.
};

struct ps_sprite_dragon {
  struct ps_sprite hdr;
  int playeridv[PS_PLAYER_LIMIT];
  int playeridc;
  struct ps_dragon_head *headv;
  int headc,heada;
  struct ps_dragon_leg *legv;
  int legc,lega;
  int facedir; // -1 or 1
  int flying;
  int walking;
  int wingframe,wingcounter;
  int leganimp;
  struct ps_dragon_input input_left;
  struct ps_dragon_input input_right;
  struct ps_dragon_input input_up;
  struct ps_dragon_input input_down;
  struct ps_dragon_input input_fly;
};

#define SPR ((struct ps_sprite_dragon*)spr)

/* Delete.
 */

static void _ps_dragon_del(struct ps_sprite *spr) {
  if (SPR->headv) free(SPR->headv);
  if (SPR->legv) free(SPR->legv);
}

/* Initialize.
 */

static int _ps_dragon_init(struct ps_sprite *spr) {

  SPR->facedir=-1;

  if (ps_dragon_add_leg(spr,-(PS_TILESIZE>>1),PS_TILESIZE>>1)<0) return -1;
  if (ps_dragon_add_leg(spr,PS_TILESIZE>>1,PS_TILESIZE>>1)<0) return -1;

  return 0;
}

/* Configure.
 */

static int _ps_dragon_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_dragon_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_dragon_configure(), for editor.
  return 0;
}

/* Update state of an input source.
 */

static void ps_dragon_input_update(struct ps_dragon_input *input) {
  input->state=(ps_get_player_buttons(input->playerid)&input->btnid);
}

/* Move dragon.
 */

static int ps_dragon_walk(struct ps_sprite *spr,struct ps_game *game,int dx,int dy) {

  double speed=SPR->flying?PS_DRAGON_FLY_SPEED:PS_DRAGON_WALK_SPEED;
  spr->x+=dx*speed;
  spr->y+=dy*speed;

  if ((dx<0)&&(SPR->facedir>=0)) {
    SPR->facedir=-1;
  } else if ((dx>0)&&(SPR->facedir<=0)) {
    SPR->facedir=1;
  }

  SPR->walking=1;

  /* Ugly hack to prevent screen-switch troubles.
   * game->inhibit_screen_switch is set when we move to a neighbor grid.
   * Normal heroes unset it when anybody leaves the OFFSCREEN state (ie returns into play).
   * Dragon doesn't use hero state, so we'll participate manually.
   */
  if ((spr->x>1.0)&&(spr->y>1.0)&&(spr->x<PS_SCREENW-1.0)&&(spr->y<PS_SCREENH-1.0)) {
    game->inhibit_screen_switch=0;
  }

  return 0;
}

/* Control flight.
 */

static int ps_dragon_safe_to_land(const struct ps_sprite *spr,const struct ps_game *game) {
  int xa=spr->x-spr->radius;
  int xz=spr->x+spr->radius;
  int ya=spr->y-spr->radius;
  int yz=spr->y+spr->radius;
  int hole=ps_grid_test_rect_physics(game->grid,xa,ya,xz-xa,yz-ya,1<<PS_BLUEPRINT_CELL_HOLE);
  if (hole) return 0;
  return 1;
}

static int ps_dragon_fly(struct ps_sprite *spr,struct ps_game *game,int fly) {
  if (fly) {
    if (SPR->flying) return 0;
    SPR->flying=1;
    spr->impassable&=~(1<<PS_BLUEPRINT_CELL_HOLE);
  } else {
    if (!SPR->flying) return 0;
    // If we are over water or something, just keep flying even though the button is not pressed.
    // In that case, this clause will be polled every update until the situation changes.
    if (ps_dragon_safe_to_land(spr,game)) {
      SPR->flying=0;
      spr->impassable|=(1<<PS_BLUEPRINT_CELL_HOLE);
    }
  }
  return 0;
}

/* Leg animation.
 */

static int ps_dragon_animate_legs(struct ps_sprite *spr) {

  /* Update animation counter.
   * Must first determine what the current state is (either 1 frame or PS_DRAGON_LEG_ANIM_TIME).
   */
  int animc=1;
  if (SPR->walking&&!SPR->flying) animc=PS_DRAGON_LEG_ANIM_TIME;
  if (++(SPR->leganimp)>=animc) {
    SPR->leganimp=0;
  }

  /* Each animation is a back-and-forth cycling between two positions (which might be the same).
   * Determine the 'a' and 'z' positions for each leg, then calculate its target.
   */
  struct ps_dragon_leg *leg=SPR->legv;
  int i=SPR->legc; for (;i-->0;leg++) {
  
    int ax,ay,zx,zy;
    if (SPR->flying) { 
      ax=zx=PS_DRAGON_LEG_FLY_X;
      ay=zy=PS_DRAGON_LEG_FLY_Y;
    } else if (SPR->walking) {
      if (i&1) {
        ax=PS_DRAGON_LEG_WALK_A_X;
        ay=PS_DRAGON_LEG_WALK_A_Y;
        zx=PS_DRAGON_LEG_WALK_Z_X;
        zy=PS_DRAGON_LEG_WALK_Z_Y;
      } else {
        ax=PS_DRAGON_LEG_WALK_Z_X;
        ay=PS_DRAGON_LEG_WALK_Z_Y;
        zx=PS_DRAGON_LEG_WALK_A_X;
        zy=PS_DRAGON_LEG_WALK_A_Y;
      }
    } else {
      ax=zx=PS_DRAGON_LEG_IDLE_X;
      ay=zy=PS_DRAGON_LEG_IDLE_Y;
    }

    if (leg->attachx<0) {
      ax=leg->attachx-ax;
      zx=leg->attachx-zx;
    } else {
      ax=leg->attachx+ax;
      zx=leg->attachx+zx;
    }
    ay=leg->attachy+ay;
    zy=leg->attachy+zy;

    int targetx,targety;
    if (animc>=2) { // Animate back and forth.
      int c=animc>>1,p;
      if (SPR->leganimp<c) { // (a) to (z)
        p=SPR->leganimp;
      } else { // (z) to (a)
        p=animc-SPR->leganimp;
      }
      targetx=ax+(p*(zx-ax))/c;
      targety=ay+(p*(zy-ay))/c;
    } else { // Just use (a).
      targetx=ax;
      targety=ay;
    }

    if (leg->x<targetx) leg->x++;
    else if (leg->x>targetx) leg->x--;
    if (leg->y<targety) leg->y++;
    else if (leg->y>targety) leg->y--;
      
  }

  return 0;
}

/* Populate (x,y) based on (attachx,attachy,angle,magnitude).
 */

static void ps_dragon_head_calculate_position(struct ps_dragon_head *head) {
  head->x=head->attachx+sin(head->angle)*head->magnitude;
  head->y=head->attachy-cos(head->angle)*head->magnitude;
}

/* Move head laterally.
 */

static int ps_dragon_head_rotate(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head,int dt) {
  if (SPR->facedir>0) dt=-dt;
  head->angle+=dt*PS_DRAGON_HEAD_ANGLE_STEP;
  if (head->angle<PS_DRAGON_HEAD_ANGLE_MIN) head->angle=PS_DRAGON_HEAD_ANGLE_MIN;
  else if (head->angle>PS_DRAGON_HEAD_ANGLE_MAX) head->angle=PS_DRAGON_HEAD_ANGLE_MAX;
  ps_dragon_head_calculate_position(head);
  return 0;
}

/* Move head in or out.
 */

static int ps_dragon_head_advance(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head,int dp) {
  head->magnitude+=dp*PS_DRAGON_HEAD_MAGNITUDE_STEP;
  if (head->magnitude<PS_DRAGON_HEAD_MAGNITUDE_MIN) head->magnitude=PS_DRAGON_HEAD_MAGNITUDE_MIN;
  else if (head->magnitude>PS_DRAGON_HEAD_MAGNITUDE_MAX) head->magnitude=PS_DRAGON_HEAD_MAGNITUDE_MAX;
  ps_dragon_head_calculate_position(head);
  return 0;
}

/* Breathe fire.
 */

static int ps_dragon_head_begin_fire(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head) {
  head->firing=1;
  return 0;
}

static int ps_dragon_head_end_fire(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head) {
  head->firing=0;
  return 0;
}

/* While fire active, check for kills.
 */
 
static int ps_dragon_head_check_kills(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head) {

  /* Where is the fire? */
  double firex,firey;
  double dx=((head->x-head->attachx)*PS_TILESIZE)/head->magnitude;
  double dy=((head->y-head->attachy)*PS_TILESIZE)/head->magnitude;
  if (SPR->facedir>0) {
    firex=spr->x-head->x-dx;
    firey=spr->y+head->y+dy;
  } else {
    firex=spr->x+head->x+dx;
    firey=spr->y+head->y+dy;
  }
  struct ps_circle firecircle=ps_circle(firex,firey,PS_DRAGON_FIRE_RADIUS);

  /* Check fragile sprites. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *fragile=grp->sprv[i];
    if (fragile==spr) continue;
    if (ps_sprite_collide_circle(fragile,&firecircle)) {
      if (ps_sprite_receive_damage(game,fragile,spr)<0) return -1;
    }
  }
  
  return 0;
}

/* Update one head.
 */

static int ps_dragon_update_head(struct ps_sprite *spr,struct ps_game *game,struct ps_dragon_head *head) {

  ps_dragon_input_update(&head->input_clock);
  ps_dragon_input_update(&head->input_counter);
  ps_dragon_input_update(&head->input_forward);
  ps_dragon_input_update(&head->input_backward);
  ps_dragon_input_update(&head->input_fire);

  if (head->input_clock.state&&!head->input_counter.state) {
    if (ps_dragon_head_rotate(spr,game,head,1)<0) return -1;
  } else if (!head->input_clock.state&&head->input_counter.state) {
    if (ps_dragon_head_rotate(spr,game,head,-1)<0) return -1;
  }

  if (head->input_forward.state&&!head->input_backward.state) {
    if (ps_dragon_head_advance(spr,game,head,1)<0) return -1;
  } else if (!head->input_forward.state&&head->input_backward.state) {
    if (ps_dragon_head_advance(spr,game,head,-1)<0) return -1;
  }

  if (head->input_fire.state&&!head->firing) {
    if (ps_dragon_head_begin_fire(spr,game,head)<0) return -1;
  } else if (!head->input_fire.state&&head->firing) {
    if (ps_dragon_head_end_fire(spr,game,head)<0) return -1;
  }

  if (head->firing) {
    if (ps_dragon_head_check_kills(spr,game,head)<0) return -1;
  }

  if (++(head->fireanimcounter)>=PS_DRAGON_FIRE_FRAME_TIME) {
    head->fireanimcounter=0;
    if (++(head->fireanimframe)>=2) {
      head->fireanimframe=0;
    }
  }

  return 0;
}

/* Update.
 */

static int _ps_dragon_update(struct ps_sprite *spr,struct ps_game *game) {

  ps_dragon_input_update(&SPR->input_left);
  ps_dragon_input_update(&SPR->input_right);
  ps_dragon_input_update(&SPR->input_up);
  ps_dragon_input_update(&SPR->input_down);
  ps_dragon_input_update(&SPR->input_fly);

  SPR->walking=0;
  if (SPR->input_left.state&&!SPR->input_right.state) {
    if (ps_dragon_walk(spr,game,-1,0)<0) return -1;
  } else if (!SPR->input_left.state&&SPR->input_right.state) {
    if (ps_dragon_walk(spr,game,1,0)<0) return -1;
  }
  if (SPR->input_up.state&&!SPR->input_down.state) {
    if (ps_dragon_walk(spr,game,0,-1)<0) return -1;
  } else if (!SPR->input_up.state&&SPR->input_down.state) {
    if (ps_dragon_walk(spr,game,0,1)<0) return -1;
  }

  if (SPR->input_fly.state) {
    if (ps_dragon_fly(spr,game,1)<0) return -1;
  } else {
    if (ps_dragon_fly(spr,game,0)<0) return -1;
  }

  struct ps_dragon_head *head=SPR->headv;
  int i=SPR->headc; for (;i-->0;head++) {
    if (ps_dragon_update_head(spr,game,head)<0) return -1;
  }

  if (ps_dragon_animate_legs(spr)<0) return -1;

  if (SPR->flying) {
    if (++(SPR->wingcounter)>=PS_DRAGON_WING_FRAME_TIME) {
      SPR->wingcounter=0;
      if (++(SPR->wingframe)>=4) SPR->wingframe=0;
    }
  }

  return 0;
}

/* Draw bones.
 * (xa,ya) is inclusive; (xz,yz) is exclusive.
 * Both are unflopped and relative to sprite.
 */

static int ps_dragon_draw_bones(struct akgl_vtx_maxtile *vtxv,int vtxc,struct ps_sprite *spr,int xa,int ya,int xz,int yz) {

  if (SPR->facedir>0) {
    xa=-xa;
    xz=-xz;
  }
  xa+=spr->x;
  ya+=spr->y;
  xz+=spr->x;
  yz+=spr->y;
  int dx=xz-xa;
  int dy=yz-ya;
  
  int i=0; for (;i<vtxc;i++,vtxv++) {
    vtxv->x=xa+(i*dx)/vtxc;
    vtxv->y=ya+(i*dy)/vtxc;
    vtxv->tileid=spr->tileid+2;
    vtxv->size=PS_TILESIZE;
    vtxv->ta=0;
    vtxv->pr=vtxv->pg=vtxv->pb=0x80;
    vtxv->a=0xff;
    vtxv->t=0;
    vtxv->xform=AKGL_XFORM_NONE; // Symmetric, doesn't matter.
  }
  return 0;
}

/* Draw head.
 */

static int ps_dragon_draw_head(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr,struct ps_dragon_head *head) {
  int vtxc=PS_DRAGON_NECK_BONE_COUNT+1;
  if (head->firing) vtxc++;
  if (vtxc>vtxa) return vtxc;
  ps_dragon_draw_bones(vtxv,PS_DRAGON_NECK_BONE_COUNT,spr,head->attachx,head->attachy,head->x,head->y);
  
  struct akgl_vtx_maxtile *vtx=vtxv+vtxc-1;
  vtx->x=spr->x+head->x*-SPR->facedir;
  vtx->y=spr->y+head->y;
  if (head->firing) {
    vtx->tileid=spr->tileid+0x13;
  } else {
    vtx->tileid=spr->tileid+3;
  }
  vtx->size=PS_DRAGON_HEAD_SIZE;
  vtx->ta=0;
  vtx->pr=head->pr;
  vtx->pg=head->pg;
  vtx->pb=head->pb;
  vtx->a=0xff;
  if (SPR->facedir>0) {
    vtx->t=(head->angle*-128)/M_PI+0xa0;
  } else {
    vtx->t=(head->angle*128)/M_PI+0x60;
  }
  vtx->xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  if (head->firing) {
    struct akgl_vtx_maxtile *fire=vtx-1;
    memcpy(fire,vtx,sizeof(struct akgl_vtx_maxtile));
    fire->tileid=spr->tileid+0x04;
    if (head->fireanimframe) fire->tileid+=0x10;
    int dx=((head->x-head->attachx)*PS_TILESIZE)/head->magnitude;
    int dy=((head->y-head->attachy)*PS_TILESIZE)/head->magnitude;
    if (SPR->facedir>0) {
      fire->x-=dx;
      fire->y+=dy;
    } else {
      fire->x+=dx;
      fire->y+=dy;
    }
  }
  
  return vtxc;
}

/* Draw leg.
 */

static int ps_dragon_draw_leg(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr,struct ps_dragon_leg *leg) {
  int vtxc=PS_DRAGON_LEG_BONE_COUNT+1;
  if (vtxc>vtxa) return vtxc;
  ps_dragon_draw_bones(vtxv,PS_DRAGON_LEG_BONE_COUNT,spr,leg->attachx,leg->attachy,leg->x,leg->y);
  
  struct akgl_vtx_maxtile *vtx=vtxv+vtxc-1;
  vtx->x=spr->x+leg->x*-SPR->facedir;
  vtx->y=spr->y+leg->y;
  vtx->tileid=spr->tileid+0x12;
  vtx->size=PS_TILESIZE;
  vtx->ta=0;
  vtx->pr=vtx->pg=vtx->pb=0x80;
  vtx->a=0xff;
  vtx->t=0;
  vtx->xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;
  
  return vtxc;
}

/* Draw body.
 */

static int ps_dragon_draw_body(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<4) return 4;

  vtxv[0].x=spr->x-(PS_TILESIZE>>1);
  vtxv[0].y=spr->y-(PS_TILESIZE>>1);
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  memcpy(vtxv+1,vtxv,sizeof(struct akgl_vtx_maxtile));
  memcpy(vtxv+2,vtxv,sizeof(struct akgl_vtx_maxtile)<<1);

  vtxv[1].x+=PS_TILESIZE;
  vtxv[2].y+=PS_TILESIZE;
  vtxv[3].x+=PS_TILESIZE;
  vtxv[3].y+=PS_TILESIZE;

  if (SPR->facedir<0) {
    vtxv[0].tileid=spr->tileid+0x00;
    vtxv[1].tileid=spr->tileid+0x01;
    vtxv[2].tileid=spr->tileid+0x10;
    vtxv[3].tileid=spr->tileid+0x11;
  } else {
    vtxv[0].tileid=spr->tileid+0x01;
    vtxv[1].tileid=spr->tileid+0x00;
    vtxv[2].tileid=spr->tileid+0x11;
    vtxv[3].tileid=spr->tileid+0x10;
  }
  
  return 4;
}

/* Draw wing.
 */

static int ps_dragon_draw_wing(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y-4;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  vtxv[0].xform=(SPR->facedir<0)?AKGL_XFORM_NONE:AKGL_XFORM_FLOP;

  if (SPR->flying) switch (SPR->wingframe) {
    case 0: {
        vtxv[0].y+=PS_TILESIZE>>1;
        vtxv[0].tileid=spr->tileid+0x20;
      } break;
    case 1: 
    case 3: {
        vtxv[0].tileid=spr->tileid+0x21;
      } break;
    case 2: {
        vtxv[0].y-=PS_TILESIZE>>1;
        vtxv[0].tileid=spr->tileid+0x22;
      } break;
  } else {
    vtxv[0].y+=PS_TILESIZE>>1;
    vtxv[0].tileid=spr->tileid+0x20;
  }
  
  return 1;
}

/* Draw, main.
 */

static int _ps_dragon_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=0,err,i;

  if ((err=ps_dragon_draw_body(vtxv+vtxc,vtxa-vtxc,spr))<0) return -1; vtxc+=err;
  for (i=0;i<SPR->legc;i++) {
    if ((err=ps_dragon_draw_leg(vtxv+vtxc,vtxa-vtxc,spr,SPR->legv+i))<0) return -1; vtxc+=err;
  }
  for (i=0;i<SPR->headc;i++) {
    if ((err=ps_dragon_draw_head(vtxv+vtxc,vtxa-vtxc,spr,SPR->headv+i))<0) return -1; vtxc+=err;
  }
  if ((err=ps_dragon_draw_wing(vtxv+vtxc,vtxa-vtxc,spr))<0) return -1; vtxc+=err;

  return vtxc;
}

/* Hurt.
 */

static int _ps_dragon_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_dragon={
  .name="dragon",
  .objlen=sizeof(struct ps_sprite_dragon),

  .radius=PS_TILESIZE-1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_dragon_init,
  .del=_ps_dragon_del,
  .configure=_ps_dragon_configure,
  .get_configure_argument_name=_ps_dragon_get_configure_argument_name,
  .update=_ps_dragon_update,
  .draw=_ps_dragon_draw,
  
  .hurt=_ps_dragon_hurt,

};

/* Add player.
 */
 
int ps_sprite_dragon_add_player(struct ps_sprite *spr,int playerid,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_dragon)) return -1;
  if ((playerid<1)||(playerid>PS_PLAYER_LIMIT)) return -1;
  int i=SPR->playeridc; while (i-->0) {
    if (SPR->playeridv[i]==playerid) return 0;
  }
  
  SPR->playeridv[SPR->playeridc++]=playerid;

  /* If this is our first player, it controls the dragon's body.
   */
  if (SPR->playeridc==1) {
    SPR->input_left=ps_dragon_input(playerid,PS_PLRBTN_LEFT);
    SPR->input_right=ps_dragon_input(playerid,PS_PLRBTN_RIGHT);
    SPR->input_up=ps_dragon_input(playerid,PS_PLRBTN_UP);
    SPR->input_down=ps_dragon_input(playerid,PS_PLRBTN_DOWN);
    SPR->input_fly=ps_dragon_input(playerid,PS_PLRBTN_A);

  /* Any subsequent player gets a head to control.
   */
  } else {
    if (ps_dragon_add_head(spr,playerid,game)<0) return -1;
  }

  return 0;
}

/* Grow body part buffers.
 */

static int ps_dragon_require_head(struct ps_sprite *spr) {
  if (SPR->headc<SPR->heada) return 0;
  int na=SPR->heada+4;
  if (na>INT_MAX/sizeof(struct ps_dragon_head)) return -1;
  void *nv=realloc(SPR->headv,sizeof(struct ps_dragon_head)*na);
  if (!nv) return -1;
  SPR->headv=nv;
  SPR->heada=na;
  return 0;
}

static int ps_dragon_require_leg(struct ps_sprite *spr) {
  if (SPR->legc<SPR->lega) return 0;
  int na=SPR->lega+4;
  if (na>INT_MAX/sizeof(struct ps_dragon_leg)) return -1;
  void *nv=realloc(SPR->legv,sizeof(struct ps_dragon_leg)*na);
  if (!nv) return -1;
  SPR->legv=nv;
  SPR->lega=na;
  return 0;
}

/* Reposition heads.
 * The head attachment points describe an arc along the dragon's back.
 */

static int ps_dragon_reposition_heads(struct ps_sprite *spr) {
  struct ps_dragon_head *head=SPR->headv;
  int i=SPR->headc; for (;i-->0;head++) {
    double angle=PS_DRAGON_NECK_ANGLE_MIN+((i+1)*(PS_DRAGON_NECK_ANGLE_MAX-PS_DRAGON_NECK_ANGLE_MIN))/(SPR->headc+1);
    head->attachx= sin(angle)*PS_DRAGON_NECK_DISTANCE;
    head->attachy=-cos(angle)*PS_DRAGON_NECK_DISTANCE;
    head->angle=angle;
    head->magnitude=PS_DRAGON_HEAD_MAGNITUDE_INITIAL;
    ps_dragon_head_calculate_position(head);
  }
  return 0;
}

/* Add body parts.
 */

static int ps_dragon_add_head(struct ps_sprite *spr,int playerid,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_dragon)) return -1;
  if (ps_dragon_require_head(spr)<0) return -1;
  struct ps_dragon_head *head=SPR->headv+SPR->headc++;
  memset(head,0,sizeof(struct ps_dragon_head));
  head->playerid=playerid;

  if ((playerid>=1)&&(playerid<=PS_PLAYER_LIMIT)) {
    struct ps_player *player=game->playerv[playerid-1];
    if (player&&player->plrdef) {
      uint32_t bodyrgba=player->plrdef->palettev[player->palette].rgba_body;
      head->pr=bodyrgba>>24;
      head->pg=bodyrgba>>16;
      head->pb=bodyrgba>>8;
    }
    head->input_clock=ps_dragon_input(playerid,PS_PLRBTN_RIGHT);
    head->input_counter=ps_dragon_input(playerid,PS_PLRBTN_LEFT);
    head->input_forward=ps_dragon_input(playerid,PS_PLRBTN_UP);
    head->input_backward=ps_dragon_input(playerid,PS_PLRBTN_DOWN);
    head->input_fire=ps_dragon_input(playerid,PS_PLRBTN_A);
  }

  if (ps_dragon_reposition_heads(spr)<0) return -1;

  return 0;
}

static int ps_dragon_add_leg(struct ps_sprite *spr,int x,int y) {
  if (!spr||(spr->type!=&ps_sprtype_dragon)) return -1;
  if (ps_dragon_require_leg(spr)<0) return -1;
  struct ps_dragon_leg *leg=SPR->legv+SPR->legc++;
  memset(leg,0,sizeof(struct ps_dragon_leg));

  leg->attachx=x;
  leg->attachy=y;
  leg->x=x;
  leg->y=y+(PS_TILESIZE>>1);

  return 0;
}
