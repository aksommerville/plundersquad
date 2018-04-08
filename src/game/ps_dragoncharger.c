#include "ps.h"
#include "ps_dragoncharger.h"
#include "ps_game.h"
#include "ps_sprite.h"
#include "sprites/ps_sprite_hero.h"
#include "res/ps_resmgr.h"

#define PS_DRAGONCHARGER_CHARGE_INCREMENT  12 /* Add so much charge to one player per keystroke. */
#define PS_DRAGONCHARGER_CHARGE_DECREMENT   1 /* Remove so much charge for each player on each update. */
#define PS_DRAGONCHARGER_CHARGE_CEILING   150 /* Clamp charge here on addition. */
#define PS_DRAGONCHARGER_CHARGE_THRESHOLD  40 /* Average charge must exceed this to actuate. */
#define PS_DRAGONCHARGER_CHARGE_MINIMUM     1 /* Each active player must have at least this charge to actuate. */

#define PS_DRAGONCHARGER_MINIMUM_PARTY_SIZE 2
#define PS_DRAGONCHARGER_PARTY_DISTANCE PS_TILESIZE

#define PS_DRAGONCHARGER_SPRDEFID 23

/* Object lifecycle.
 */
 
struct ps_dragoncharger *ps_dragoncharger_new() {
  struct ps_dragoncharger *charger=calloc(1,sizeof(struct ps_dragoncharger));
  if (!charger) return 0;

  charger->refc=1;

  return charger;
}

void ps_dragoncharger_del(struct ps_dragoncharger *charger) {
  if (!charger) return;
  if (charger->refc-->1) return;

  free(charger);
}

int ps_dragoncharger_ref(struct ps_dragoncharger *charger) {
  if (!charger) return -1;
  if (charger->refc<1) return -1;
  if (charger->refc==INT_MAX) return -1;
  charger->refc++;
  return 0;
}

/* Instantiate dragon sprite.
 */

static struct ps_sprite *ps_dragoncharger_instantiate_dragon(struct ps_dragoncharger *charger,struct ps_game *game) {

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_DRAGONCHARGER_SPRDEFID);
  if (!sprdef) {
    ps_log(RES,ERROR,"sprdef:%d not found for dragon",PS_DRAGONCHARGER_SPRDEFID);
    return 0;
  }

  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,0,0);
  if (!spr) return 0;

  return spr;
}

/* Actuate!
 * Eliminate the given sprites and replace them with a dragon controlled by all of the involved players.
 */

static int ps_dragoncharger_actuate(struct ps_dragoncharger *charger,struct ps_game *game,struct ps_sprite **partyv,int partyc) {
  ps_log(GAME,TRACE,"%s",__func__);

  struct ps_sprite *dragon=ps_dragoncharger_instantiate_dragon(charger,game);
  if (!dragon) return -1;

  double x=0.0,y=0.0;
  int i=partyc; while (i-->0) {
    struct ps_sprite *spr=partyv[i];
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    int playerid=ps_hero_get_playerid(spr);
    if ((playerid<1)||(playerid>PS_PLAYER_LIMIT)) continue;
    playerid--;
    charger->charge[playerid]=0; // Just to be safe.
    if (ps_sprite_dragon_add_player(dragon,playerid+1,game)<0) return -1;
    x+=spr->x;
    y+=spr->y;
  }

  dragon->x=x/partyc;
  dragon->y=y/partyc;

  return 0;
}

/* Identify sprites in the given list which intersect the given sprite.
 */

static int ps_dragoncharger_identify_companions(struct ps_sprite **companionv,struct ps_sprite *focus,struct ps_sprite **candidatev,int candidatec) {
  int companionc=0;
  int i=candidatec; for (;i-->0;candidatev++) {
    struct ps_sprite *candidate=*candidatev;
    int dx=candidate->x-focus->x;
    if ((dx<-PS_DRAGONCHARGER_PARTY_DISTANCE)||(dx>PS_DRAGONCHARGER_PARTY_DISTANCE)) continue;
    int dy=candidate->y-focus->y;
    if ((dy<-PS_DRAGONCHARGER_PARTY_DISTANCE)||(dy>PS_DRAGONCHARGER_PARTY_DISTANCE)) continue;
    companionv[companionc++]=candidate;
    if (companionc>=PS_PLAYER_LIMIT-1) break;
  }
  return companionc;
}

/* Look for possible activity and actuate as warranted.
 */

static int ps_dragoncharger_check_heroes(struct ps_dragoncharger *charger,struct ps_game *game,struct ps_sprgrp *grp) {
  int already_checked[PS_PLAYER_LIMIT]={0};
  int i=0; for (;i<grp->sprc;i++) {
  
    struct ps_sprite *spr=grp->sprv[i];
    int playerid=ps_hero_get_playerid(spr);
    if ((playerid<1)||(playerid>PS_PLAYER_LIMIT)) continue;
    playerid--;
    if (already_checked[playerid]) continue;
    if (charger->charge[playerid]<PS_DRAGONCHARGER_CHARGE_MINIMUM) continue;

    struct ps_sprite *companionv[PS_PLAYER_LIMIT];
    int companionc=ps_dragoncharger_identify_companions(companionv,spr,grp->sprv+i+1,grp->sprc-i-1);
    int partysize=1+companionc;
    if (partysize<PS_DRAGONCHARGER_MINIMUM_PARTY_SIZE) continue;

    int totalcharge=charger->charge[playerid];
    int companioni=0; for (;companioni<companionc;companioni++) {
      struct ps_sprite *companion=companionv[companioni];
      int cplayerid=ps_hero_get_playerid(companion);
      if ((cplayerid<1)||(cplayerid>PS_PLAYER_LIMIT)) continue;
      cplayerid--;
      already_checked[cplayerid]=1;
      if (charger->charge[cplayerid]<PS_DRAGONCHARGER_CHARGE_MINIMUM) {
        totalcharge=0;
        break;
      }
      totalcharge+=charger->charge[cplayerid];
    }

    companionv[companionc++]=spr;
    int averagecharge=totalcharge/partysize;
    if (averagecharge<PS_DRAGONCHARGER_CHARGE_THRESHOLD) {
      while (companionc-->0) ps_hero_set_dragon_charge(companionv[companionc],averagecharge,PS_DRAGONCHARGER_CHARGE_THRESHOLD);
    } else {
      if (ps_dragoncharger_actuate(charger,game,companionv,companionc)<0) return -1;
    }

  }
  return 0;
}

/* XXX TEMP: Log all changes to charge.
 */

static void ps_dragoncharger_dump_charge(const struct ps_dragoncharger *charger) {
  char msg[256];
  int msgc=0,i,total=0;
  for (i=0;i<PS_PLAYER_LIMIT;i++) {
    msgc+=snprintf(msg+msgc,sizeof(msg)-msgc,"%3d ",charger->charge[i]);
    total+=charger->charge[i];
  }
  msgc+=snprintf(msg+msgc,sizeof(msg)-msgc,"; total %d",total);
  ps_log(GAME,DEBUG,"Dragon charger: %.*s",msgc,msg);
}

/* Update.
 */

int ps_dragoncharger_update(struct ps_dragoncharger *charger,struct ps_game *game) {
  if (!charger||!game) return -1;

  /* Decrement all charges. */
  int total=0;
  int i=0; for (;i<PS_PLAYER_LIMIT;i++) {
    charger->charge[i]-=PS_DRAGONCHARGER_CHARGE_DECREMENT;
    if (charger->charge[i]<0) {
      charger->charge[i]=0;
    }
    total+=charger->charge[i];
  }

  /* XXX TEMP: Log all changes to charge. */
  if (memcmp(charger->charge,charger->pvcharge,sizeof(charger->charge))) {
    ps_dragoncharger_dump_charge(charger);
    memcpy(charger->pvcharge,charger->charge,sizeof(charger->charge));
  }

  /* The vast majority of the time, nothing is going on.
   * If the total charge is less than the average threshold, we definitely can't do anything.
   * So get out fast in that easily-detectable case.
   */
  if (total<PS_DRAGONCHARGER_CHARGE_THRESHOLD) return 0;

  /* Look for actuable charges among the hero group.
   */
  if (ps_dragoncharger_check_heroes(charger,game,game->grpv+PS_SPRGRP_HERO)<0) return -1;
  
  return 0;
}

/* Apply charge.
 */
 
int ps_dragoncharger_charge(struct ps_dragoncharger *charger,struct ps_game *game,int playerid) {
  if (!charger) return -1;
  if ((playerid<1)||(playerid>PS_PLAYER_LIMIT)) return 0; // No such player, whatever.
  playerid--;
  charger->charge[playerid]+=PS_DRAGONCHARGER_CHARGE_INCREMENT;
  if (charger->charge[playerid]>PS_DRAGONCHARGER_CHARGE_CEILING) {
    charger->charge[playerid]=PS_DRAGONCHARGER_CHARGE_CEILING;
  }
  return 0;
}
