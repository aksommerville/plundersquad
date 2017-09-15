#include "res/ps_res_internal.h"
#include "video/ps_image_decode.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define OBJ ((struct ps_res_IMAGE*)obj)

/* Delete resource.
 */

static void ps_restype_IMAGE_del(void *obj) {
  akgl_texture_del(OBJ->texture);
}

/* Decode resource.
 */

static int ps_restype_IMAGE_decode(void *objpp,const void *src,int srcc,int rid,const char *refpath) {
  void *obj=calloc(1,sizeof(struct ps_res_IMAGE));
  if (!obj) return -1;

  if (ps_video_is_init()) {
    if (!(OBJ->texture=ps_image_decode(src,srcc,refpath))) {
      ps_restype_IMAGE_del(obj);
      free(obj);
      return -1;
    }
  } else {
    ps_log(RES,INFO,"Stubbing image decode because video provider is not initialized.");
  }
  
  *(void**)objpp=obj;
  return 0;
}

/* Link resource.
 */

static int ps_restype_IMAGE_link(void *obj) {
  return 0;
}

/* Setup.
 */

int ps_restype_setup_IMAGE(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_IMAGE;
  type->name="image";
  type->rid_limit=INT_MAX;

  type->del=ps_restype_IMAGE_del;
  type->decode=ps_restype_IMAGE_decode;
  type->link=ps_restype_IMAGE_link;

  return 0;
}
