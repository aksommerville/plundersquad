/* ps_world.h
 * Full world map.
 * This is a grid of screens.
 * XXX Move this into ps_scenario
 */

#ifndef PS_WORLD_H
#define PS_WORLD_H

#include "ps_screen.h"

struct ps_world {
  int w,h;
  struct ps_screen v[];
};

struct ps_world *ps_world_new(int w,int h);
void ps_world_del(struct ps_world *world);

// Valid arguments required.
#define PS_WORLD_SCREEN(world,x,y) ((world)->v+((y)*(world)->w+(x)))

// NULL if invalid.
struct ps_screen *ps_world_get_screen(struct ps_world *world,int x,int y);

#endif
