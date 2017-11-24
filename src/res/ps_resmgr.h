/* ps_resmgr.h
 * Resource Manager is responsible for the game's static assets.
 */

#ifndef PS_RESMGR_H
#define PS_RESMGR_H

struct ps_restype;

int ps_resmgr_init(const char *path,int edit);
void ps_resmgr_quit();

/* Drop all resources and reread from disk.
 */
int ps_resmgr_reload();

int ps_resmgr_clear();

#define PS_RESTYPE_TILESHEET      0
#define PS_RESTYPE_IMAGE          1
#define PS_RESTYPE_BLUEPRINT      2
#define PS_RESTYPE_SPRDEF         3
#define PS_RESTYPE_REGION         4
#define PS_RESTYPE_PLRDEF         5
#define PS_RESTYPE_COUNT          6

struct ps_restype *ps_resmgr_get_type_by_id(int tid);
#define PS_RESTYPE(tag) ps_resmgr_get_type_by_id(PS_RESTYPE_##tag)
struct ps_restype *ps_resmgr_get_type_by_name(const char *name,int namec);

void *ps_res_get(int tid,int rid);

// Reverse lookup.
int ps_res_get_id_by_obj(int tid,const void *obj);

#endif
