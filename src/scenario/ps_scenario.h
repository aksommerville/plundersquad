/* ps_scenario.h
 * Top-level scenario container.
 * A scenario is the volatile data generated for each play of the game.
 */

#ifndef PS_SCENARIO_H
#define PS_SCENARIO_H

struct ps_world;

struct ps_scenario {
  int refc;
  struct ps_world *world;
  int homex,homey;
  int treasurec;
};

struct ps_scenario *ps_scenario_new();
void ps_scenario_del(struct ps_scenario *scenario);
int ps_scenario_ref(struct ps_scenario *scenario);

int ps_scenario_clear(struct ps_scenario *scenario);

int ps_scenario_encode(void *dstpp,const struct ps_scenario *scenario);
int ps_scenario_decode(struct ps_scenario *scenario,const void *src,int srcc);

#endif
