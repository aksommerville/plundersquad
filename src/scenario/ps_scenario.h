/* ps_scenario.h
 * Top-level scenario container.
 * A scenario is the volatile data generated for each play of the game.
 */

#ifndef PS_SCENARIO_H
#define PS_SCENARIO_H

struct ps_screen;

struct ps_scenario {
  int refc;
  int homex,homey;
  int treasurec;
  int w,h; // World size in screens.
  struct ps_screen *screenv;
};

struct ps_scenario *ps_scenario_new();
void ps_scenario_del(struct ps_scenario *scenario);
int ps_scenario_ref(struct ps_scenario *scenario);

/* Two ways to get a screen: safe and unsafe.
 */
#define PS_SCENARIO_SCREEN(scenario,x,y) ((scenario)->screenv+((y)*(scenario)->w+(x)))
struct ps_screen *ps_scenario_get_screen(const struct ps_scenario *scenario,int x,int y);

int ps_scenario_clear(struct ps_scenario *scenario);

int ps_scenario_reallocate_screens(struct ps_scenario *scenario,int w,int h);

int ps_scenario_encode(void *dstpp,const struct ps_scenario *scenario);
int ps_scenario_decode(struct ps_scenario *scenario,const void *src,int srcc);

#endif
