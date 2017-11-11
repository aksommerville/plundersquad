#include "ps_res_internal.h"

struct ps_resmgr ps_resmgr={0};

/* Init.
 */

int ps_resmgr_init(const char *path,int edit) {
  if (!path||!path[0]) return -1;
  
  if (ps_resmgr.init) return -1;
  memset(&ps_resmgr,0,sizeof(struct ps_resmgr));
  ps_resmgr.init=1;

  int pathc=0; while (path[pathc]) pathc++;
  if (!(ps_resmgr.rootpath=malloc(pathc+1))) {
    ps_resmgr_quit();
    return -1;
  }
  memcpy(ps_resmgr.rootpath,path,pathc+1);
  ps_resmgr.rootpathc=pathc;

  ps_resmgr.edit=edit?1:0;

  #define SETUPTYPE(tag) \
    if (ps_restype_setup_##tag(ps_resmgr.typev+PS_RESTYPE_##tag)<0) { \
      ps_resmgr_quit(); \
      return -1; \
    }
  SETUPTYPE(TILESHEET)
  SETUPTYPE(IMAGE)
  SETUPTYPE(SOUNDEFFECT)
  SETUPTYPE(SONG)
  SETUPTYPE(BLUEPRINT)
  SETUPTYPE(SPRDEF)
  SETUPTYPE(REGION)
  SETUPTYPE(PLRDEF)
  #undef SETUPTYPE

  if (ps_resmgr_reload()<0) {
    ps_resmgr_quit();
    return -1;
  }

  return 0;
}

/* Quit.
 */

void ps_resmgr_quit() {
  int i;

  for (i=0;i<PS_RESTYPE_COUNT;i++) ps_restype_cleanup(ps_resmgr.typev+i);
  
  if (ps_resmgr.rootpath) free(ps_resmgr.rootpath);
  
  memset(&ps_resmgr,0,sizeof(struct ps_resmgr));
}

/* Drop all resources.
 */
 
int ps_resmgr_clear() {
  if (!ps_resmgr.init) return -1;
  int i=PS_RESTYPE_COUNT;
  struct ps_restype *type=ps_resmgr.typev;
  for (;i-->0;type++) {
    if (ps_restype_clear(type)<0) return -1;
  }
  return 0;
}

/* Get type by ID or name.
 */
 
struct ps_restype *ps_resmgr_get_type_by_id(int tid) {
  if (!ps_resmgr.init) return 0;
  if (tid<0) return 0;
  if (tid>=PS_RESTYPE_COUNT) return 0;
  return ps_resmgr.typev+tid;
}
 
struct ps_restype *ps_resmgr_get_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!namec) return 0;
  struct ps_restype *type=ps_resmgr.typev;
  int i=PS_RESTYPE_COUNT; for (;i-->0;type++) {
    if (!type->name) continue;
    if (memcmp(type->name,name,namec)) continue;
    if (type->name[namec]) continue;
    return type;
  }
  return 0;
}

/* Get resource by type and ID.
 */
 
void *ps_res_get(int tid,int rid) {
  if ((tid<0)||(tid>=PS_RESTYPE_COUNT)) return 0;
  struct ps_restype *type=ps_resmgr.typev+tid;
  int p=ps_restype_res_search(type,rid);
  if (p<0) return 0;
  return type->resv[p].obj;
}

/* Reverse lookup by type and object.
 */

int ps_res_get_id_by_obj(int tid,const void *obj) {
  if ((tid<0)||(tid>=PS_RESTYPE_COUNT)) return -1;
  const struct ps_restype *type=ps_resmgr.typev+tid;
  int i=type->resc;
  const struct ps_res *res=type->resv;
  for (;i-->0;res++) {
    if (res->obj==obj) return res->id;
  }
  return -1;
}
