/* ps_player.h
 */

#ifndef PS_PLAYER_H
#define PS_PLAYER_H

struct ps_player {
  int refc;
  int playerid; // 1..8
  uint16_t skills;
};

struct ps_player *ps_player_new();
void ps_player_del(struct ps_player *player);
int ps_player_ref(struct ps_player *player);

#endif
