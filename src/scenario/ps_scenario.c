#include "ps.h"
#include "ps_scenario.h"
#include "ps_screen.h"

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

  if (scenario->screenv) {
    int screenc=scenario->w*scenario->h;
    while (screenc-->0) {
      ps_screen_cleanup(scenario->screenv+screenc);
    }
    free(scenario->screenv);
  }

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

/* Get screen.
 */
 
struct ps_screen *ps_scenario_get_screen(const struct ps_scenario *scenario,int x,int y) {
  if (!scenario) return 0;
  if ((x<0)||(x>=scenario->w)) return 0;
  if ((y<0)||(y>=scenario->h)) return 0;
  return PS_SCENARIO_SCREEN(scenario,x,y);
}

/* Clear.
 */

int ps_scenario_clear(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  if (ps_scenario_reallocate_screens(scenario,0,0)<0) return -1;
  scenario->homex=0;
  scenario->homey=0;
  scenario->treasurec=0;
  return 0;
}

/* Initialize screens.
 */

static void ps_scenario_seal_edges(struct ps_scenario *scenario) {
  int xz=scenario->w-1;
  int yz=scenario->h-1;
  int i=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;i-->0;screen++) {
    if (!screen->x) screen->doorw=PS_DOOR_CLOSED;
    if (!screen->y) screen->doorn=PS_DOOR_CLOSED;
    if (screen->x==xz) screen->doore=PS_DOOR_CLOSED;
    if (screen->y==yz) screen->doors=PS_DOOR_CLOSED;
  }
}

static int ps_scenario_initialize_screens(struct ps_scenario *scenario) {

  /* Set (x,y) in each screen. */
  uint8_t x=0,y=0;
  struct ps_screen *screen=scenario->screenv;;
  int screenc=scenario->w*scenario->h;
  while (screenc-->0) {
    screen->x=x;
    screen->y=y;
    if (++x>=scenario->w) {
      x=0;
      y++;
    }
    screen++;
  }

  ps_scenario_seal_edges(scenario);

  return 0;
}

/* Reallocate screens.
 */
 
int ps_scenario_reallocate_screens(struct ps_scenario *scenario,int w,int h) {
  if (!scenario) return -1;
  if ((w<0)||(h<0)||(w&&!h)||(!w&&h)) return -1;
  if (w>PS_WORLD_W_LIMIT) return -1;
  if (h>PS_WORLD_H_LIMIT) return -1;

  struct ps_screen *nv=0;
  if (w&&h) {
    nv=calloc(sizeof(struct ps_screen),w*h);
    if (!nv) return -1;
  }
  if (scenario->screenv) {
    int screenc=scenario->w*scenario->h;
    while (screenc-->0) ps_screen_cleanup(scenario->screenv+screenc);
    free(scenario->screenv);
  }
  scenario->screenv=nv;
  scenario->w=w;
  scenario->h=h;

  if (ps_scenario_initialize_screens(scenario)<0) {
    scenario->w=scenario->h=0;
    return -1;
  }
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
