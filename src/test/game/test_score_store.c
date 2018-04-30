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

/* Another defect, this one might be real.
 * "Length 1 with this configuration", 18 results. Should have been none.
 * ...This was indeed a defect. ps_score_record_parties_match was returning -1 when it meant 0.
 */

PS_TEST(test_score_store_defect_2_bad_count,score_store) {

  struct ps_score_store *store=ps_score_store_new(0,0);
  PS_ASSERT(store)

  /* Actual content of my highscores file at the time of the defect: */
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,22839,4,9,3,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2253,1,4,6,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2642,1,1,4,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,  928,1,4,9,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 3475,1,1,5,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 1499,1,1,1,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2881,1,1,7,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 3405,1,1,8,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 1988,1,1,2,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,10230,2,9,3,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,11059,2,9,2,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2012,1,9,6,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 6390,1,9,5,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,  879,1,4,8,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 7194,1,4,4,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 1796,1,4,7,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,12643,1,4,9,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 2024,1,4,1,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,   22,1,4,1,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 5425,2,4,5,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 9745,2,4,9,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,12034,2,4,7,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,14295,1,4,2,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 6468,2,4,6,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store, 3575,2,4,6,-1))
  PS_ASSERT_CALL(ps_score_store_add_false_record(store,  205,1,8,2,3,4,5,-1)) // <-- this game

  struct ps_score_comparison comparison={0};
  PS_ASSERT_CALL(ps_score_store_rate_most_recent(&comparison,store))

  PS_ASSERT_INTS(comparison.relevant.criterion,PS_SCORE_CRITERION_ALL)
  PS_ASSERT_INTS(comparison.relevant.rank,2)
  PS_ASSERT_INTS(comparison.relevant.count,26)
  PS_ASSERT_INTS(comparison.relevant.tiec,0)

  ps_score_store_del(store);
  return 0;
}
