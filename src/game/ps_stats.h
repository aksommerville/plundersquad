/* ps_stats.h
 * Record of high-level events during gameplay.
 * ps_game always has one.
 * This encodes into the encoded game (TODO).
 */

#ifndef PS_STATS_H
#define PS_STATS_H

struct ps_stats_player {
  int stepc; // How many frames did I spend walking?
  int killc_monster; // How many monsters killed.
  int killc_hero; // How many heroes killed.
  int deathc; // How many times killed.
  int switchc; // How many switches actuated.
  int framec_alive; // Total count of frames while alive.
  int stepc_since_rebirth; // Count of steps since healed.
  int framec_since_rebirth; // Count of frames since healed.
  int treasurec; // Count of treasures collected.
};

/* One-time deeds.
 * We record the coordinates of the screen where a puzzle was solved.
 * Optionally, it may remain solved.
 * eg big monsters only need to be killed once.
 */
struct ps_deed {
  int x,y;
};

struct ps_stats {
  int refc;
  int playtime; // Count of updates.
  int framec_since_treasure;
  struct ps_stats_player playerv[PS_PLAYER_LIMIT];
  struct ps_deed *deedv;
  int deedc,deeda;
};

struct ps_stats *ps_stats_new();
void ps_stats_del(struct ps_stats *stats);
int ps_stats_ref(struct ps_stats *stats);

int ps_stats_clear(struct ps_stats *stats);

void ps_stats_dump(const struct ps_stats *stats);

int ps_stats_check_deed(const struct ps_stats *stats,int x,int y);
int ps_stats_set_deed(struct ps_stats *stats,int x,int y);
int ps_stats_remove_deed(struct ps_stats *stats,int x,int y);

#endif
