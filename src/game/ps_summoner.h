/* ps_summoner.h
 * Coordinates instantiation of sprites during play (ie not at screen load).
 */

#ifndef PS_SUMMONER_H
#define PS_SUMMONER_H

struct ps_game;
struct ps_path;
struct ps_sprgrp;
struct ps_sprdef;

struct ps_summoner_entry {
  int volume;
  int delay; // Counts down to next instantiation.
  int spritelimit;
  struct ps_sprdef *sprdef; // WEAK
  struct ps_path *cells;
  struct ps_sprgrp *alive;
};

struct ps_summoner {
  int refc;
  struct ps_summoner_entry *entryv;
  int entryc,entrya;
};

struct ps_summoner *ps_summoner_new();
void ps_summoner_del(struct ps_summoner *summoner);
int ps_summoner_ref(struct ps_summoner *summoner);

int ps_summoner_reset(struct ps_summoner *summoner,struct ps_game *game);

int ps_summoner_update(struct ps_summoner *summoner,struct ps_game *game);

#endif
