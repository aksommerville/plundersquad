/* ps_restype_trdef.c
 * Treasure definition.
 * 
 * SERIAL FORMAT
 *   0000   2 Thumbnail tile ID.
 *   0002   2 Full image ID.
 *   0004   1 Name length.
 *   0005 ... Name.
 *   ....
 */

#include "res/ps_res_internal.h"

#define OBJ ((struct ps_res_trdef*)obj)

/* Delete.
 */

static void ps_TRDEF_del(void *obj) {
  if (!obj) return;
  free(obj);
}

/* Encode.
 */
 
int ps_res_trdef_encode(void *dstpp,const struct ps_res_trdef *trdef) {
  if (!dstpp||!trdef) return -1;
  int dstc=5+trdef->namec;
  uint8_t *dst=malloc(dstc);
  if (!dst) return -1;

  dst[0]=trdef->thumbnail_tileid>>8;
  dst[1]=trdef->thumbnail_tileid;
  dst[2]=trdef->full_imageid>>8;
  dst[3]=trdef->full_imageid;
  dst[4]=trdef->namec;
  memcpy(dst+5,trdef->name,trdef->namec);

  *(void**)dstpp=dst;
  return dstc;
}

/* Decode into object.
 */

static int ps_res_trdef_decode(struct ps_res_trdef *trdef,const uint8_t *src,int srcc) {
  if (srcc<5) return -1;

  trdef->thumbnail_tileid=(src[0]<<8)|src[1];
  trdef->full_imageid=(src[2]<<8)|src[3];
  trdef->image=0;
  
  trdef->namec=src[4];
  if (trdef->namec>PS_TRDEF_NAME_LIMIT) {
    trdef->namec=0;
    return -1;
  }
  int srcp=5;
  if (srcp>srcc-trdef->namec) {
    trdef->namec=0;
    return -1;
  }
  memcpy(trdef->name,src+srcp,trdef->namec);
  srcp+=trdef->namec;
  
  return 0;
}

/* Decode, main entry point.
 */

static int ps_TRDEF_decode(void *dstpp,const void *src,int srcc,int id,const char *refpath) {
  struct ps_res_trdef *trdef=calloc(1,sizeof(struct ps_res_trdef));
  if (!trdef) return -1;
  if (ps_res_trdef_decode(trdef,src,srcc)<0) {
    ps_TRDEF_del(trdef);
    return -1;
  }
  *(void**)dstpp=trdef;
  return 0;
}

/* Link.
 */

static int ps_TRDEF_link(void *obj) {

  if (!(OBJ->image=ps_res_get(PS_RESTYPE_IMAGE,OBJ->full_imageid))) {
    ps_log(RES,ERROR,"image:%d not found, required for trdef",OBJ->full_imageid);
    return 0;
  }

  return 0;
}

/* Setup.
 */

int ps_restype_setup_TRDEF(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_TRDEF;
  type->name="trdef";
  type->rid_limit=INT_MAX;

  type->del=ps_TRDEF_del;
  type->decode=ps_TRDEF_decode;
  type->link=ps_TRDEF_link;

  return 0;
}
