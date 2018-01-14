/* ps_statusreport.h
 * Map or treasure list for display to the player, as part of the visible grid.
 * This is maintained by ps_game.
 */

#ifndef PS_STATUSREPORT_H
#define PS_STATUSREPORT_H

struct ps_game;
struct akgl_texture;

struct ps_statusreport {
  int refc;
  struct akgl_texture *texture;
  int dstx,dsty,dstw,dsth;
};

struct ps_statusreport *ps_statusreport_new();
void ps_statusreport_del(struct ps_statusreport *report);
int ps_statusreport_ref(struct ps_statusreport *report);

int ps_statusreport_setup(struct ps_statusreport *report,const struct ps_game *game,int x,int y,int w,int h);

int ps_statusreport_draw(struct ps_statusreport *report,int offx,int offy);

#endif
