#include "test/ps_test.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "scenario/ps_blueprint.h"
#include "game/ps_sprite.h"
#include "game/ps_plrdef.h"

/* General resource assertions.
 */

static int at_least_one_resource(int restype) {
  struct ps_restype *type=ps_resmgr_get_type_by_id(restype);
  if (!type) return 0;
  if (type->resc<1) return 0;
  return 1;
}

static int resource_exists(int restype,int resid) {
  void *res=ps_res_get(restype,resid);
  if (!res) return 0;
  return 1;
}

/* Blueprint assertions.
 */

static int at_least_one_home_blueprint_for_eight_players() {
  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  if (!restype) return 0;
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    const struct ps_blueprint *blueprint=res->obj;
    int heroc=ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_HERO);
    if (heroc>=8) return 1;
  }
  return 0;
}

static int at_least_one_treasure_blueprint() {
  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  if (!restype) return 0;
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    const struct ps_blueprint *blueprint=res->obj;
    int treasurec=ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_TREASURE);
    if (treasurec>0) return 1;
  }
  return 0;
}

/* Player definition assertions.
 */

static int plrdef_for_skill(uint16_t skill) {
  const struct ps_restype *restype=PS_RESTYPE(PLRDEF);
  if (!restype) return 0;
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    const struct ps_plrdef *plrdef=res->obj;
    if (plrdef->skills&skill) return 1;
  }
  return 0;
}

/* Sprite definition assertions.
 */

static int sprdef_for_type(const struct ps_sprtype *sprtype) {
  const struct ps_restype *restype=PS_RESTYPE(SPRDEF);
  if (!restype) return 0;
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    const struct ps_sprdef *sprdef=res->obj;
    if (sprdef->type==sprtype) return 1;
  }
  return 0;
}

/* Count blueprint solutions by player count.
 */

static void count_blueprint_solutions_by_player_count_1(int *blueprintc_by_playerc,const struct ps_blueprint *blueprint) {

  /* No-challenge blueprints go in slot zero. */
  if (blueprint->solutionc<1) {
    blueprintc_by_playerc[0]++;
    return;
  }

  /* Increment each player count that has a solution to this blueprint.
   * We don't care about difficulty or skill sets.
   */
  int i=1; for (;i<=PS_PLAYER_LIMIT;i++) {
    if (ps_blueprint_is_solvable(0,blueprint,i,0xffff)) {
      blueprintc_by_playerc[i]++;
    }
  }
  
}

static int count_blueprint_solutions_by_player_count() {

  int blueprintc_by_playerc[1+PS_PLAYER_LIMIT]={0};

  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  PS_ASSERT(restype)
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    const struct ps_blueprint *blueprint=res->obj;
    count_blueprint_solutions_by_player_count_1(blueprintc_by_playerc,blueprint);
  }

  for (i=1;i<=PS_PLAYER_LIMIT;i++) {
    PS_ASSERT(blueprintc_by_playerc[i],"No challenges for player count %d. We can't generate an interesting scenario for %d players.",i,i)
  }

  ps_log(RES,INFO,"Unchallenged blueprints: %4d",blueprintc_by_playerc[0]);
  for (i=1;i<=PS_PLAYER_LIMIT;i++) {
    ps_log(RES,INFO,"       Challenges for %d: %4d",i,blueprintc_by_playerc[i]);
  }
  
  return 0;
}

/* Count blueprint solutions by skill.
 */

static int blueprint_has_other_solution_for_playerc(const struct ps_blueprint *blueprint,const struct ps_blueprint_solution *known_solution,int playerc) {
  int i=blueprint->solutionc;
  const struct ps_blueprint_solution *solution=blueprint->solutionv;
  for (;i-->0;solution++) {
    if (solution==known_solution) continue;
    if (playerc<solution->plo) continue;
    if (playerc>solution->phi) continue;
    return 1;
  }
  return 0;
}

static int count_blueprint_solutions_by_skill() {

  const struct ps_res *res;
  const struct ps_blueprint *blueprint;
  const struct ps_blueprint_solution *solution;
  int i,ii;
  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  PS_ASSERT(restype);

  #define ITERATE \
    for (res=restype->resv,i=restype->resc,blueprint=res->obj;i-->0;res++,blueprint=res->obj) \
      for (solution=blueprint->solutionv,ii=blueprint->solutionc;ii-->0;solution++)

  #define REQUIRE(var) \
    PS_ASSERT(var,"Blueprint solution condition not met: %s",#var)

  /* There must be at least one blueprint for some player count where only this skill can solve it. */
  #define MUST_HAVE_LONE_SOLUTION(tag) { \
    int have_lone_solution_for_##tag=0; \
    ITERATE { \
      if (solution->skills==PS_SKILL_##tag) { \
        int playerc=solution->plo; \
        for (;playerc<=solution->phi;playerc++) { \
          if (!blueprint_has_other_solution_for_playerc(blueprint,solution,playerc)) { \
            have_lone_solution_for_##tag=1; \
            goto _lone_solution_##tag##_ready_; \
          } \
        } \
      } \
    } \
   _lone_solution_##tag##_ready_: \
    REQUIRE(have_lone_solution_for_##tag) \
  }

  /* This skill is never directly required to solve a challenge alone., and therefore must not be listed in a solution. */
  #define MUST_NOT_HAVE_SOLUTION(tag) { \
    ITERATE { \
      PS_ASSERT( \
        !(solution->skills&PS_SKILL_##tag), \
        "Skill %s should never be part of a solution. blueprint:%d", \
        #tag,ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,blueprint) \
      ) \
    } \
  }

  /* This skill can never solve a challenge alone, but may be part of multi-skill solutions. 
   * Typically, this means the skill in question makes some other solution easier.
   */
  #define MUST_NOT_HAVE_LONE_SOLUTION(tag) { \
    ITERATE { \
      PS_ASSERT( \
        solution->skills!=PS_SKILL_##tag, \
        "Skill %s is not expected to solve anything by itself. blueprint:%d", \
        #tag,ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,blueprint) \
      ) \
    } \
  }

  /* This skill must be required by a solution somewhere, but there could be other skills involved too. */
  #define MUST_HAVE_SOLUTION(tag) { \
    int have_solution_for_##tag=0; \
    ITERATE { \
      if (solution->skills&PS_SKILL_##tag) { \
        have_solution_for_##tag=1; \
        goto _solution_##tag##_ready_; \
      } \
    } \
   _solution_##tag##_ready_: \
    REQUIRE(have_solution_for_##tag) \
  }

  /* Now declare how each skill is required to be used: */
  MUST_HAVE_LONE_SOLUTION(SWORD)
  MUST_HAVE_LONE_SOLUTION(ARROW)
  MUST_HAVE_LONE_SOLUTION(HOOKSHOT)
  MUST_NOT_HAVE_SOLUTION(FLAME)
  MUST_HAVE_LONE_SOLUTION(HEAL)
  MUST_HAVE_LONE_SOLUTION(IMMORTAL)
  MUST_HAVE_LONE_SOLUTION(FLY)
  MUST_NOT_HAVE_SOLUTION(MARTYR)
  MUST_NOT_HAVE_SOLUTION(SPEED)
  MUST_NOT_HAVE_SOLUTION(WEIGHT)
  MUST_NOT_HAVE_SOLUTION(FROG)
  MUST_HAVE_SOLUTION(COMBAT)

  #undef ITERATE
  #undef REQUIRE
  #undef MUST_HAVE_LONE_SOLUTION
  #undef MUST_NOT_HAVE_SOLUTION
  #undef MUST_NOT_HAVE_LONE_SOLUTION
  #undef MUST_HAVE_SOLUTION

  return 0;
}

/* Live resources test.
 */

PS_TEST(test_res_content,res,functional,ignore) {

  int pvloglevel=ps_log_level_by_domain[PS_LOG_DOMAIN_RES];
  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=PS_LOG_LEVEL_TRACE;

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  /* blueprint */
  PS_ASSERT(at_least_one_home_blueprint_for_eight_players())
  PS_ASSERT(at_least_one_treasure_blueprint())
  if (count_blueprint_solutions_by_player_count()<0) return -1;
  if (count_blueprint_solutions_by_skill()<0) return -1;
  //TODO analyze solutions for skills and difficulty -- what does that mean?

  /* plrdef */
  PS_ASSERT(plrdef_for_skill(PS_SKILL_SWORD))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_ARROW))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_HOOKSHOT))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_FLAME))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_HEAL))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_FLY))
  PS_ASSERT(plrdef_for_skill(PS_SKILL_MARTYR))

  /* region */
  PS_ASSERT(at_least_one_resource(PS_RESTYPE_REGION))

  /* song */
  /* soundeffect */

  /* sprdef */
  //TODO There are specific sprdef IDs called out in code that we should validate here.
  PS_ASSERT(sprdef_for_type(&ps_sprtype_arrow))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_healmissile))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_explosion))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_switch))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bug))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_seamonster))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_missile))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_treasurechest))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_dragonbug))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bumblebat))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_blueberry))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_rabbit))

  /* tilesheet */
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,1),"UI chrome")
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,3),"hero")
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,4),"monsters")

  ps_resmgr_quit();
  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=pvloglevel;
  return 0;
}
