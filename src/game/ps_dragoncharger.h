/* ps_dragoncharger.h
 * Manages the feature where two or more dead players can tap their action 
 * buttons rapidly and transform into a dragon which they control jointly.
 */

#ifndef PS_DRAGONCHARGER_H
#define PS_DRAGONCHARGER_H

struct ps_game;

struct ps_dragoncharger {
  int refc;
  int charge[PS_PLAYER_LIMIT];
  int pvcharge[PS_PLAYER_LIMIT];
};

struct ps_dragoncharger *ps_dragoncharger_new();
void ps_dragoncharger_del(struct ps_dragoncharger *charger);
int ps_dragoncharger_ref(struct ps_dragoncharger *charger);

int ps_dragoncharger_update(struct ps_dragoncharger *charger,struct ps_game *game);

int ps_dragoncharger_charge(struct ps_dragoncharger *charger,struct ps_game *game,int playerid);

#endif
