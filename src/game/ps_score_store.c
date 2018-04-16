#include "ps.h"
#include "ps_score_store.h"
#include "os/ps_clockassist.h"

/* Object definition.
 */

struct ps_score_store {
  int refc;
};

/* Object lifecycle.
 */

struct ps_score_store *ps_score_store_new() {
  struct ps_score_store *store=calloc(1,sizeof(struct ps_score_store));
  if (!store) return 0;

  store->refc=1;

  return store;
}

void ps_score_store_del(struct ps_score_store *store) {
  if (!store) return;
  if (store->refc-->1) return;

  free(store);
}

int ps_score_store_ref(struct ps_score_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Add record.
 */
 
int ps_score_store_add_record(
  struct ps_score_store *store,
  int playerc,//TODO record team composition, not just count
  int difficulty,
  int length,
  int treasurec,
  int playtime,
  int monster_kills,
  int hero_kills,
  int hero_deaths
) {
  if (!store) return -1;

  int64_t timestamp=ps_time_now()/1000000;
  char line[256];
  int linec=snprintf(
    line,sizeof(line),
    "%lld %d %d %d %d %d %d %d %d\n",
    timestamp,playerc,difficulty,length,treasurec,playtime,monster_kills,hero_kills,hero_deaths
  );
  if ((linec<1)||(linec>=sizeof(line))) return -1;

  ps_log(GAME,DEBUG,"record for score store: %.*s",linec,line);

  return 0;
}
