#include "ps.h"
#include "ps_bloodhound_activator.h"
#include "ps_game.h"
#include "ps_stats.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"

//TODO Tweak bloodhound activation constants.

#define PS_BLOODHOUND_MAX_DIFFICULTY 5 /* Bloodhound is present at this difficulty but no higher. */
#define PS_BLOODHOUND_AFTER_TREASURE_TIME 600 /* Don't appear soon after a treasure is collected. */
#define PS_BLOODHOUND_STEPC_EACH_MIN 300 /* Each player must walk so far between bloodhound, also must be alive. */

/* The average step count since heal per player must be above this, for each difficulty level. */
static const int ps_bloodhound_stepc_average_by_difficulty[]={
  600, // dummy
  600, // 1
  600, // 2
  600, // 3
  600, // 4
  600, // 5
};

static const int ps_bloodhound_activation_interval_by_difficulty[]={
  0,   // dummy
  1200, // 1
  1200, // 2
  1200, // 3
  1200, // 4
  1200, // 5
};

/* Object lifecycle.
 */
 
struct ps_bloodhound_activator *ps_bloodhound_activator_new() {
  struct ps_bloodhound_activator *activator=calloc(1,sizeof(struct ps_bloodhound_activator));
  if (!activator) return 0;

  activator->refc=1;

  return activator;
}

void ps_bloodhound_activator_del(struct ps_bloodhound_activator *activator) {
  if (!activator) return;
  if (activator->refc-->1) return;

  free(activator);
}

int ps_bloodhound_activator_ref(struct ps_bloodhound_activator *activator) {
  if (!activator) return -1;
  if (activator->refc<1) return -1;
  if (activator->refc==INT_MAX) return -1;
  activator->refc++;
  return 0;
}

/* Reset.
 */

int ps_bloodhound_activator_reset(struct ps_bloodhound_activator *activator) {
  if (!activator) return -1;
  activator->gridx=-1;
  activator->gridy=-1;
  activator->done_for_screen=0;
  activator->pvtime=0;
  return 0;
}

/* Is there uncollected treasure on the current screen?
 */

static int ps_bloodhound_uncollected_treasure_present_here(const struct ps_game *game) {
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  int i=game->grid->poic;
  for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
      if (poi->argv[0]<0) continue;
      if (poi->argv[0]>=PS_TREASURE_LIMIT) continue;
      if (!game->treasurev[poi->argv[0]]) return 1;
    }
  }
  return 0;
}

/* Update.
 */

int ps_bloodhound_activator_update(struct ps_bloodhound_activator *activator,struct ps_game *game) {
  if (!activator||!game) return -1;
  if (game->playerc<1) return 0;

  /* Difficulty limit. */
  if (game->difficulty>PS_BLOODHOUND_MAX_DIFFICULTY) return 0;

  /* Update current screen and abort if already activated here. */
  if ((game->gridx!=activator->gridx)||(game->gridy!=activator->gridy)) {
    activator->gridx=game->gridx;
    activator->gridy=game->gridy;
    activator->done_for_screen=0;
  } else if (activator->done_for_screen) return 0;

  /* Require a decent interval after collecting treasure. */
  if (game->stats->framec_since_treasure<PS_BLOODHOUND_AFTER_TREASURE_TIME) return 0;

  /* Don't appear if there is uncollected treasure on this screen. */
  if (ps_bloodhound_uncollected_treasure_present_here(game)) return 0;

  /* Require a decent interval between activations, based on difficulty. */
  int elapsed=game->stats->playtime-activator->pvtime;
  if (elapsed<ps_bloodhound_activation_interval_by_difficulty[game->difficulty]) return 0;

  /* Gather player step counts since healing, abort if anyone is dead. */
  int stepc=0;
  int i; for (i=0;i<game->playerc;i++) {
    const struct ps_stats_player *pstats=game->stats->playerv+i;
    if (pstats->stepc_since_rebirth<PS_BLOODHOUND_STEPC_EACH_MIN) return 0;
    stepc+=pstats->stepc_since_rebirth;
  }
  stepc/=game->playerc;
  if (stepc<ps_bloodhound_stepc_average_by_difficulty[game->difficulty]) return 0;

  /* OK, let's do it. */
  if (ps_game_summon_bloodhound(game)<0) return -1;
  activator->done_for_screen=1;
  activator->pvtime=game->stats->playtime;
  
  return 0;
}
