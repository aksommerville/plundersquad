#include "ps.h"
#include "ps_gamelog.h"
#include "os/ps_fs.h"

#define PS_GAMELOG_PATH "etc/playtest/gamelog"
#define PS_GAMELOG_SAVE_INTERVAL 600

/* Object definition.
 */

struct ps_gamelog {

  int refc;
  int framec;
  int dirty;
  int save_framep;

  // Index is blueprintid. Value is mask of player counts.
  // For now, this is the whole file, verbatim.
  uint8_t *blueprint_usage;
  int blueprint_usagec,blueprint_usagea;
  
};

/* Object lifecycle.
 */
 
struct ps_gamelog *ps_gamelog_new() {
  struct ps_gamelog *gamelog=calloc(1,sizeof(struct ps_gamelog));
  if (!gamelog) return 0;

  gamelog->refc=1;

  if (ps_gamelog_load(gamelog)<0) {
    ps_gamelog_del(gamelog);
    return 0;
  }

  return gamelog;
}

void ps_gamelog_del(struct ps_gamelog *gamelog) {
  if (!gamelog) return;
  if (gamelog->refc-->1) return;

  free(gamelog);
}

int ps_gamelog_ref(struct ps_gamelog *gamelog) {
  if (!gamelog) return -1;
  if (gamelog->refc<1) return -1;
  if (gamelog->refc==INT_MAX) return -1;
  gamelog->refc++;
  return 0;
}

/* Update frame counter.
 */

int ps_gamelog_tick(struct ps_gamelog *gamelog) {
  if (!gamelog) return -1;
  
  gamelog->framec++;

  if (gamelog->dirty&&(gamelog->framec-gamelog->save_framep>=PS_GAMELOG_SAVE_INTERVAL)) {
    if (ps_gamelog_save(gamelog)<0) return -1;
  }

  return 0;
}

/* Save.
 */

int ps_gamelog_save(struct ps_gamelog *gamelog) {
  if (!gamelog) return -1;
  if (ps_file_write(PS_GAMELOG_PATH,gamelog->blueprint_usage,gamelog->blueprint_usagec)<0) return -1;
  ps_log(GAME,DEBUG,"Rewrote %s",PS_GAMELOG_PATH);
  gamelog->dirty=0;
  gamelog->save_framep=gamelog->framec;
  return 0;
}

/* Load.
 */

int ps_gamelog_load(struct ps_gamelog *gamelog) {
  if (!gamelog) return -1;
  void *src=0;
  int srcc=ps_file_read(&src,PS_GAMELOG_PATH);
  if ((srcc<0)||!src) return ps_gamelog_clear(gamelog);
  if (gamelog->blueprint_usage) free(gamelog->blueprint_usage);
  gamelog->blueprint_usage=src;
  gamelog->blueprint_usagec=srcc;
  gamelog->blueprint_usagea=srcc;
  gamelog->dirty=0;
  return 0;
}

/* Clear.
 */

int ps_gamelog_clear(struct ps_gamelog *gamelog) {
  if (!gamelog) return -1;
  gamelog->blueprint_usagec=0;
  gamelog->dirty=0;
  return 0;
}

/* Require blueprint usage.
 */

static int ps_gamelog_require_blueprint_usage(struct ps_gamelog *gamelog,int blueprintid) {
  if (blueprintid<gamelog->blueprint_usagec) return 0;
  int na=blueprintid+1;
  int nc=na;
  if (na>gamelog->blueprint_usagea) {
    if (na<INT_MAX-32) na=(na+32)&~31;
    void *nv=realloc(gamelog->blueprint_usage,na);
    if (!nv) return -1;
    gamelog->blueprint_usage=nv;
    gamelog->blueprint_usagea=na;
  }
  memset(gamelog->blueprint_usage+gamelog->blueprint_usagec,0,nc-gamelog->blueprint_usagec);
  gamelog->blueprint_usagec=nc;
  return 0;
}

/* Log blueprint usage.
 */

int ps_gamelog_blueprint_used(struct ps_gamelog *gamelog,int blueprintid,int playerc) {
  if (!gamelog) return -1;
  if ((playerc<1)||(playerc>8)) return -1;
  if (blueprintid<0) return -1;
  if (ps_gamelog_require_blueprint_usage(gamelog,blueprintid)<0) return -1;
  uint8_t mask=(1<<(playerc-1));
  if (gamelog->blueprint_usage[blueprintid]&mask) return 0;
  gamelog->blueprint_usage[blueprintid]|=mask;
  gamelog->dirty=1;
  return 0;
}
