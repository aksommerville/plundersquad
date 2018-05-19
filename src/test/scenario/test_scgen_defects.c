#include "test/ps_test.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "scenario/ps_scgen.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_region.h"
#include "akpng/akpng.h"
#include "akgl/akgl.h"
#include "os/ps_fs.h"
#include "os/ps_clockassist.h"
#include <time.h>

/* List of visited locations.
 */

#define MAX_SCREENC 108 /* per ps_scgen_calculate_world_size */

struct screen_list {
  struct coordinate {
    int x,y;
  } v[MAX_SCREENC];
  int c;
};

static int screen_list_test(const struct screen_list *list,int x,int y) {
  const struct coordinate *coord=list->v;
  int i=list->c; for (;i-->0;coord++) {
    if ((coord->x==x)&&(coord->y==y)) return 1;
  }
  return 0;
}

static int screen_list_add(struct screen_list *list,int x,int y) {
  if (screen_list_test(list,x,y)) return 0;
  if (list->c>=MAX_SCREENC) return -1;
  list->v[list->c].x=x;
  list->v[list->c].y=y;
  list->c++;
  return 1;
}

/* Detect loops in scenario screens.
 * Walk home from each screen, following the breadcrumbs placed by generator.
 * If we cross our own path, that's a loop.
 */

static int detect_scenario_loop_from_screen(const struct ps_scenario *scenario,int x,int y) {
  struct screen_list visited={0};
  const struct ps_screen *screen=scenario->screenv+(y*scenario->w)+x;
  while ((x!=scenario->homex)||(y!=scenario->homey)) {
    if (screen_list_add(&visited,x,y)<1) return 1;
    switch (screen->direction_home) {
      case PS_DIRECTION_NORTH: y--; screen-=scenario->w; break;
      case PS_DIRECTION_SOUTH: y++; screen+=scenario->w; break;
      case PS_DIRECTION_WEST: x--; screen--; break;
      case PS_DIRECTION_EAST: x++; screen++; break;
    }
  }
  return 0;
}

static int detect_scenario_loop(const struct ps_scenario *scenario) {
  int y=0; for (;y<scenario->h;y++) {
    int x=0; for (;x<scenario->w;x++) {
      if (detect_scenario_loop_from_screen(scenario,x,y)) {
        PS_LOG("Detected loop from screen (%d,%d)",x,y);
        return 1;
      }
    }
  }
  return 0;
}

/* commit c8baca881a0a3616ca6e94565cc8a05c496cfe1f
 * playerc=1 difficulty=9 length=2
 * Unfortunately, this was on the second play of the run, so we can't replay it by forcing random seed.
 * I observed that blueprint:90 (teleport-bullets) was pointed wrong.
 * Entered it directly from home screen and arrived on the solution side.
 * Did not confirm, but I suspect that there was a loop causing the screen to point the wrong direction home.
 * This test will force that situation.
 *
 * ...UPDATE
 * The test didn't work but now I know why -- blueprint:90 had no solutions listed.
 * That is an error; if we want a blueprint to be directional it must declare itself as a challenge (by declaring a solution).
 */

PS_TEST(test_defect_challenge_direction,scgen,ignore) {

  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=PS_LOG_LEVEL_WARN;
  ps_log_level_by_domain[PS_LOG_DOMAIN_GENERATOR]=PS_LOG_LEVEL_ERROR;

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  int repc=10000;
  while (repc-->0) {
    PS_LOG("%d attempts remaining",repc);

    /* Set random seed. */
    int seed=ps_time_now();
    //seed=1526682486;
    //PS_LOG("Random seed %d",seed);
    srand(seed);

    struct ps_scgen *scgen=ps_scgen_new();
    PS_ASSERT(scgen)

    /* Set input parameters for generator. */
    scgen->playerc=1;
    scgen->skills=0;
    scgen->difficulty=9;
    scgen->length=9;

    /* Generate. */
    PS_ASSERT_CALL(ps_scgen_generate(scgen))

    /* Look for loops. */
    PS_ASSERT_NOT(detect_scenario_loop(scgen->scenario),"seed=%d",seed)
    // Run this test until it fails, and record that random seed.
    //...not catching anything, after thousands of runs at different sizes. Hrmph.

    ps_scgen_del(scgen);
  }
  
  ps_resmgr_quit();
  return 0;
}
