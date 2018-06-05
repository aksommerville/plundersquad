#include "ps.h"
#include "ps_scgen.h"
#include "ps_blueprint.h"
#include "ps_blueprint_chooser.h"
#include "ps_screen.h"
#include "ps_scenario.h"
#include "res/ps_resmgr.h"

/* Object definition.
 */

struct ps_blueprint_chooser {
  struct ps_scgen *scgen; // required

  // Blueprints are sorted into these lists:
  struct ps_blueprint_list *homes;      // Contains HERO POI. We will only use one.
  struct ps_blueprint_list *treasures;  // Contains TREASURE POI.
  struct ps_blueprint_list *challenges; // Contains a solution. These are weighted, see below.
  struct ps_blueprint_list *fillers;    // Unchallenged filler screens.

  // We calculate a "weight" for each challenge blueprint, higher is more desirable.
  int *challenge_weightv;
  int challenge_weight_total;
  
};

/* Object lifecycle.
 */

struct ps_blueprint_chooser *ps_blueprint_chooser_new(struct ps_scgen *scgen) {
  if (!scgen) return 0;
  if (!scgen->scenario) return 0;
  struct ps_blueprint_chooser *chooser=calloc(1,sizeof(struct ps_blueprint_chooser));
  if (!chooser) return 0;

  chooser->scgen=scgen;

  return chooser;
}

void ps_blueprint_chooser_del(struct ps_blueprint_chooser *chooser) {
  if (!chooser) return;

  ps_blueprint_list_del(chooser->homes);
  ps_blueprint_list_del(chooser->treasures);
  ps_blueprint_list_del(chooser->challenges);
  ps_blueprint_list_del(chooser->fillers);

  if (chooser->challenge_weightv) free(chooser->challenge_weightv);
  
  free(chooser);
}

/* Acquire global blueprint list if we haven't yet.
 * Note that this stores in (chooser->scgen), not in (chooser) itself.
 */

static int ps_blueprint_chooser_ensure_global_blueprints(struct ps_blueprint_chooser *chooser) {
  if (!chooser->scgen->blueprints) {
    if (!(chooser->scgen->blueprints=ps_blueprint_list_new())) return -1;
    if (ps_blueprint_list_match_resources(
      chooser->scgen->blueprints,chooser->scgen->playerc,chooser->scgen->skills,chooser->scgen->difficulty
    )<0) return -1;
  }
  ps_log(GENERATOR,DEBUG,"Have %d legal blueprints globally.",chooser->scgen->blueprints->c);
  return 0;
}

/* Sort blueprints into four lists.
 */

static int ps_blueprint_chooser_sort_blueprints(struct ps_blueprint_chooser *chooser) {

  #define SETUPLIST(name) \
    if (chooser->name) { \
      if (ps_blueprint_list_clear(chooser->name)<0) return -1; \
    } else { \
      if (!(chooser->name=ps_blueprint_list_new())) return -1; \
    }
  SETUPLIST(homes)
  SETUPLIST(treasures)
  SETUPLIST(challenges)
  SETUPLIST(fillers)
  #undef SETUPLIST

  int i=chooser->scgen->blueprints->c;
  while (i-->0) {
    struct ps_blueprint *blueprint=chooser->scgen->blueprints->v[i];
    struct ps_blueprint_list *list=0;

    if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_HERO)>0) {
      list=chooser->homes;
    } else if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_TREASURE)>0) {
      list=chooser->treasures;
    } else if (blueprint->solutionc>0) {
      list=chooser->challenges;
    } else {
      list=chooser->fillers;
    }

    if (list) {
      if (ps_blueprint_list_add(list,blueprint)<0) return -1;
    } else {
      ps_log(GENERATOR,DEBUG,"Discarding blueprint:%d",ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,blueprint));
    }
  }

  return 0;
}

/* After populating lists, ensure that we have a valid set to pull from.
 */

static int ps_blueprint_chooser_assert_valid_blueprint_sets(const struct ps_blueprint_chooser *chooser) {

  // Home and treasure, we only need one of each.
  if (chooser->homes->c<1) return -1;
  if (chooser->treasures->c<1) return -1;

  // Determine how many screens will get challenge or filler.
  int screenc=chooser->scgen->scenario->w*chooser->scgen->scenario->h;
  int screenc_net=screenc-1-chooser->scgen->treasurec;

  // Assume that challenges will be used first.
  // Challenged blueprints are never repeated within one scenario.
  // If that covers the whole scenario, we're done.
  screenc_net-=chooser->challenges->c;
  if (screenc_net<=0) return 0;

  // Filler blueprints may be repeated, so as long as we have at least one, we're good.
  if (chooser->fillers->c>0) return 0;
  return -1;
}

/* Weigh one challenged blueprint.
 * Higher values are more likely to be chosen.
 * A blueprint is more preferred if its difficulty is higher, or its solution player count is higher.
 */

static int ps_blueprint_chooser_weigh_challenge(const struct ps_blueprint_chooser *chooser,const struct ps_blueprint *blueprint) {
  struct ps_blueprint_solution solution={0};
  int solutionc=ps_blueprint_analyze_solutions(&solution,blueprint,chooser->scgen->playerc,chooser->scgen->skills);
  if (solutionc<1) return 0;

  int weight=100; // Start at a reasonable offset. Selection will be mostly random.
  weight+=solutionc*20; // More valid solutions means a more interesting blueprint.
  weight+=solution.difficulty*64; // More difficult is more fun.
  weight+=solution.plo*32; // More players required is more fun.
  weight+=solution.preference; // Consider the author's opinion of this solution.
  
  return weight;
}

/* Reallocate the challenge weight list and calculate its content.
 */

static int ps_blueprint_chooser_calculate_challenge_weights(struct ps_blueprint_chooser *chooser) {

  if (chooser->challenge_weightv) free(chooser->challenge_weightv);
  if (!(chooser->challenge_weightv=calloc(sizeof(int),chooser->challenges->c))) return -1;
  chooser->challenge_weight_total=0;

  int i=chooser->challenges->c;
  while (i-->0) {
    const struct ps_blueprint *blueprint=chooser->challenges->v[i];
    int weight=ps_blueprint_chooser_weigh_challenge(chooser,blueprint);
    if (weight<1) weight=1; // Weight must be at least one, because we've already assumed that all blueprints are playable.
    chooser->challenge_weightv[i]=weight;
    if (chooser->challenge_weight_total>INT_MAX-weight) {
      chooser->challenge_weight_total=INT_MAX;
    } else {
      chooser->challenge_weight_total+=weight;
    }
  }

  return 0;
}

/* Remove a challenge blueprint from consideration.
 */

static int ps_blueprint_chooser_remove_challenge(struct ps_blueprint_chooser *chooser,int p) {
  chooser->challenge_weight_total-=chooser->challenge_weightv[p];
  if (ps_blueprint_list_remove_at(chooser->challenges,p)<0) return -1;
  memmove(chooser->challenge_weightv+p,chooser->challenge_weightv+p+1,sizeof(int)*(chooser->challenges->c-p));
  return 0;
}

/* Select blueprint for a prime challenge.
 * If we return NULL here, the operation will proceed and treat this screen as an ordinary challenge or filler.
 * If we do return a blueprint, we have also removed it.
 */

static struct ps_blueprint *ps_blueprint_chooser_choose_prime_challenge(struct ps_blueprint_chooser *chooser,const struct ps_screen *screen) {
  int maxp=-1,maxweight=0;
  struct ps_blueprint *preferred=0;
  int i=chooser->challenges->c; while (i-->0) {
    if (chooser->challenge_weightv[i]<=maxweight) continue;
    preferred=chooser->challenges->v[i];
    maxp=i;
    maxweight=chooser->challenge_weightv[i];
  }
  if (!preferred) return 0;
  if (ps_blueprint_chooser_remove_challenge(chooser,maxp)<0) return 0;
  return preferred;
}

/* Select a challenge blueprint randomly.
 * This does not remove it.
 */

static int ps_blueprint_chooser_choose_challenge(const struct ps_blueprint_chooser *chooser) {
  int selection=rand()%chooser->challenge_weight_total;
  int i=chooser->challenges->c; while (i-->0) {
    selection-=chooser->challenge_weightv[i];
    if (selection<0) return i;
  }
  return 0;
}

/* Consider what kind of blueprint this screen needs and select one randomly.
 */

static struct ps_blueprint *ps_blueprint_chooser_choose_blueprint(struct ps_blueprint_chooser *chooser,const struct ps_screen *screen) {

  /* Use a random treasure blueprint if the TREASURE feature is set.
   */
  if (screen->features&PS_SCREEN_FEATURE_TREASURE) {
    return chooser->treasures->v[rand()%chooser->treasures->c];
  }

  /* Use a random home blueprint if the HOME feature is set.
   */
  if (screen->features&PS_SCREEN_FEATURE_HOME) {
    return chooser->homes->v[rand()%chooser->homes->c];
  }

  /* If this is a leaf node (not treasure, so it's a dead end), use filler.
   * If we don't have any filler, that's fine, skip this bit.
   */
  if ((chooser->fillers->c>0)&&(ps_screen_count_doors(screen)==1)) {
    return chooser->fillers->v[rand()%chooser->fillers->c];
  }

  /* If we have any challenge blueprints left, select one with weighted random.
   */
  if ((chooser->challenges->c>0)&&(chooser->challenge_weight_total>0)) {
    int p=ps_blueprint_chooser_choose_challenge(chooser);
    if ((p<0)||(p>=chooser->challenges->c)) return 0;
    struct ps_blueprint *blueprint=chooser->challenges->v[p];
    if (ps_blueprint_chooser_remove_challenge(chooser,p)<0) return 0;
    return blueprint;
  }

  /* No fillers? Shouldn't have been possible to reach this point.
   */
  if (chooser->fillers->c<1) return 0;
  return chooser->fillers->v[rand()%chooser->fillers->c];
}

/* Choose a transform for one screen. No errors.
 */

static uint8_t ps_blueprint_chooser_choose_xform(const struct ps_blueprint_chooser *chooser,const struct ps_screen *screen) {

  /* If the blueprint has no challenge, it's purely random and doesn't mean much.
   * And if some damn fool put a solution on a HOME or TREASURE blueprint, we'll ignore it.
   */
  if (screen->blueprint->solutionc<1) return rand()&3;
  if (screen->features&(PS_SCREEN_FEATURE_HOME|PS_SCREEN_FEATURE_TREASURE)) return rand()&3;

  int awayward=ps_screen_get_single_awayward_direction(screen);
  uint8_t alt_axis_bit=0;
  uint8_t xform=0;

  /* Always point the entry (N/W) side of the blueprint homeward.
   * Record the alternate axis.
   */
  switch (screen->direction_home) {
    case PS_DIRECTION_NORTH: alt_axis_bit=PS_BLUEPRINT_XFORM_HORZ; break;
    case PS_DIRECTION_SOUTH: alt_axis_bit=PS_BLUEPRINT_XFORM_HORZ; xform=PS_BLUEPRINT_XFORM_VERT; break;
    case PS_DIRECTION_WEST: alt_axis_bit=PS_BLUEPRINT_XFORM_VERT; break;
    case PS_DIRECTION_EAST: alt_axis_bit=PS_BLUEPRINT_XFORM_VERT; xform=PS_BLUEPRINT_XFORM_HORZ; break;
  }

  /* If we have a single awayward direction, ie a 2-door screen, ensure that the awayward direction is on the exit (S/E) side.
   * Without an awayward direction (likely), leave (alt_axis_bit) set to randomize.
   */
  switch (awayward) {
    case PS_DIRECTION_NORTH: if (alt_axis_bit==PS_BLUEPRINT_XFORM_VERT) alt_axis_bit=0; xform|=PS_BLUEPRINT_XFORM_VERT; break;
    case PS_DIRECTION_SOUTH: if (alt_axis_bit==PS_BLUEPRINT_XFORM_VERT) alt_axis_bit=0; break;
    case PS_DIRECTION_WEST: if (alt_axis_bit==PS_BLUEPRINT_XFORM_HORZ) alt_axis_bit=0; xform|=PS_BLUEPRINT_XFORM_HORZ; break;
    case PS_DIRECTION_EAST: if (alt_axis_bit==PS_BLUEPRINT_XFORM_HORZ) alt_axis_bit=0; break;
  }

  /* TODO: Additional logic could be applied here, to try to point the exit side treasureward.
   * A player naturally assumes that he has to take the more difficult path to get the reward.
   */

  /* If (alt_axis_bit) is still set, apply it 50% of the time.
   */
  if (alt_axis_bit&&(rand()&1)) {
    xform|=alt_axis_bit;
  }
  
  return xform;
}

/* Walk the scenario, visiting each screen in turn.
 */

static int ps_blueprint_chooser_iterate_screens(struct ps_blueprint_chooser *chooser) {

  /* Make one pass first for prime challenges. */
  struct ps_screen *screen=chooser->scgen->scenario->screenv;
  int i=chooser->scgen->scenario->w*chooser->scgen->scenario->h;
  for (;i-->0;screen++) {
    if (screen->features&PS_SCREEN_FEATURE_CHALLENGE) {
      struct ps_blueprint *blueprint=ps_blueprint_chooser_choose_prime_challenge(chooser,screen);
      if (blueprint) {
        if (ps_screen_set_blueprint(screen,blueprint)<0) return -1;
      }
    }
  }

  /* Now assign everything else, more or less randomly. */
  for (screen=chooser->scgen->scenario->screenv,i=chooser->scgen->scenario->w*chooser->scgen->scenario->h;i-->0;screen++) {
    if (!screen->blueprint) {
      struct ps_blueprint *blueprint=ps_blueprint_chooser_choose_blueprint(chooser,screen);
      if (!blueprint) return -1;
      if (ps_screen_set_blueprint(screen,blueprint)<0) return -1;
    }
    screen->xform=ps_blueprint_chooser_choose_xform(chooser,screen);
  }
  return 0;
}

/* Main entry point.
 */

int ps_blueprint_chooser_populate_screens(struct ps_blueprint_chooser *chooser) {
  if (!chooser) return -1;
  if (ps_blueprint_chooser_ensure_global_blueprints(chooser)<0) return -1;
  if (ps_blueprint_chooser_sort_blueprints(chooser)<0) return -1;
  if (ps_blueprint_chooser_assert_valid_blueprint_sets(chooser)<0) return -1;
  if (ps_blueprint_chooser_calculate_challenge_weights(chooser)<0) return -1;
  if (ps_blueprint_chooser_iterate_screens(chooser)<0) return -1;
  return 0;
}
