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
