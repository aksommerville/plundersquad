/* ps_score_store.h
 * Permanent record of completed games.
 */

#ifndef PS_SCORE_STORE_H
#define PS_SCORE_STORE_H

struct ps_score_store;

struct ps_score_store *ps_score_store_new();
void ps_score_store_del(struct ps_score_store *store);
int ps_score_store_ref(struct ps_score_store *store);

int ps_score_store_add_record(
  struct ps_score_store *store,
  int playerc,
  int difficulty,
  int length,
  int treasurec,
  int playtime,
  int monster_kills,
  int hero_kills,
  int hero_deaths
);

#endif
