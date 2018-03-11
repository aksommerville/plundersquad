#include "res/ps_res_internal.h"
#include "video/ps_image_decode.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define OBJ ((struct ps_res_IMAGE*)obj)

/* Delete resource.
 */

static void ps_restype_IMAGE_del(void *obj) {
  akgl_texture_del(OBJ->texture);
  if (OBJ->pixels) free(OBJ->pixels);
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
    if (akgl_texture_set_filter(OBJ->texture,1)<0) {
      // Whatever.
    }
  } else {
    if (ps_image_decode_pixels(
      &OBJ->pixels,&OBJ->fmt,&OBJ->w,&OBJ->h,src,srcc
    )<0) {
      ps_restype_IMAGE_del(obj);
      free(obj);
      return -1;
    }
  }
  
  *(void**)objpp=obj;
  return 0;
}

/* Link resource.
 */

static int ps_restype_IMAGE_link(void *obj) {
  return 0;
}

/* Encode resource.
 */
 
static int ps_restype_IMAGE_encode(void *dst,int dsta,const void *obj) {
  if (!OBJ->pixels) {
    ps_log(RES,ERROR,"Image loaded with video support -- Can't encode because raw data was dropped.");
    return -1;
  }
  return ps_image_encode(dst,dsta,OBJ->pixels,OBJ->w,OBJ->h,OBJ->fmt);
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
  type->encode=ps_restype_IMAGE_encode;

  return 0;
}
