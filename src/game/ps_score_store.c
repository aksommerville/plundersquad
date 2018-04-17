/* The score store is not portable.
 * We read and write struct ps_score_record directly to the file.
 */

#include "ps.h"
#include "ps_score_store.h"
#include "game/ps_game.h"
#include "game/ps_stats.h"
#include "game/ps_player.h"
#include "res/ps_resmgr.h"
#include "os/ps_clockassist.h"
#include "os/ps_fs.h"

/* Object definition.
 */

struct ps_score_record {
  int64_t time;
  uint8_t playerc;
  uint8_t plrdefidv[PS_PLAYER_LIMIT];
  uint8_t difficulty;
  uint8_t length;
  uint8_t treasurec;
  uint32_t playtime;
  uint32_t killc_monster;
  uint32_t killc_hero;
  uint32_t deathc;
};

struct ps_score_store {
  int refc;
  struct ps_score_record *recordv;
  int recordc,recorda;
  char *path;
};

/* Object lifecycle.
 */

struct ps_score_store *ps_score_store_new(const char *path,int pathc) {
  struct ps_score_store *store=calloc(1,sizeof(struct ps_score_store));
  if (!store) return 0;

  store->refc=1;

  if (path) {
    if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
    if (pathc) {
    
      if (!(store->path=malloc(pathc+1))) {
        ps_score_store_del(store);
        return 0;
      }
      memcpy(store->path,path,pathc);
      store->path[pathc]=0;

      if ((store->recordc=ps_file_read(&store->recordv,store->path))<0) {
        ps_log(GAME,WARN,"%s: Failed to read high scores.",store->path);
        store->recordc=0;
        store->recordv=0;
      } else {
        store->recordc/=sizeof(struct ps_score_record);
        store->recorda=store->recordc;
      }
      
    }
  }

  return store;
}

void ps_score_store_del(struct ps_score_store *store) {
  if (!store) return;
  if (store->refc-->1) return;

  if (store->path) free(store->path);
  if (store->recordv) free(store->recordv);

  free(store);
}

int ps_score_store_ref(struct ps_score_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Grow record list.
 */

static int ps_score_store_require(struct ps_score_store *store) {
  if (store->recordc<store->recorda) return 0;
  int na=store->recorda+8;
  if (na>INT_MAX/sizeof(struct ps_score_record)) return -1;
  void *nv=realloc(store->recordv,sizeof(struct ps_score_record)*na);
  if (!nv) return -1;
  store->recordv=nv;
  store->recorda=na;
  return 0;
}

/* Append records to file.
 */

static int ps_score_store_append_to_file(struct ps_score_store *store,const struct ps_score_record *srcv,int srcc) {
  if (!store) return -1;
  if (!store->path) return 0;
  if (ps_file_append(store->path,srcv,sizeof(struct ps_score_record)*srcc)<0) {
    ps_log(GAME,ERROR,"%s: Failed to write high scores.",store->path);
  } else {
    ps_log(GAME,INFO,"%s: Wrote high scores.",store->path);
  }
  return 0;
}

/* Compose new record from game.
 */

static int ps_score_record_from_game(struct ps_score_record *record,const struct ps_game *game) {
  int i;
  if (!record||!game) return -1;
  memset(record,0,sizeof(struct ps_score_record));

  record->time=ps_time_now();

  record->playerc=game->playerc;
  record->difficulty=game->difficulty;
  record->length=game->length;
  record->treasurec=game->treasurec;

  for (i=game->playerc;i-->0;) {
    struct ps_player *player=game->playerv[i];
    if (!player) continue;
    int plrdefid=ps_res_get_id_by_obj(PS_RESTYPE_PLRDEF,player->plrdef);
    if ((plrdefid>0)&&(plrdefid<256)) record->plrdefidv[i]=plrdefid;
  }
  
  if (game->stats) {
    record->playtime=game->stats->playtime;
    for (i=game->playerc;i-->0;) {
      record->killc_monster+=game->stats->playerv[i].killc_monster;
      record->killc_hero+=game->stats->playerv[i].killc_hero;
      record->deathc+=game->stats->playerv[i].deathc;
    }
    if (record->deathc>=record->killc_hero) {
      record->deathc-=record->killc_hero;
    }
  }
  
  return 0;
}

/* Add record.
 */
 
int ps_score_store_add_record(
  struct ps_score_store *store,
  const struct ps_game *game
) {
  if (!store||!game) return -1;

  if (ps_score_store_require(store)<0) return -1;
  if (ps_score_record_from_game(store->recordv+store->recordc,game)<0) return -1;
  if (ps_score_store_append_to_file(store,store->recordv+store->recordc,1)<0) return -1;
  store->recordc++;

  return 0;
}
