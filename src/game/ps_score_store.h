/* ps_score_store.h
 * Permanent record of completed games.
 */

#ifndef PS_SCORE_STORE_H
#define PS_SCORE_STORE_H

struct ps_score_store;
struct ps_game;

struct ps_score_store *ps_score_store_new(const char *path,int pathc);
void ps_score_store_del(struct ps_score_store *store);
int ps_score_store_ref(struct ps_score_store *store);

int ps_score_store_add_record(
  struct ps_score_store *store,
  const struct ps_game *game
);

/* Return a list of (plrdefidA,countA,...,plrdefidZ,countZ) for all plrdef
 * resources that have ever been involved in a victorious game.
 * Assembly page will use this to gently suggest the user to choose an underused hero.
 * Returns count of entries (the list is twice that long).
 * Caller must free (*dstpp) on successful return.
 */
int ps_score_store_count_plrdefid_usages(int **dstpp,const struct ps_score_store *store);

/* For testing only, add a record to the store.
 * Terminate plrdefid list with <0.
 */
int ps_score_store_add_false_record(struct ps_score_store *store,int playtime,int length,int difficulty,int plrdefid,...);

/* Given the party in (game), what difficulty level do we suggest?
 * Generally, this should be one greater than the highest difficulty beaten by an identical party.
 * But there may be other black magic in the logic; leave that to us.
 */
int ps_score_store_recommend_difficulty(const struct ps_score_store *store,const struct ps_game *game);

/* Return the length of the most recent game, or -1 if anything goes wrong (eg first play).
 */
int ps_score_store_get_most_recent_length(const struct ps_score_store *store);

/* Game rating.
 * Ask for a rating immediately after adding a record.
 * The most recent record is our comparison reference.
 *****************************************************************************/

/* Levels of matching.
 * Higher CRITERION values are more specific matches.
 * Generally you want the highest criterion with sensible data.
 */
#define PS_SCORE_CRITERION_ALL                           0
#define PS_SCORE_CRITERION_PLAYERC                       1
#define PS_SCORE_CRITERION_PLAYERC_LENGTH                2
#define PS_SCORE_CRITERION_PARTY                         3
#define PS_SCORE_CRITERION_PARTY_LENGTH                  4
#define PS_SCORE_CRITERION_PLAYERC_LENGTH_DIFFICULTY     5
#define PS_SCORE_CRITERION_PARTY_LENGTH_DIFFICULTY       6
#define PS_SCORE_CRITERION_COUNT                         7

struct ps_score_rating {
  int criterion; // PS_SCORE_CRITERION_*
  int rank; // 1..count
  int count; // >=1
  int tiec; // If >0, others match this rank precisely.
  int best; // Best score on record, good to know if we're not #1.
};

struct ps_score_criterion {
  int count_better;
  int count_same;
  int count_worse;
  int best;
};

struct ps_score_comparison {
  struct ps_score_rating relevant;
  struct ps_score_criterion rankv[PS_SCORE_CRITERION_COUNT];
};

/* Compare most recent game to all history.
 * (comparison->relevant) will have a subjective assessment of the best data to report to the user.
 * (comparison->rankv) will have raw data.
 */
int ps_score_store_rate_most_recent(struct ps_score_comparison *comparison,const struct ps_score_store *store);

#endif
