#include "res/ps_res_internal.h"
#include "video/ps_image_decode.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define OBJ ((struct ps_res_TILESHEET*)obj)

/* Delete resource.
 */

static void ps_restype_TILESHEET_del(void *obj) {
  akgl_texture_del(OBJ->texture);
  if (OBJ->pixels) free(OBJ->pixels);
}

/* Decode resource.
 */

static int ps_restype_TILESHEET_warned_about_stub_load=0;

static int ps_restype_TILESHEET_decode(void *objpp,const void *src,int srcc,int rid,const char *refpath) {
  void *obj=calloc(1,sizeof(struct ps_res_TILESHEET));
  if (!obj) return -1;
  
  if (ps_video_is_init()) {
    if (!(OBJ->texture=ps_image_decode(src,srcc,refpath))) {
      ps_restype_TILESHEET_del(obj);
      free(obj);
      return -1;
    }
    
  } else {
    if (!ps_restype_TILESHEET_warned_about_stub_load) {
      ps_restype_TILESHEET_warned_about_stub_load=1;
      ps_log(RES,DEBUG,"Stubbing image decode because video provider is not initialized.");
    }
    if (ps_image_decode_pixels(
      &OBJ->pixels,&OBJ->fmt,&OBJ->w,&OBJ->h,src,srcc
    )<0) {
      ps_restype_TILESHEET_del(obj);
      free(obj);
      return -1;
    }
  }
  
  *(void**)objpp=obj;
  return 0;
}

/* Link resource.
 */

static int ps_restype_TILESHEET_link(void *obj) {
  return 0;
}

/* Setup.
 */

int ps_restype_setup_TILESHEET(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_TILESHEET;
  type->name="tilesheet";
  type->rid_limit=255;

  type->del=ps_restype_TILESHEET_del;
  type->decode=ps_restype_TILESHEET_decode;
  type->link=ps_restype_TILESHEET_link;

  return 0;
}
