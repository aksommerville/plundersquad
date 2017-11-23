#include "test/ps_test.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_scgen.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
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

/* Assert that two scenarios are identical -- The one was serialized then deserialized into the other.
 * (a) is the "expected" and (b) is the "actual".
 */

static int assert_grid_cells_identical(const struct ps_grid_cell *a,const struct ps_grid_cell *b) {
  PS_ASSERT_INTS(a->tsid,b->tsid) // XXX unused?
  PS_ASSERT_INTS(a->tileid,b->tileid)
  PS_ASSERT_INTS(a->physics,b->physics)
  PS_ASSERT_INTS(a->shape,b->shape)
  return 0;
}

static int assert_pois_identical(const struct ps_blueprint_poi *a,const struct ps_blueprint_poi *b) {
  PS_ASSERT_INTS(a->x,b->x)
  PS_ASSERT_INTS(a->y,b->y)
  PS_ASSERT_INTS(a->type,b->type)
  PS_ASSERT_INTS(a->argv[0],b->argv[0])
  PS_ASSERT_INTS(a->argv[1],b->argv[1])
  PS_ASSERT_INTS(a->argv[2],b->argv[2])
  return 0;
}

static int assert_grids_identical(const struct ps_grid *a,const struct ps_grid *b) {

  /* It's tempting to just memcmp() poiv, but there could be uninitialized space within it.
   * Also if there is a failure, I want details.
   */
  PS_ASSERT_INTS(a->poic,b->poic)
  const struct ps_blueprint_poi *apoi=a->poiv;
  const struct ps_blueprint_poi *bpoi=b->poiv;
  int i=a->poic; for (;i-->0;apoi++,bpoi++) {
    if (assert_pois_identical(apoi,bpoi)<0) {
      ps_log(TEST,ERROR,"Mismatch in grid POI.");
      return -1;
    }
  }

  PS_ASSERT(a->region==b->region,"a->region=%p, b->region=%p",a->region,b->region)
  PS_ASSERT_INTS(a->monsterc_min,b->monsterc_min)
  PS_ASSERT_INTS(a->monsterc_max,b->monsterc_max)

  const struct ps_grid_cell *acell=a->cellv;
  const struct ps_grid_cell *bcell=b->cellv;
  int y=0; for (;y<PS_GRID_ROWC;y++) {
    int x=0; for (;x<PS_GRID_COLC;x++,acell++,bcell++) {
      if (assert_grid_cells_identical(acell,bcell)<0) {
        ps_log(TEST,ERROR,"Mismatch at cell (%d,%d).",x,y);
        return -1;
      }
    }
  }

  return 0;
}

static int assert_screens_identical(const struct ps_screen *a,const struct ps_screen *b) {

  PS_ASSERT_INTS(a->x,b->x)
  PS_ASSERT_INTS(a->y,b->y)

  //TODO These fields may be transient, maybe we don't want to encode them.
  PS_ASSERT_INTS(a->xform,b->xform)
  PS_ASSERT_INTS(a->features,b->features)
  PS_ASSERT_INTS(a->doorn,b->doorn)
  PS_ASSERT_INTS(a->doors,b->doors)
  PS_ASSERT_INTS(a->doore,b->doore)
  PS_ASSERT_INTS(a->doorw,b->doorw)
  PS_ASSERT_INTS(a->direction_home,b->direction_home)

  /* Blueprints and regions are pulled from the same resource manager so they must end up the same object. */
  PS_ASSERT(a->blueprint==b->blueprint,"a->blueprint=%p, b->blueprint=%p",a->blueprint,b->blueprint)
  PS_ASSERT(a->region==b->region,"a->region=%p, b->region=%p",a->region,b->region)

  PS_ASSERT(a->grid)
  PS_ASSERT(b->grid)
  if (assert_grids_identical(a->grid,b->grid)<0) {
    ps_log(TEST,ERROR,"Mismatch in grids.");
    return -1;
  }

  return 0;
}

static int assert_scenarios_identical(const struct ps_scenario *a,const struct ps_scenario *b) {

  PS_ASSERT_INTS(a->homex,b->homex)
  PS_ASSERT_INTS(a->homey,b->homey)
  PS_ASSERT_INTS(a->treasurec,b->treasurec)
  PS_ASSERT_INTS(a->w,b->w)
  PS_ASSERT_INTS(a->h,b->h)

  int screenc=a->w*a->h;
  const struct ps_screen *ascreen=a->screenv;
  const struct ps_screen *bscreen=b->screenv;
  for (;screenc-->0;ascreen++,bscreen++) {
    if (assert_screens_identical(ascreen,bscreen)<0) {
      ps_log(TEST,ERROR,"Failed at screen (%d,%d) of (%d,%d)",ascreen->x,ascreen->y,a->w,a->h);
      return -1;
    }
  }

  return 0;
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

  ps_log(TEST,INFO,"Encoded scenario to %d bytes.",serialc);

  struct ps_scenario *readback=ps_scenario_new();
  PS_ASSERT(scenario,"ps_scenario_new()")
  PS_ASSERT_CALL(ps_scenario_decode(readback,serial,serialc))

  if (assert_scenarios_identical(scenario,readback)<0) {
    //TODO dump serialized scenario?
    return -1;
  }

  free(serial);
  ps_scenario_del(scenario);
  ps_scenario_del(readback);
  ps_resmgr_quit();
  return 0;
}
