/* ps_player.h
 */

#ifndef PS_PLAYER_H
#define PS_PLAYER_H

struct ps_plrdef;

struct ps_player {
  int refc;
  int playerid; // 1..8
  const struct ps_plrdef *plrdef; // WEAK, owned by resource manager
  int palette; // index into (plrdef->palettev)
};

struct ps_player *ps_player_new();
void ps_player_del(struct ps_player *player);
int ps_player_ref(struct ps_player *player);

#endif
