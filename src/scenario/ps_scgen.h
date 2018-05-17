/* ps_scgen.h
 * Scenario generator.
 * This object coordinates construction of the scenario, at a high level.
 *
 * 1. Create scgen object.
 * 2. Populate inputs directly:
 *     playerc
 *     skills
 *     difficulty
 *     length
 * 3. Call ps_scgen_generate()
 * 4. Pull result from (scenario) or error message from (msg,msgc).
 *
 */

#ifndef PS_SCGEN_H
#define PS_SCGEN_H

struct ps_scenario;
struct ps_blueprint_list;
struct ps_zones;

struct ps_scgen {

  // Inputs:
  int playerc;
  uint16_t skills;
  int difficulty;
  int length;

  // Inputs for test generator only:
  struct ps_blueprint_list *blueprints_require;
  int regionid;

  // Outputs:
  struct ps_scenario *scenario;
  char *msg; int msgc;

  // Intermediate:
  int treasurec;
  int homex,homey;
  struct ps_screen **screenbuf; // WEAK
  int screenbufc,screenbufa;
  struct ps_blueprint_list *blueprints;
  struct ps_blueprint_list *blueprints_scratch;
  struct ps_zones *zones;
};

struct ps_scgen *ps_scgen_new();
void ps_scgen_del(struct ps_scgen *scgen);

/* The main event.
 * Before calling, you must set the generator's inputs. (Set them directly)
 * We return >=0 and populate (scenario) on success.
 * We return <0 and usually populate (msg) on error.
 * Beware that on error, an incomplete scenario may exist. Observe the return value.
 */
int ps_scgen_generate(struct ps_scgen *scgen);

/* Generate a map with explicit blueprints and region, for test purposes.
 */
int ps_scgen_test_require_blueprint(struct ps_scgen *scgen,int blueprintid);
int ps_scgen_test_require_region(struct ps_scgen *scgen,int regionid);
int ps_scgen_test_generate(struct ps_scgen *scgen);
int ps_scgen_test_generate_all_blueprints(struct ps_scgen *scgen);

/* Set error message and return -1.
 */
int ps_scgen_failv(struct ps_scgen *scgen,const char *fmt,va_list vargs);
int ps_scgen_fail(struct ps_scgen *scgen,const char *fmt,...);

/* Internal use.
 */
int ps_scgen_randint(struct ps_scgen *scgen,int limit);

#endif
