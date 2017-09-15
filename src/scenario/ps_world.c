#include "ps.h"
#include "ps_world.h"

/* Close all doors leading off-world.
 */

static void ps_world_seal_edges(struct ps_world *world) {
  int xz=world->w-1;
  int yz=world->h-1;
  int i=world->w*world->h;
  struct ps_screen *screen=world->v;
  for (;i-->0;screen++) {
    if (!screen->x) screen->doorw=PS_DOOR_CLOSED;
    if (!screen->y) screen->doorn=PS_DOOR_CLOSED;
    if (screen->x==xz) screen->doore=PS_DOOR_CLOSED;
    if (screen->y==yz) screen->doors=PS_DOOR_CLOSED;
  }
}

/* New.
 */

struct ps_world *ps_world_new(int w,int h) {

  if ((w<1)||(w>PS_WORLD_W_LIMIT)) return 0;
  if ((h<1)||(h>PS_WORLD_H_LIMIT)) return 0;
  int screenc=w*h;

  struct ps_world *world=calloc(1,sizeof(struct ps_world)+sizeof(struct ps_screen)*screenc);
  if (!world) return 0;

  world->w=w;
  world->h=h;

  /* Assign (x,y) to each screen. */
  uint8_t x=0,y=0;
  struct ps_screen *screen=world->v;
  while (screenc-->0) {
    screen->x=x;
    screen->y=y;
    if (++x>=w) {
      x=0;
      y++;
    }
    screen++;
  }

  ps_world_seal_edges(world);

  return world;
}

/* Delete.
 */

void ps_world_del(struct ps_world *world) {
  if (!world) return;

  int screenc=world->w*world->h;
  while (screenc-->0) ps_screen_cleanup(world->v+screenc);

  free(world);
}

/* Trivial accessors.
 */
 
struct ps_screen *ps_world_get_screen(struct ps_world *world,int x,int y) {
  if (!world) return 0;
  if ((x<0)||(x>=world->w)) return 0;
  if ((y<0)||(y>=world->h)) return 0;
  return PS_WORLD_SCREEN(world,x,y);
}
