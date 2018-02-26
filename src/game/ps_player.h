/* ps_player.h
 */

#ifndef PS_PLAYER_H
#define PS_PLAYER_H

struct ps_plrdef;

/* Awards. */
#define PS_AWARD_NONE              0 /* Always this during play, never after game-over. */
#define PS_AWARD_PARTICIPANT       1 /* Fallback if we really can't find any other. */
#define PS_AWARD_WALKER            2 /* Took the most steps. */
#define PS_AWARD_KILLER            3 /* Killed the most monsters. */
#define PS_AWARD_TRAITOR           4 /* Killed the most heroes. */
#define PS_AWARD_DEATH             5 /* Died the most. */
#define PS_AWARD_ACTUATOR          6 /* Actuated the most switches. */
#define PS_AWARD_LIVELY            7 /* Spent the most time alive. */
#define PS_AWARD_GHOST             8 /* Spent the most time dead. */
#define PS_AWARD_TREASURE          9 /* Collected the most treasure. */
#define PS_AWARD_LAZY             10 /* Took the fewest steps. */
#define PS_AWARD_PACIFIST         11 /* Fewest total kills. */
#define PS_AWARD_GENEROUS         12 /* Only player to collect no treasure. */
#define PS_AWARD_HARD_TARGET      13 /* Fewest deaths. */
#define PS_AWARD_COUNT            14

struct ps_player {
  int refc;
  int playerid; // 1..8
  const struct ps_plrdef *plrdef; // WEAK, owned by resource manager
  int palette; // index into (plrdef->palettev)
  int award;
};

struct ps_player *ps_player_new();
void ps_player_del(struct ps_player *player);
int ps_player_ref(struct ps_player *player);

#endif
