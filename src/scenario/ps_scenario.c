#include "ps.h"
#include "ps_scenario.h"
#include "ps_world.h"

/* New.
 */

struct ps_scenario *ps_scenario_new() {
  struct ps_scenario *scenario=calloc(1,sizeof(struct ps_scenario));
  if (!scenario) return 0;

  scenario->refc=1;

  return scenario;
}

/* Delete.
 */

void ps_scenario_del(struct ps_scenario *scenario) {
  if (!scenario) return;
  if (scenario->refc-->1) return;

  ps_world_del(scenario->world);

  free(scenario);
}

/* Retain.
 */

int ps_scenario_ref(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  if (scenario->refc<1) return -1;
  if (scenario->refc==INT_MAX) return -1;
  scenario->refc++;
  return 0;
}

/* Clear.
 */

int ps_scenario_clear(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  return 0;
}

/* Encode.
 */

int ps_scenario_encode(void *dstpp,const struct ps_scenario *scenario) {
  return -1;
}

/* Decode.
 */

int ps_scenario_decode(struct ps_scenario *scenario,const void *src,int srcc) {
  return -1;
}
