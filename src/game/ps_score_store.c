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

/* Sort plrdefidv. This is very important, so we can easily identify identical parties in the future.
 */

static void ps_score_record_sort_plrdefidv(struct ps_score_record *record) {
  int lo=0,hi=record->playerc-1,d=1;
  while (lo<hi) {
    int first,last,i,done=1;
    if (d==1) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      int cmp=0;
      if (record->plrdefidv[i]<record->plrdefidv[i+d]) cmp=-1;
      else if (record->plrdefidv[i]>record->plrdefidv[i+d]) cmp=1;
      if (cmp==d) {
        uint8_t tmp=record->plrdefidv[i];
        record->plrdefidv[i]=record->plrdefidv[i+d];
        record->plrdefidv[i+d]=tmp;
        done=0;
      }
    }
    if (done) return;
    if (d==1) { d=-1; hi--; }
    else { d=1; lo++; }
  }
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
  ps_score_record_sort_plrdefidv(record);
  
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

/* Add false record.
 */
 
int ps_score_store_add_false_record(struct ps_score_store *store,int playtime,int length,int difficulty,int plrdefid,...) {
  if (!store) return -1;
  if (ps_score_store_require(store)<0) return -1;
  
  struct ps_score_record *record=store->recordv+store->recordc++;
  record->time=ps_time_now();
  record->length=length;
  record->difficulty=difficulty;
  record->playtime=playtime;
  
  va_list vargs;
  va_start(vargs,plrdefid);
  while (plrdefid>=0) {
    if (record->playerc>=PS_PLAYER_LIMIT) return -1;
    record->plrdefidv[record->playerc++]=plrdefid;
    plrdefid=va_arg(vargs,int);
  }

  if (ps_score_store_append_to_file(store,record,1)<0) return -1;
  return 0;
}

/* Count plrdefid.
 */

static int ps_score_store_search_plrdefid_usage(const int *dstv,int dstc,int plrdefid) {
  int lo=0,hi=dstc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int p=ck<<1;
         if (plrdefid<dstv[p]) hi=ck;
    else if (plrdefid>dstv[p]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}
 
int ps_score_store_count_plrdefid_usages(int **dstpp,const struct ps_score_store *store) {
  if (!dstpp||!store) return -1;
  *dstpp=0;
  if (store->recordc<1) return 0;
  int dsta=8,dstc=0;
  int *dstv=calloc(sizeof(int),dsta<<1);
  if (!dstv) return -1;

  const struct ps_score_record *record=store->recordv;
  int i=store->recordc; for (;i-->0;record++) {
    if (record->playerc>PS_PLAYER_LIMIT) {
      ps_log(,ERROR,"High scores file is corrupt! Found playerc=%d.",record->playerc);
      free(dstv);
      return -1;
    }
    int j=record->playerc; while (j-->0) {
      int plrdefid=record->plrdefidv[j];
      int p=ps_score_store_search_plrdefid_usage(dstv,dstc,plrdefid);
      if (p<0) {

        if (dstc>=dsta) {
          dsta+=8;
          void *nv=realloc(dstv,sizeof(int)*dsta*2);
          if (!nv) {
            free(dstv);
            return -1;
          }
          dstv=nv;
        }

        p=-p-1;
        memmove(dstv+(p<<1)+2,dstv+(p<<1),sizeof(int)*((dstc-p)<<1));
        dstc++;
        dstv[p<<1]=plrdefid;
        dstv[(p<<1)+1]=1;

      } else {
        dstv[(p<<1)+1]++;
      }
    }
  }

  *dstpp=dstv;
  return dstc;
}

/* Record comparison helpers.
 */

static int ps_score_record_parties_match(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (a->playerc!=b->playerc) return -1;
  int i=a->playerc; while (i-->0) {
    if (a->plrdefidv[i]!=b->plrdefidv[i]) return 0;
  }
  return 1;
}

/* Record comparators.
 */

typedef int (*ps_score_record_comparator)(const struct ps_score_record *a,const struct ps_score_record *b);

static int ps_score_record_comparator_ALL(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PLAYERC(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (a->playerc!=b->playerc) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PLAYERC_LENGTH(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (a->playerc!=b->playerc) return -2;
  if (a->length!=b->length) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PARTY(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (!ps_score_record_parties_match(a,b)) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PARTY_LENGTH(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (!ps_score_record_parties_match(a,b)) return -2;
  if (a->length!=b->length) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PLAYERC_LENGTH_DIFFICULTY(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (a->playerc!=b->playerc) return -2;
  if (a->length!=b->length) return -2;
  if (a->difficulty!=b->difficulty) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static int ps_score_record_comparator_PARTY_LENGTH_DIFFICULTY(const struct ps_score_record *a,const struct ps_score_record *b) {
  if (!ps_score_record_parties_match(a,b)) return -2;
  if (a->length!=b->length) return -2;
  if (a->difficulty!=b->difficulty) return -2;
  if (a->playtime<b->playtime) return -1;
  if (a->playtime>b->playtime) return 1;
  return 0;
}

static ps_score_record_comparator ps_score_record_comparatorv[PS_SCORE_CRITERION_COUNT]={
  ps_score_record_comparator_ALL,
  ps_score_record_comparator_PLAYERC,
  ps_score_record_comparator_PLAYERC_LENGTH,
  ps_score_record_comparator_PARTY,
  ps_score_record_comparator_PARTY_LENGTH,
  ps_score_record_comparator_PLAYERC_LENGTH_DIFFICULTY,
  ps_score_record_comparator_PARTY_LENGTH_DIFFICULTY,
};

/* Compare to history.
 */
 
int ps_score_store_rate_most_recent(struct ps_score_comparison *comparison,const struct ps_score_store *store) {
  if (!comparison||!store) return -1;
  if (store->recordc<1) return -1;
  memset(comparison,0,sizeof(struct ps_score_comparison));

  /* Gather the raw data. */
  const struct ps_score_record *a=store->recordv+store->recordc-1;
  const struct ps_score_record *b=store->recordv;
  int i=store->recordc-1; for (;i-->0;b++) {
    int criterion=0; for (;criterion<PS_SCORE_CRITERION_COUNT;criterion++) {
      ps_score_record_comparator cmp=ps_score_record_comparatorv[criterion];
      switch (cmp(a,b)) {
        case -1: comparison->rankv[criterion].count_worse++; break;
        case 0: comparison->rankv[criterion].count_same++; break;
        case 1: comparison->rankv[criterion].count_better++; break;
        // Any other result means it didn't match.
      }
    }
  }

  /* Criterion IDs are in order; take the highest one with a result. */
  int criterion=PS_SCORE_CRITERION_COUNT; while (criterion-->0) {
    int recordc=comparison->rankv[criterion].count_better+comparison->rankv[criterion].count_same+comparison->rankv[criterion].count_worse;
    if (recordc<1) continue;
    comparison->relevant.criterion=criterion;
    comparison->relevant.rank=1+comparison->rankv[criterion].count_better;
    comparison->relevant.count=1+recordc; // Add one because the reference record is part of this group too.
    comparison->relevant.tiec=comparison->rankv[criterion].count_same;
    return 0;
  }

  /* First time playing? Fill in default results. */
  comparison->relevant.criterion=PS_SCORE_CRITERION_ALL;
  comparison->relevant.rank=1;
  comparison->relevant.count=1;
  comparison->relevant.tiec=0;
  return 0;
}
