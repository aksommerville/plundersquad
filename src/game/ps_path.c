#include "ps.h"
#include "ps_path.h"

void ps_path_cleanup(struct ps_path *path) {
  if (path->v) free(path->v);
}

int ps_path_require(struct ps_path *path) {
  if (!path) return -1;
  if (path->c<path->a) return 0;
  int na=path->a+32;
  if (na>INT_MAX/sizeof(struct ps_path_entry)) return -1;
  void *nv=realloc(path->v,sizeof(struct ps_path_entry)*na);
  if (!nv) return -1;
  path->v=nv;
  path->a=na;
  return 0;
}

int ps_path_has(const struct ps_path *path,int x,int y) {
  if (!path) return 0;
  const struct ps_path_entry *entry=path->v;
  int i=path->c; for (;i-->0;entry++) {
    if ((entry->x==x)&&(entry->y==y)) return 1;
  }
  return 0;
}

int ps_path_add(struct ps_path *path,int x,int y) {
  if (ps_path_require(path)<0) return -1;
  struct ps_path_entry *entry=path->v+path->c++;
  entry->x=x;
  entry->y=y;
  return 0;
}
