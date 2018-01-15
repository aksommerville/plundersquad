/* ps_path.h
 * Utility for recording a path through two-dimensional space.
 * For screens in the scenario, or cells in the grid.
 * This can also be used as a loose set of coordinates.
 */

#ifndef PS_PATH_H
#define PS_PATH_H

struct ps_path {
  struct ps_path_entry {
    int x,y;
  } *v;
  int c,a;
};

void ps_path_cleanup(struct ps_path *path);
int ps_path_require(struct ps_path *path);
int ps_path_has(const struct ps_path *path,int x,int y);
int ps_path_add(struct ps_path *path,int x,int y);

#endif
