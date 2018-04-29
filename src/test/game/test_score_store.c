#include "test/ps_test.h"
#include "game/ps_score_store.h"

/* Observed a defect right off the bat.
 * Should have reported "#2 of 2" but was "#4 of 4", for exact configuration.
 * Reviewing the data, seems it reported PS_SCORE_CRITERION_PARTY_LENGTH_DIFFICULTY
 * when it meant PS_SCORE_CRITERION_PLAYERC_LENGTH_DIFFICULTY.
 * uh...
 * hmm. Now that I'm actually testing it, I think it worked as designed.
 */

PS_TEST(test_score_store_defect_wrong_criterion,score_store) {

  struct ps_score_store *store=ps_score_store_new(0,0);
  PS_ASSERT(store)

  /* Actual content of my highscores file at the time of the defect: */
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,22839,4,9,3,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2253,1,4,6,-1)) // match
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2642,1,1,4,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,  928,1,4,9,-1)) // match
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 3475,1,1,5,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 1499,1,1,1,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2881,1,1,7,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 3405,1,1,8,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 1988,1,1,2,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,10230,2,9,3,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,11059,2,9,2,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2012,1,9,6,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 6390,1,9,5,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,  879,1,4,8,-1)) // match
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 7194,1,4,4,-1)) // <-- this game

  struct ps_score_comparison comparison={0};
  PS_ASSERT_CALL(ps_score_store_rate_most_recent(&comparison,store))

  PS_ASSERT_INTS(comparison.relevant.criterion,PS_SCORE_CRITERION_PLAYERC_LENGTH_DIFFICULTY)
  PS_ASSERT_INTS(comparison.relevant.rank,4)
  PS_ASSERT_INTS(comparison.relevant.count,4)
  PS_ASSERT_INTS(comparison.relevant.tiec,0)

  ps_score_store_del(store);
  return 0;
}
