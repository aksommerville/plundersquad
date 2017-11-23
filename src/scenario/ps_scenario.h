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

/* ===== Serial format =====
 * TODO There should probably be some version control in here, for both code and data.
 * TODO We probably also need to encode the generator criteria.
 * Encoded scenario is one Scenario Header followed by the appropriate count of Screens.
 * Screens are ordered LRTB (the same order we use live).
 * --- Scenario Header
 *   0000   1 world width in screens
 *   0001   1 world height in screens
 *   0002   1 treasure count
 *   0003   1 home screen x
 *   0004   1 home screen y
 *   0005   1 unused
 *   0006   2 grid size, for validation
 *   0008   2 extra header length
 *   000a ... extra header
 * --- Screen
 *   0000   2 blueprint id
 *   0002   2 region id
 *   0004   1 xform
 *   0005   1 features
 *   0006   1 direction_home
 *   0007   1 unused
 *   0008   4 doors: n,s,e,w
 *   000c   2 monsterc: min,max
 *   000e   2 poic
 *   0010 ... poiv:
 *     0000   1 x
 *     0001   1 y
 *     0002   1 type
 *     0003   1 unused
 *     0004  12 argv[3]
 *     0010
 *  .... ... cellv:
 *     0000   1 tsid
 *     0001   1 tileid
 *     0002   1 physics
 *     0003   1 shape
 *     0004
 * Both functions return the encoded length.
 * encode populates (*dstpp) with a new buffer which you must free.
 */
int ps_scenario_encode(void *dstpp,const struct ps_scenario *scenario);
int ps_scenario_decode(struct ps_scenario *scenario,const void *src,int srcc);

#endif
