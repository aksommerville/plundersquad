#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_RABBIT_RENDER_OFFSET_X -10
#define PS_RABBIT_RENDER_OFFSET_Y -13

#define PS_RABBIT_PHASE_IDLE          0 /* [default] Sitting still, contemplating our next move. */
#define PS_RABBIT_PHASE_HOP           1 /* Hopping to a new location. */
#define PS_RABBIT_PHASE_LICK          2 /* Sticking out tongue to grab something. */
#define PS_RABBIT_PHASE_BURN          3 /* Breathing fire. */
#define PS_RABBIT_PHASE_SPIT          4 /* Spitting out whatever's in our belly. */

#define PS_RABBIT_COUNTER_INIT   30 /* Idle time at initialization (constant). */
#define PS_RABBIT_IDLE_TIME_MIN  40
#define PS_RABBIT_IDLE_TIME_MAX  90
#define PS_RABBIT_HOP_TIME_MIN   40
#define PS_RABBIT_HOP_TIME_MAX   90
#define PS_RABBIT_LICK_TIME      60
#define PS_RABBIT_BURN_TIME      60
#define PS_RABBIT_SPIT_TIME      30

#define PS_RABBIT_FLAME_ANIM_DELAY 6

#define PS_RABBIT_FLAME_EXTENSION_MIN    (PS_TILESIZE/2)
#define PS_RABBIT_FLAME_EXTENSION_MAX    (PS_TILESIZE*3)
#define PS_RABBIT_TONGUE_EXTENSION_MIN   (PS_TILESIZE/2)
#define PS_RABBIT_TONGUE_EXTENSION_MAX   (PS_TILESIZE*2)
#define PS_RABBIT_FLAME_ANCHOR_X   -11
#define PS_RABBIT_FLAME_ANCHOR_Y    -3
#define PS_RABBIT_FLAME_W            4.0

#define PS_RABBIT_HOP_SPEED_MIN    10
#define PS_RABBIT_HOP_SPEED_MAX    20
#define PS_RABBIT_HOP_SPEED_SCALE  10.0

/* There are three elective things we can do: hop, lick, or burn.
 * For now, we won't do any real decision-making. Just declaring odds.
 * These numbers are purely relative; they don't have to add up to 100 or anything.
 * "ODDS" with empty belly; "FODDS" with full belly.
 */
#define PS_RABBIT_ODDS_HOP   50
#define PS_RABBIT_ODDS_LICK  20
#define PS_RABBIT_ODDS_BURN  10
#define PS_RABBIT_ODDS_IDLE   1
#define PS_RABBIT_FODDS_HOP  50
#define PS_RABBIT_FODDS_SPIT 30
#define PS_RABBIT_FODDS_IDLE  1

#define PS_RABBIT_INVINCIBLE_TIME 60

#define PS_RABBIT_GRPMASK_BELLY ( \
  (1<<PS_SPRGRP_KEEPALIVE)| \
0)

/* Private sprite object.
 */

struct ps_sprite_rabbit {
  struct ps_sprite hdr;
  int phase;
  int counter;
  int flameanimcounter;
  int flameanimframe;
  int flop; // 0=left, 1=right. Graphics are oriented left.
  int extension; // Extent of tongue or fire, in phase BURN or LICK.
  double dx,dy; // In HOP phase.
  int hp;
  int invincible;
  struct ps_sprgrp *slave; // 0 or 1 member. Content of tongue or belly, depending on my phase.
  uint32_t slave_grpmask;
  uint16_t slave_impassable;
};

#define SPR ((struct ps_sprite_rabbit*)spr)

/* Delete.
 */

static void _ps_rabbit_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->slave);
  ps_sprgrp_del(SPR->slave);
}

/* Initialize.
 */

static int _ps_rabbit_init(struct ps_sprite *spr) {

  SPR->hp=5; // TODO Should rabbit HP be configurable?
  SPR->phase=PS_RABBIT_PHASE_IDLE;
  SPR->counter=PS_RABBIT_COUNTER_INIT;
  if (!(SPR->slave=ps_sprgrp_new())) return -1;

  return 0;
}

/* Configure.
 */

static int _ps_rabbit_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Enter IDLE phase.
 */

static int ps_rabbit_begin_IDLE(struct ps_sprite *spr,struct ps_game *game) {

  /* Remain in IDLE for a random duration within limits. */
  SPR->phase=PS_RABBIT_PHASE_IDLE;
  SPR->counter=PS_RABBIT_IDLE_TIME_MIN+(rand()%(PS_RABBIT_IDLE_TIME_MAX-PS_RABBIT_IDLE_TIME_MIN+1));

  /* Face whichever direction has more heroes.
   * If equal, face towards the screen center.
   */
  int leftc=0,rightc=0,i;
  for (i=game->grpv[PS_SPRGRP_HERO].sprc;i-->0;) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if (SPR->slave->sprc&&(SPR->slave->sprv[0]==hero)) ; // Ignore hero if it's in my belly.
    else if (hero->x<spr->x) leftc++;
    else rightc++;
  }
  if (leftc>rightc) {
    SPR->flop=0;
  } else if (leftc<rightc) {
    SPR->flop=1;
  } else if (spr->x<PS_SCREENW>>1) {
    SPR->flop=1;
  } else {
    SPR->flop=0;
  }
  
  return 0;
}

/* Enter HOP phase.
 */

static int ps_rabbit_begin_HOP(struct ps_sprite *spr,struct ps_game *game) {

  SPR->phase=PS_RABBIT_PHASE_HOP;
  SPR->counter=PS_RABBIT_HOP_TIME_MIN+(rand()%(PS_RABBIT_HOP_TIME_MAX-PS_RABBIT_HOP_TIME_MIN+1));

  /* Random direction and speed. */
  double direction=(rand()%628)/100.0;
  double speed=(PS_RABBIT_HOP_SPEED_MIN+(rand()%(PS_RABBIT_HOP_SPEED_MAX-PS_RABBIT_HOP_SPEED_MIN+1)))/PS_RABBIT_HOP_SPEED_SCALE;
  SPR->dx=cos(direction)*speed;
  SPR->dy=sin(direction)*speed;

  /* (dx) must agree with (flop). */
  if (SPR->flop) {
    if (SPR->dx<0.0) SPR->dx=-SPR->dx;
  } else {
    if (SPR->dx>0.0) SPR->dx=-SPR->dx;
  }

  /* If we're in the top or bottom third of the screen, move towards vertical center. */
  if (spr->y<PS_SCREENH/3) {
    if (SPR->dy<0.0) SPR->dy=-SPR->dy;
  } else if (spr->y>(PS_SCREENH*2)/3) {
    if (SPR->dy>0.0) SPR->dy=-SPR->dy;
  }

  return 0;
}

/* Enter BURN phase.
 */

static int ps_rabbit_begin_BURN(struct ps_sprite *spr,struct ps_game *game) {

  PS_SFX_RABBIT_FIRE

  SPR->phase=PS_RABBIT_PHASE_BURN;
  SPR->counter=PS_RABBIT_BURN_TIME;

  SPR->extension=PS_RABBIT_FLAME_EXTENSION_MIN;

  return 0;
}

/* Enter LICK phase.
 */

static int ps_rabbit_begin_LICK(struct ps_sprite *spr,struct ps_game *game) {

  PS_SFX_RABBIT_TONGUE

  SPR->phase=PS_RABBIT_PHASE_LICK;
  SPR->counter=PS_RABBIT_LICK_TIME;

  SPR->extension=PS_RABBIT_TONGUE_EXTENSION_MIN;

  return 0;
}

/* Enter SPIT phase.
 */

static int ps_rabbit_begin_SPIT(struct ps_sprite *spr,struct ps_game *game) {

  SPR->phase=PS_RABBIT_PHASE_SPIT;
  SPR->counter=PS_RABBIT_SPIT_TIME;

  if (SPR->slave->sprc>0) {
    struct ps_sprite *pumpkin=SPR->slave->sprv[0];

    PS_SFX_RABBIT_SPIT

    pumpkin->y=spr->y;
    if (SPR->flop) {
      pumpkin->x=spr->x+PS_TILESIZE*2;
    } else {
      pumpkin->x=spr->x-PS_TILESIZE*2;
    }

    if (ps_sprite_rabbit_drop_slave(spr,game)<0) return -1;
  }

  return 0;
}

/* End of IDLE phase, select our next move.
 */

static int ps_rabbit_choose_phase(struct ps_sprite *spr,struct ps_game *game) {

  /* Full belly: HOP, SPIT, IDLE. */
  if (SPR->slave->sprc) {
    const int odds_total=PS_RABBIT_FODDS_HOP+PS_RABBIT_FODDS_SPIT+PS_RABBIT_FODDS_IDLE;
    int odds_selection=rand()%odds_total;
    if ((odds_selection-=PS_RABBIT_FODDS_HOP)<0) return ps_rabbit_begin_HOP(spr,game);
    if ((odds_selection-=PS_RABBIT_FODDS_SPIT)<0) return ps_rabbit_begin_SPIT(spr,game);

  /* Empty belly: HOP, BURN, LICK, IDLE. */
  } else {
    const int odds_total=PS_RABBIT_ODDS_HOP+PS_RABBIT_ODDS_BURN+PS_RABBIT_ODDS_LICK+PS_RABBIT_ODDS_IDLE;
    int odds_selection=rand()%odds_total;
    if ((odds_selection-=PS_RABBIT_ODDS_HOP)<0) return ps_rabbit_begin_HOP(spr,game);
    if ((odds_selection-=PS_RABBIT_ODDS_BURN)<0) return ps_rabbit_begin_BURN(spr,game);
    if ((odds_selection-=PS_RABBIT_ODDS_LICK)<0) return ps_rabbit_begin_LICK(spr,game);
  }

  /* Idle is always an option, and it's also the default. */
  return ps_rabbit_begin_IDLE(spr,game);
}

/* Leaving LICK phase, if something is on our tongue we must swallow it.
 */

static int ps_rabbit_swallow_pumpkin(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->slave->sprc<1) return 0;
  struct ps_sprite *pumpkin=SPR->slave->sprv[0];

  PS_SFX_RABBIT_SWALLOW

  if (ps_game_set_group_mask_for_sprite(game,pumpkin,PS_RABBIT_GRPMASK_BELLY)<0) return -1;
  
  return 0;
}

/* End phase, begin the next one.
 */

static int ps_rabbit_end_phase(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_RABBIT_PHASE_IDLE: return ps_rabbit_choose_phase(spr,game);
    case PS_RABBIT_PHASE_HOP: return ps_rabbit_begin_IDLE(spr,game);
    case PS_RABBIT_PHASE_LICK: {
        if (ps_rabbit_swallow_pumpkin(spr,game)<0) return -1;
        return ps_rabbit_begin_IDLE(spr,game);
      }
    case PS_RABBIT_PHASE_BURN: return ps_rabbit_begin_IDLE(spr,game);
    case PS_RABBIT_PHASE_SPIT: return ps_rabbit_begin_IDLE(spr,game);
    default: return ps_rabbit_begin_IDLE(spr,game);
  }
  return -1;
}

/* Update movement in HOP phase.
 */

static int ps_rabbit_update_HOP(struct ps_sprite *spr,struct ps_game *game) {

  spr->x+=SPR->dx;
  spr->y+=SPR->dy;

  /* Force within screen bounds. */
  if (spr->y<spr->radius) spr->y=spr->radius;
  else if (spr->y>PS_SCREENH-spr->radius) spr->y=PS_SCREENH-spr->radius;
  if (spr->x<spr->radius) spr->x=spr->radius;
  else if (spr->x>PS_SCREENW-spr->radius) spr->x=PS_SCREENW-spr->radius;
  
  return 0;
}

/* Set extent of tongue or flame.
 */

static int ps_rabbit_set_extension(struct ps_sprite *spr,int min,int max,int p,int c) {
  int halfc=c>>1;
  if (p>halfc) p=c-p;
  SPR->extension=min+(p*(max-min))/halfc;
  return 0;
}

static int ps_rabbit_get_extension(const struct ps_sprite *spr) {
  if (SPR->flop) {
    return spr->x-PS_RABBIT_FLAME_ANCHOR_X+SPR->extension;
  } else {
    return spr->x+PS_RABBIT_FLAME_ANCHOR_X-SPR->extension;
  }
}

/* In LICK phase, connect with a lickable sprite.
 */

static int ps_rabbit_grab_pumpkin(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *pumpkin) {

  if (SPR->slave->sprc) return -1;
  if (ps_sprgrp_add_sprite(SPR->slave,pumpkin)<0) return -1;
  if (ps_sprite_set_master(pumpkin,spr,game)<0) return -1;
  PS_SFX_RABBIT_GRAB

  SPR->slave_grpmask=ps_game_get_group_mask_for_sprite(game,pumpkin);
  SPR->slave_impassable=pumpkin->impassable;

  // Experimentally, remove the FRAGILE group now instead of waiting to swallow. So we don't accidentally kill it ourselves.
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,pumpkin)<0) return -1;
  
  return 0;
}

/* Update tongue in LICK phase.
 */

static int ps_rabbit_update_LICK(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_rabbit_set_extension(spr,PS_RABBIT_TONGUE_EXTENSION_MIN,PS_RABBIT_TONGUE_EXTENSION_MAX,SPR->counter,PS_RABBIT_LICK_TIME)<0) return -1;

  /* If we have a pumpkin already, set its position and we're done.
   */
  if (SPR->slave->sprc) {
    struct ps_sprite *pumpkin=SPR->slave->sprv[0];
    pumpkin->x=ps_rabbit_get_extension(spr);
    pumpkin->y=spr->y+PS_RABBIT_FLAME_ANCHOR_Y;
    return 0;
  }

  /* Look for lickable sprites.
   * We'll only lick heroes for now. No problem to revisit this decision and include others later.
   * Rabbits *can* eat dragons, and it's hilarious.
   */
  struct ps_sprgrp *lickable=game->grpv+PS_SPRGRP_HERO;
  if (lickable->sprc>0) {
    double x=ps_rabbit_get_extension(spr);
    double y=spr->y+PS_RABBIT_FLAME_ANCHOR_Y;
    int i=lickable->sprc; for (;i-->0;) {
      struct ps_sprite *candidate=lickable->sprv[i];
      double dx=x-candidate->x;
      if ((dx<-candidate->radius)||(dx>candidate->radius)) continue;
      double dy=y-candidate->y;
      if ((dy<-candidate->radius)||(dy>candidate->radius)) continue;
      // Is it worth the extra check for circular candidates? I don't think anyone will notice.
      return ps_rabbit_grab_pumpkin(spr,game,candidate);
    }
  }
  
  return 0;
}

/* Update flame in BURN phase.
 */

static int ps_rabbit_update_BURN(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_rabbit_set_extension(spr,PS_RABBIT_FLAME_EXTENSION_MIN,PS_RABBIT_FLAME_EXTENSION_MAX,SPR->counter,PS_RABBIT_BURN_TIME)<0) return -1;

  /* Get the extent of the flame.
   * Unlike tongue which is a single point, the flame is a rectangle.
   */
  struct ps_fbox flamebox;
  flamebox.n=spr->y+PS_RABBIT_FLAME_ANCHOR_Y-PS_RABBIT_FLAME_W/2.0;
  flamebox.s=flamebox.n+PS_RABBIT_FLAME_W;
  int x=ps_rabbit_get_extension(spr);
  if (x<spr->x) {
    flamebox.w=x;
    flamebox.e=spr->x;
  } else {
    flamebox.w=spr->x;
    flamebox.e=x;
  }

  /* Hurt any fragile sprites colliding with the box, except ourselves of course.
   */
  struct ps_sprgrp *burnable=game->grpv+PS_SPRGRP_FRAGILE;
  int i=burnable->sprc; for (;i-->0;) {
    struct ps_sprite *candidate=burnable->sprv[i];
    if (candidate==spr) continue;
    if (!ps_sprite_collide_fbox(candidate,&flamebox)) continue;
    if (ps_sprite_receive_damage(game,candidate,spr)<0) return -1;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_rabbit_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->invincible) SPR->invincible--;

  /* Flame animation counter runs constantly, whether we are using it or not. */
  if (++(SPR->flameanimcounter)>=PS_RABBIT_FLAME_ANIM_DELAY) {
    SPR->flameanimcounter=0;
    if (++(SPR->flameanimframe)>=4) {
      SPR->flameanimframe=0;
    }
  }

  /* Regardless of the current phase, (counter) counts down to the next one. */
  if (--(SPR->counter)<=0) {
    if (ps_rabbit_end_phase(spr,game)<0) return -1;
  }

  /* Perform phase-specific updates. */
  switch (SPR->phase) {
    case PS_RABBIT_PHASE_IDLE: break;
    case PS_RABBIT_PHASE_HOP: if (ps_rabbit_update_HOP(spr,game)<0) return -1; break;
    case PS_RABBIT_PHASE_LICK: if (ps_rabbit_update_LICK(spr,game)<0) return -1; break;
    case PS_RABBIT_PHASE_BURN: if (ps_rabbit_update_BURN(spr,game)<0) return -1; break;
    case PS_RABBIT_PHASE_SPIT: break;
  }

  return 0;
}

/* Draw.
 */

static int ps_rabbit_measure_tongue(const struct ps_sprite *spr) {
  switch (SPR->phase) {
    case PS_RABBIT_PHASE_LICK:
    case PS_RABBIT_PHASE_BURN: {
        // Tweak this formula based on the graphics, so the shaft always reaches the mouth and doesn't show up behind our back.
        return SPR->extension/PS_TILESIZE+2;
      }
  }
  return 0;
}

static void ps_rabbit_draw_tongue(struct akgl_vtx_maxtile *vtxv,int vtxc,const struct ps_sprite *spr) {

  vtxv->size=PS_TILESIZE;
  vtxv->ta=0x00;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0x00;
  vtxv->y=spr->y+PS_RABBIT_FLAME_ANCHOR_Y;

  int dx;
  if (SPR->flop) {
    dx=-PS_TILESIZE;
    vtxv->xform=AKGL_XFORM_FLOP;
    vtxv->x=spr->x-PS_RABBIT_FLAME_ANCHOR_X+SPR->extension;
  } else {
    dx=PS_TILESIZE;
    vtxv->xform=AKGL_XFORM_NONE;
    vtxv->x=spr->x+PS_RABBIT_FLAME_ANCHOR_X-SPR->extension;
  }

  uint8_t shaft_tileid;
  if (SPR->phase==PS_RABBIT_PHASE_BURN) {
    vtxv->tileid=spr->tileid+0x20+SPR->flameanimframe;
    shaft_tileid=vtxv->tileid+4;
  } else {
    vtxv->tileid=spr->tileid+0x28;
    shaft_tileid=vtxv->tileid+1;
  }

  int i=1; for (;i<vtxc;i++) {
    struct akgl_vtx_maxtile *vtx=vtxv+i;
    memcpy(vtx,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtx->x+=i*dx;
    vtx->tileid=shaft_tileid;
  }
}

static void ps_rabbit_draw_body(struct akgl_vtx_maxtile *vtxv,const struct ps_sprite *spr) {

  /* Constant properties of the first vertex. */
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0x00;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0x00;
  vtxv->xform=SPR->flop?AKGL_XFORM_FLOP:AKGL_XFORM_NONE;

  /* Mutable properties of the first vertex. */
  vtxv->x=spr->x+PS_RABBIT_RENDER_OFFSET_X;
  vtxv->y=spr->y+PS_RABBIT_RENDER_OFFSET_Y;
  switch (SPR->phase) {
    case PS_RABBIT_PHASE_HOP: if (SPR->slave->sprc) vtxv->tileid=spr->tileid+0x08; else vtxv->tileid=spr->tileid+0x02; break;
    case PS_RABBIT_PHASE_BURN: vtxv->tileid=spr->tileid+0x04; break;
    case PS_RABBIT_PHASE_LICK: vtxv->tileid=spr->tileid+0x04; break;
    case PS_RABBIT_PHASE_SPIT: vtxv->tileid=spr->tileid+0x04; break;
    default: if (SPR->slave->sprc) vtxv->tileid=spr->tileid+0x06; else vtxv->tileid=spr->tileid;
  }
  if (SPR->invincible) {
    vtxv->tr=0xff; vtxv->tg=0x00; vtxv->tb=0x00;
    vtxv->ta=(SPR->invincible*255)/PS_RABBIT_INVINCIBLE_TIME;
  }

  /* Copy first to second, third, and fourth; and adjust position and tileid */
  int col=1,row=0;
  int i; for (i=1;i<4;i++) {
    memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtxv[i].tileid+=(row<<4)+col;
    vtxv[i].x+=PS_TILESIZE*col;
    vtxv[i].y+=PS_TILESIZE*row;
    if (++col>1) { col=0; row++; }
  }

  /* If we are facing right (ie 'flopped'), swap horizontal positions. */
  if (SPR->flop) {
    int16_t tmp;
    tmp=vtxv[0].x; vtxv[0].x=vtxv[1].x; vtxv[1].x=tmp;
    tmp=vtxv[2].x; vtxv[2].x=vtxv[3].x; vtxv[3].x=tmp;
  }
}

static int _ps_rabbit_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int tonguevtxc=ps_rabbit_measure_tongue(spr);
  int vtxc=tonguevtxc+4; // 4 for the body.
  if (vtxc>vtxa) return vtxc;

  ps_rabbit_draw_tongue(vtxv,tonguevtxc,spr);
  ps_rabbit_draw_body(vtxv+tonguevtxc,spr);

  return vtxc;
}

/* Hurt.
 */

static int _ps_rabbit_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {

  if (SPR->invincible) return 0;

  SPR->hp--;
  if (!SPR->hp) {

    PS_SFX_MONSTER_DEAD
  
    /* IMPORTANT! If we have something in the belly at the moment of death, spit it out. */
    ps_sprite_rabbit_drop_slave(spr,game);

    SPR->invincible=INT_MAX;
    if (ps_game_create_fireworks(game,spr->x,spr->y)<0) return -1;
    if (ps_game_create_prize(game,spr->x,spr->y)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    if (ps_game_check_deathgate(game)<0) return -1;
    if (ps_game_report_kill(game,assailant,spr)<0) return -1;

    return 0;
  }

  PS_SFX_MONSTER_HURT
  SPR->invincible=PS_RABBIT_INVINCIBLE_TIME;

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_rabbit={
  .name="rabbit",
  .objlen=sizeof(struct ps_sprite_rabbit),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_rabbit_init,
  .del=_ps_rabbit_del,
  .configure=_ps_rabbit_configure,
  .update=_ps_rabbit_update,
  .draw=_ps_rabbit_draw,
  
  .hurt=_ps_rabbit_hurt,

};

/* Drop slave.
 */
 
int ps_sprite_rabbit_drop_slave(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_rabbit)) return -1;
  if (SPR->slave&&(SPR->slave->sprc>0)) {
    struct ps_sprite *slave=SPR->slave->sprv[0];
    if (ps_game_set_group_mask_for_sprite(game,slave,SPR->slave_grpmask)<0) return -1;
    slave->impassable=SPR->slave_impassable;
    if (ps_sprgrp_clear(SPR->slave)<0) return -1;
  }
  return 0;
}
