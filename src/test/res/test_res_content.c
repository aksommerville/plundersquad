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
      if (solution->skills==0xffff) continue; /* intentionally unusable blueprint */ \
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
      if (solution->skills==0xffff) continue; /* intentionally unusable blueprint */ \
      if (solution->skills&PS_SKILL_##tag) { \
        have_solution_for_##tag=1; \
        goto _solution_##tag##_ready_; \
      } \
    } \
   _solution_##tag##_ready_: \
    REQUIRE(have_solution_for_##tag) \
  }

  /* For transient use: List all blueprints that have a solution involving this skill. */
  #define SHOW_SOLUTIONS_FOR_SKILL(tag) { \
    ITERATE { \
      if (solution->skills&PS_SKILL_##tag) { \
        ps_log(TEST,INFO,"%s: %3d %04x",#tag,res->id,solution->skills); \
      } \
    } \
  }

  /* Because it's convenient right here, optionally do some extra analysis: */
  //SHOW_SOLUTIONS_FOR_SKILL(SPEED)

  /* Now declare how each skill is required to be used: */
  MUST_HAVE_LONE_SOLUTION(SWORD)
  MUST_HAVE_SOLUTION(ARROW)
  MUST_HAVE_LONE_SOLUTION(HOOKSHOT)
  MUST_HAVE_LONE_SOLUTION(FLAME)
  MUST_HAVE_LONE_SOLUTION(HEAL)
  MUST_HAVE_LONE_SOLUTION(IMMORTAL)
  MUST_HAVE_LONE_SOLUTION(FLY)
  MUST_HAVE_SOLUTION(MARTYR)
  MUST_HAVE_SOLUTION(COMBAT)

  #undef ITERATE
  #undef REQUIRE
  #undef MUST_HAVE_LONE_SOLUTION
  #undef MUST_NOT_HAVE_SOLUTION
  #undef MUST_NOT_HAVE_LONE_SOLUTION
  #undef MUST_HAVE_SOLUTION
  #undef SHOW_SOLUTIONS_FOR_SKILL

  return 0;
}

/* Count blueprints 2-dimensionally: player count against difficulty.
 * Ensure that each configuration has a healthy blueprint count.
 * Hmmm... This test doesn't tell us much, because it disregards skills.
 */

static int count_blueprints_by_player_count_and_difficulty() {

  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  PS_ASSERT(restype);

  // These are both 1-based dimensions. For sanity's sake, leave an empty column and row.
  int blueprintc[1+PS_PLAYER_LIMIT][1+PS_DIFFICULTY_MAX]={0};

  int resi=restype->resc; while (resi-->0) {
    const struct ps_blueprint *blueprint=restype->resv[resi].obj;
    PS_ASSERT(blueprint)
    int playerc=1; for (;playerc<=PS_PLAYER_LIMIT;playerc++) {
      int difficulty=1; for (;difficulty<=PS_DIFFICULTY_MAX;difficulty++) {
        uint8_t solution_difficulty;
        if (ps_blueprint_is_solvable(&solution_difficulty,blueprint,playerc,0xffff)) {
          if (solution_difficulty<=difficulty) {
            blueprintc[playerc][difficulty]++;
          }
        }
      }
    }
  }

  ps_log(TEST,INFO,"PC   1   2   3   4   5   6   7   8   9");
  int playerc=1; for (;playerc<=PS_PLAYER_LIMIT;playerc++) {
    char message[256];
    int messagec=0;
    int difficulty=1; for (;difficulty<=PS_DIFFICULTY_MAX;difficulty++) {
      messagec+=snprintf(message+messagec,sizeof(message)-messagec,"%4d",blueprintc[playerc][difficulty]);
    }
    ps_log(TEST,INFO,"%2d%.*s",playerc,messagec,message);
  }
  
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
  if (count_blueprints_by_player_count_and_difficulty()<0) return -1;

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
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bloodhound))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_blueberry))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bomb))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_boxingglove))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bug))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_bumblebat))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_chicken))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_dragon))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_dragonbug))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_dummy))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_elefence))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_explosion))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_giraffe))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_gorilla))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_hero))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_hookshot))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_killozap))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_lobster))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_lwizard))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_missile))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_motionsensor))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_penguin))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_prize))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_rabbit))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_seamonster))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_skeleton))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_snake))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_switch))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_swordswitch))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_teleporter))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_treasurechest))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_turtle))
  PS_ASSERT(sprdef_for_type(&ps_sprtype_yak))

  /* tilesheet */
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,1),"UI chrome")
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,3),"hero")
  PS_ASSERT(resource_exists(PS_RESTYPE_TILESHEET,4),"monsters")

  ps_resmgr_quit();
  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=pvloglevel;
  return 0;
}

/* List all blueprints with a HERO POI.
 */
 
static int list_hero_blueprints() {
  const struct ps_restype *restype=PS_RESTYPE(BLUEPRINT);
  ps_log(RES,INFO,"Have %d blueprints total.",restype->resc);
  int i=restype->resc;
  const struct ps_res *res=restype->resv;
  for (;i-->0;res++) {
    const struct ps_blueprint *blueprint=res->obj;
    int havepoi[1+PS_PLAYER_LIMIT]={0};
    int havesome=0;
    const struct ps_blueprint_poi *poi=blueprint->poiv;
    int j=blueprint->poic;
    for (;j-->0;poi++) {
      if (poi->type==PS_BLUEPRINT_POI_HERO) {
        havesome=1;
        if ((poi->argv[0]>=1)&&(poi->argv[0]<=PS_PLAYER_LIMIT)) {
          havepoi[poi->argv[0]]++;
        } else {
          ps_log(RES,ERROR,"blueprint:%d: Invalid HERO POI for player#%d",res->id,poi->argv[0]);
          havepoi[0]++;
        }
      }
    }
    if (!havesome) continue;
    int ok=1;
    for (j=1;j<=PS_PLAYER_LIMIT;j++) {
      if (!havepoi[j]) {
        ps_log(RES,ERROR,"blueprint:%d: Missing player#%d",res->id,j);
        ok=0;
      } else if (havepoi[j]!=1) {
        ps_log(RES,ERROR,"blueprint:%d: Multiple HERO POI for player#%d (%d)",res->id,j,havepoi[j]);
        ok=0;
      }
    }
    if (ok) {
      ps_log(RES,INFO,"blueprint:%d: Have all %d HERO POI. Valid start screen.",res->id,PS_PLAYER_LIMIT);
    }
  }
  return 0;
}

/* Analyze resources (transient helper, not a real test).
 */
 
PS_TEST(examineres,ignore) {
  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))
  
  PS_ASSERT_CALL(list_hero_blueprints())
  
  ps_resmgr_quit();
  return 0;
}
