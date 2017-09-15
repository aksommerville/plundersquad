#include "ps.h"
#include "ps_player.h"

/* Object lifecycle.
 */

struct ps_player *ps_player_new() {
  struct ps_player *player=calloc(1,sizeof(struct ps_player));
  if (!player) return 0;

  player->refc=1;

  return player;
}

void ps_player_del(struct ps_player *player) {
  if (!player) return;
  if (player->refc-->1) return;

  free(player);
}

int ps_player_ref(struct ps_player *player) {
  if (!player) return -1;
  if (player->refc<1) return -1;
  if (player->refc==INT_MAX) return -1;
  player->refc++;
  return 0;
}
