#include "ps.h"
#include "ps_stats.h"

/* Object lifecycle.
 */
 
struct ps_stats *ps_stats_new() {
  struct ps_stats *stats=calloc(1,sizeof(struct ps_stats));
  if (!stats) return 0;
  stats->refc=1;
  return stats;
}

void ps_stats_del(struct ps_stats *stats) {
  if (!stats) return;
  if (stats->refc-->1) return;
  if (stats->deedv) free(stats->deedv);
  free(stats);
}

int ps_stats_ref(struct ps_stats *stats) {
  if (!stats) return -1;
  if (stats->refc<1) return -1;
  if (stats->refc==INT_MAX) return -1;
  stats->refc++;
  return 0;
}

/* Clear.
 */

int ps_stats_clear(struct ps_stats *stats) {
  if (!stats) return -1;
  
  stats->playtime=0;
  stats->framec_since_treasure=0;
  stats->deedc=0;
  memset(stats->playerv,0,sizeof(stats->playerv));

  return 0;
}

/* Dump for debug.
 */
 
void ps_stats_dump(const struct ps_stats *stats) {
  if (!stats) return;
  ps_log(GAME,DEBUG,"----- begin stats dump -----");
  
  ps_log(GAME,DEBUG,"  playtime: %d",stats->playtime);
  ps_log(GAME,DEBUG,"  framec_since_treasure: %d",stats->framec_since_treasure);

  struct ps_stats_player zeroplayer={0};
  const struct ps_stats_player *player=stats->playerv;
  int i=0; for (;i<PS_PLAYER_LIMIT;i++,player++) {
    if (!memcmp(player,&zeroplayer,sizeof(struct ps_stats_player))) continue;
    ps_log(GAME,DEBUG,"--- Player %d ---",i+1);
    ps_log(GAME,DEBUG,"  stepc: %d",player->stepc);
    ps_log(GAME,DEBUG,"  killc_monster: %d",player->killc_monster);
    ps_log(GAME,DEBUG,"  killc_hero: %d",player->killc_hero);
    ps_log(GAME,DEBUG,"  deathc: %d",player->deathc);
    ps_log(GAME,DEBUG,"  switchc: %d",player->switchc);
    ps_log(GAME,DEBUG,"  framec_alive: %d",player->framec_alive);
    ps_log(GAME,DEBUG,"  stepc_since_rebirth: %d",player->stepc_since_rebirth);
    ps_log(GAME,DEBUG,"  framec_since_rebirth: %d",player->framec_since_rebirth);
    ps_log(GAME,DEBUG,"  treasurec: %d",player->treasurec);
  }

  ps_log(GAME,DEBUG,"----- end stats dump -----");
}

/* Deeds.
 */

static int ps_stats_search_deed(const struct ps_stats *stats,int x,int y) {
  int lo=0,hi=stats->deedc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (x<stats->deedv[ck].x) hi=ck;
    else if (x>stats->deedv[ck].x) lo=ck+1;
    else if (y<stats->deedv[ck].y) hi=ck;
    else if (y>stats->deedv[ck].y) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_stats_insert_deed(struct ps_stats *stats,int p,int x,int y) {
  if ((p<0)||(p>stats->deedc)) return -1;
  if (stats->deedc>=stats->deeda) {
    int na=stats->deeda+8;
    if (na>INT_MAX/sizeof(struct ps_deed)) return -1;
    void *nv=realloc(stats->deedv,sizeof(struct ps_deed)*na);
    if (!nv) return -1;
    stats->deedv=nv;
    stats->deeda=na;
  }
  struct ps_deed *deed=stats->deedv+p;
  memmove(deed+1,deed,sizeof(struct ps_deed)*(stats->deedc-p));
  stats->deedc++;
  deed->x=x;
  deed->y=y;
  return 0;
}
 
int ps_stats_check_deed(const struct ps_stats *stats,int x,int y) {
  if (!stats) return 0;
  int p=ps_stats_search_deed(stats,x,y);
  return (p>=0)?1:0;
}

int ps_stats_set_deed(struct ps_stats *stats,int x,int y) {
  if (!stats) return -1;
  int p=ps_stats_search_deed(stats,x,y);
  if (p>=0) return 0;
  p=-p-1;
  if (ps_stats_insert_deed(stats,p,x,y)<0) return -1;
  return 0;
}

int ps_stats_remove_deed(struct ps_stats *stats,int x,int y) {
  if (!stats) return -1;
  int p=ps_stats_search_deed(stats,x,y);
  if (p<0) return 0;
  stats->deedc--;
  memmove(stats->deedv+p,stats->deedv+p+1,sizeof(struct ps_deed)*(stats->deedc-p));
  return 0;
}
