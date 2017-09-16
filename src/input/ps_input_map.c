#include "ps.h"
#include "ps_input_map.h"

/* Object lifecycle.
 */

struct ps_input_map *ps_input_map_new() {
  struct ps_input_map *map=calloc(1,sizeof(struct ps_input_map));
  if (!map) return 0;

  map->refc=1;

  return map;
}

void ps_input_map_del(struct ps_input_map *map) {
  if (!map) return;
  if (map->refc-->1) return;

  if (map->fldv) free(map->fldv);

  free(map);
}

int ps_input_map_ref(struct ps_input_map *map) {
  if (!map) return -1;
  if (map->refc<1) return -1;
  if (map->refc==INT_MAX) return -1;
  map->refc++;
  return 0;
}

/* Set button state.
 */

static inline int ps_input_map_fld_set_button(struct ps_input_map_fld *fld,int srcv) {
  if (srcv==fld->srcv) return 0;
  fld->srcv=srcv;
  int dstv=((srcv>=fld->srclo)&&(srcv<=fld->srchi))?1:0;
  if (dstv==fld->dstv) return 0;
  fld->dstv=dstv;
  return 1;
}

int ps_input_map_set_button(
  struct ps_input_map *map,
  int srcbtnid,int srcv,
  void *userdata,
  int (*cb)(int plrid,int btnid,int value,void *userdata)
) {
  int p=ps_input_map_search(map,srcbtnid);
  if (p<0) return 0;
  while (p&&(map->fldv[p-1].srcbtnid==srcbtnid)) p--;
  struct ps_input_map_fld *fld=map->fldv+p;
  while ((p<map->fldc)&&(fld->srcbtnid==srcbtnid)) {
    if (ps_input_map_fld_set_button(fld,srcv)) {
      if (cb) {
        int err=cb(map->plrid,fld->dstbtnid,fld->dstv,userdata);
        if (err<0) return err;
      }
    }
  }
  return 0;
}

/* Search.
 */

int ps_input_map_search(const struct ps_input_map *map,int srcbtnid) {
  if (!map) return -1;
  int lo=0,hi=map->fldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<map->fldv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>map->fldv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
struct ps_input_map_fld *ps_input_map_insert(struct ps_input_map *map,int p,int srcbtnid) {
  if (!map) return 0;

  if ((p<0)||(p>map->fldc)) {
    p=ps_input_map_search(map,srcbtnid);
    if (p<0) p=-p-1;
  } else {
    if (p&&(srcbtnid<map->fldv[p-1].srcbtnid)) return 0;
    if ((p<map->fldc)&&(srcbtnid>map->fldv[p].srcbtnid)) return 0;
  }

  if (map->fldc>=map->flda) {
    int na=map->flda+16;
    if (na>INT_MAX/sizeof(struct ps_input_map_fld)) return 0;
    void *nv=realloc(map->fldv,sizeof(struct ps_input_map_fld)*na);
    if (!nv) return 0;
    map->fldv=nv;
    map->flda=na;
  }

  struct ps_input_map_fld *fld=map->fldv+p;
  memmove(fld+1,fld,sizeof(struct ps_input_map_fld)*(map->fldc-p));
  map->fldc++;
  memset(fld,0,sizeof(struct ps_input_map_fld));
  fld->srcbtnid=srcbtnid;

  return fld;
}
