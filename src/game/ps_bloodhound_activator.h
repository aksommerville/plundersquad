/* ps_bloodhound_activator.h
 * Manages the business logic to decide when a bloodhound should appear.
 * We receive a generic update each frame and a nudge at reset.
 */

#ifndef PS_BLOODHOUND_ACTIVATOR_H
#define PS_BLOODHOUND_ACTIVATOR_H

struct ps_game;

struct ps_bloodhound_activator {
  int refc;
  int gridx,gridy; // The last screen we updated on.
  int done_for_screen; // If nonzero, screen must change.
  int pvtime; // Timestamp of last activation. (from game->stats->playtime)
};

struct ps_bloodhound_activator *ps_bloodhound_activator_new();
void ps_bloodhound_activator_del(struct ps_bloodhound_activator *activator);
int ps_bloodhound_activator_ref(struct ps_bloodhound_activator *activator);

int ps_bloodhound_activator_reset(struct ps_bloodhound_activator *activator);

int ps_bloodhound_activator_update(struct ps_bloodhound_activator *activator,struct ps_game *game);

#endif
