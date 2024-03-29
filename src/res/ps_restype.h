/* ps_restype.h
 * Definition and storage for a resource type.
 */

#ifndef PS_RESTYPE_H
#define PS_RESTYPE_H

struct akgl_texture;

/* Generic resource type.
 *****************************************************************************/

struct ps_res {
  int id;
  int dirty;
  void *obj;
};

struct ps_restype {
  int tid;
  const char *name;
  int rid_limit;

  void (*del)(void *obj);
  int (*decode)(void *objpp,const void *src,int srcc,int id,const char *refpath);
  int (*link)(void *obj);
  int (*encode)(void *dst,int dsta,const void *obj);

  struct ps_res *resv;
  int resc,resa;
  int rescontigc;
  
};

void ps_restype_cleanup(struct ps_restype *type);

int ps_restype_setup_TILESHEET(struct ps_restype *type);
int ps_restype_setup_IMAGE(struct ps_restype *type);
int ps_restype_setup_BLUEPRINT(struct ps_restype *type);
int ps_restype_setup_SPRDEF(struct ps_restype *type);
int ps_restype_setup_REGION(struct ps_restype *type);
int ps_restype_setup_PLRDEF(struct ps_restype *type);
int ps_restype_setup_TRDEF(struct ps_restype *type);
int ps_restype_setup_IPCM(struct ps_restype *type);
int ps_restype_setup_SONG(struct ps_restype *type);

int ps_restype_clear(struct ps_restype *type);

/* Decode a serialized resource and add it to (type)'s store under the given ID.
 * (refpath) is optional, the original file name for logging.
 */
int ps_restype_decode(struct ps_restype *type,int rid,const void *src,int srcc,const char *refpath);

int ps_restype_res_search(const struct ps_restype *type,int id);
int ps_restype_index_by_object(const struct ps_restype *type,const void *obj);
int ps_restype_res_insert(struct ps_restype *type,int p,int id,void *obj_HANDOFF);

int ps_restype_link(struct ps_restype *restype);

/* Format of specific resource types (in-memory).
 *****************************************************************************/

struct ps_res_TILESHEET {
// If video is initialized before resources, only this is set:
  struct akgl_texture *texture;
// If video is not initialized, these are set:
  void *pixels;
  int w,h,fmt;
};

struct ps_res_IMAGE {
  // IMAGE works exactly like TILESHEET; we probably shouldn't have separated the two.
  struct akgl_texture *texture;
  void *pixels;
  int w,h,fmt;
};

// Defined in <scenario/ps_blueprint.h>
struct ps_blueprint;

// Defined in <game/ps_sprite.h>
struct ps_sprdef;

// Defined in <scenario/ps_region.h>
struct ps_region;

// Defined in <game/ps_plrdef.h>
struct ps_res_plrdef;

#define PS_TRDEF_NAME_LIMIT 64

struct ps_res_trdef {
  uint16_t thumbnail_tileid;
  int full_imageid;
  struct ps_res_IMAGE *image;
  int namec;
  char name[PS_TRDEF_NAME_LIMIT];
};

int ps_res_trdef_encode(void *dstpp,const struct ps_res_trdef *trdef);

// Both are opaque, and defined by <akau/akau.h>.
// In fallback mode, resource is 4-byte length followed by serial data.
struct akau_ipcm;
struct akau_song;

// You must declare this before initializing anything:
int ps_res_ipcm_use_fallback();
int ps_res_song_use_fallback();

#endif
