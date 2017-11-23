/* ps_screen.h
 * One screenful of map data.
 * Screens have a serial format, which is managed by ps_scenario.
 */

#ifndef PS_SCREEN_H
#define PS_SCREEN_H

#include "util/ps_geometry.h"
#include "ps_blueprint_list.h"

struct ps_scenario;
struct ps_blueprint;
struct ps_grid;
struct ps_region;

#define PS_SCREEN_FEATURE_HOME      0x01
#define PS_SCREEN_FEATURE_TREASURE  0x02
#define PS_SCREEN_FEATURE_CHALLENGE 0x04

// Describes the edges of a screen; does it connect to the neighboring screen?
#define PS_DOOR_UNSET    0
#define PS_DOOR_CLOSED   1
#define PS_DOOR_OPEN     2

struct ps_screen {
  uint8_t x,y; // My coordinates in the owning world. So we can pass around pointers to screen.
  struct ps_blueprint *blueprint;
  uint8_t xform;
  uint8_t features;
  uint8_t doorn,doors,doore,doorw;
  int direction_home;
  struct ps_grid *grid;
  struct ps_region *region;//WEAK
  int visited; // Very transient; scgen functions may use it for recursive ops.
};

void ps_screen_cleanup(struct ps_screen *screen);

int ps_screen_set_blueprint(struct ps_screen *screen,struct ps_blueprint *blueprint);

int ps_screen_door_for_direction(const struct ps_screen *screen,int direction);
struct ps_screen *ps_screen_neighbor_for_direction(struct ps_screen *screen,int direction,int worldw);

int ps_screen_count_doors(const struct ps_screen *screen);

/* If this screen has only one door, return the screen on the other side of it.
 */
struct ps_screen *ps_screen_get_single_neighbor(struct ps_screen *screen,int worldw);

/* If this screen has exactly two doors, return the direction of the one that is not homeward.
 */
int ps_screen_get_single_awayward_direction(const struct ps_screen *screen);

/* Reinitialize (screen->grid) and populate its inner portion from (screen->blueprint).
 * We only set 'physics' in the grid cells.
 * This leaves the two outer rings zeroed.
 */
int ps_screen_build_inner_grid(struct ps_screen *screen);

/* Fill in 'physics' only for the outer two rings of the grid.
 * You provide the neighbor grids if we have a door in that direction.
 * We don't look at the screen's door values; NULL neighbor grid means no wall.
 */
int ps_screen_populate_grid_margins(struct ps_screen *screen,struct ps_grid *north,struct ps_grid *south,struct ps_grid *west,struct ps_grid *east);

#endif
