#include "test/ps_test.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_scgen.h"
#include "res/ps_resmgr.h"
#include <sys/time.h>

/* Generate scenario.
 */

static struct ps_scenario *generate_scenario() {

  struct ps_scgen *scgen=ps_scgen_new();
  if (!scgen) return 0;

  /* Set input parameters for generator. */
  scgen->playerc=3;
  scgen->skills=PS_SKILL_HOOKSHOT|PS_SKILL_ARROW|PS_SKILL_SWORD|PS_SKILL_COMBAT;
  scgen->difficulty=5;
  scgen->length=5;

  /* Set random seed. */
  int seed=time(0);
  PS_LOG("Random seed %d",seed);
  srand(seed);

  /* Generate. */
  if (ps_scgen_generate(scgen)<0) {
    ps_log(TEST,ERROR,"Failed to generate scenario: %.*s",scgen->msgc,scgen->msg);
    return 0;
  } else {
    if (!scgen->scenario) return 0;
  }

  struct ps_scenario *scenario=scgen->scenario;
  scgen->scenario=0;
  return scenario;
}

/* Main entry point.
 */
 
PS_TEST(test_scenario_encode,scgen,functional,ignore) {

  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=PS_LOG_LEVEL_WARN;

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  struct ps_scenario *scenario=generate_scenario();
  PS_ASSERT(scenario,"Failed to generate scenario.")

  void *serial=0;
  int serialc=ps_scenario_encode(&serial,scenario);
  PS_ASSERT_CALL(serialc,"ps_scenario_encode()")
  PS_ASSERT(serial,"serialc=%d",serialc)

  struct ps_scenario *readback;

  free(serial);
  ps_scenario_del(scenario);
  ps_resmgr_quit();
  return 0;
}
