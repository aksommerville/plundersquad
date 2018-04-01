/* ps_widget_edittrdef.c
 * Editor for trdef (treasure) resources.
 * Children:
 *   [0] menubar
 *   [1] name button
 *   [2] tile button
 *   [3] image button
 *   [4] name field
 *   [5] tile field
 *   [6] image field
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "res/ps_restype.h"
#include "res/ps_resmgr.h"
#include "os/ps_fs.h"
#include "util/ps_text.h"

#define PS_EDITTRDEF_BUTTONS_RIGHT    50
#define PS_EDITTRDEF_FIELDS_LEFT      50

static int ps_edittrdef_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_edittrdef_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_edittrdef_cb_name(struct ps_widget *button,struct ps_widget *widget);
static int ps_edittrdef_cb_tile(struct ps_widget *button,struct ps_widget *widget);
static int ps_edittrdef_cb_image(struct ps_widget *button,struct ps_widget *widget);
static int ps_edittrdef_cb_field_changed(struct ps_widget *field,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_edittrdef {
  struct ps_widget hdr;
  struct ps_res_trdef *trdef;
  int resid;
  char *basename;
};

#define WIDGET ((struct ps_widget_edittrdef*)widget)

/* Delete.
 */

static void _ps_edittrdef_del(struct ps_widget *widget) {
  if (WIDGET->basename) free(WIDGET->basename);
}

/* Initialize.
 */

static int _ps_edittrdef_init(struct ps_widget *widget) {

  widget->bgrgba=0xd0e0f0ff;

  struct ps_widget *menubar=ps_widget_spawn(widget,&ps_widget_type_menubar);
  if (!menubar) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Back",4,ps_callback(ps_edittrdef_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Save",4,ps_callback(ps_edittrdef_cb_save,0,widget))) return -1;

  struct ps_widget *child;
  if (!(child=ps_widget_button_spawn(widget,0,"Name",4,ps_callback(ps_edittrdef_cb_name,0,widget)))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Tile",4,ps_callback(ps_edittrdef_cb_tile,0,widget)))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Image",5,ps_callback(ps_edittrdef_cb_image,0,widget)))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_edittrdef_cb_field_changed,0,widget))<0) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_edittrdef_cb_field_changed,0,widget))<0) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_edittrdef_cb_field_changed,0,widget))<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_edittrdef_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_edittrdef) return -1;
  if (widget->childc!=7) return -1;
  return 0;
}

static struct ps_widget *ps_edittrdef_get_menubar(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_edittrdef_get_namebutton(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_edittrdef_get_tilebutton(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_edittrdef_get_imagebutton(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_edittrdef_get_namefield(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_edittrdef_get_tilefield(const struct ps_widget *widget) { return widget->childv[5]; }
static struct ps_widget *ps_edittrdef_get_imagefield(const struct ps_widget *widget) { return widget->childv[6]; }

/* Pack.
 */

static int _ps_edittrdef_pack(struct ps_widget *widget) {
  if (ps_edittrdef_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_edittrdef_get_menubar(widget);
  struct ps_widget *namebutton=ps_edittrdef_get_namebutton(widget);
  struct ps_widget *tilebutton=ps_edittrdef_get_tilebutton(widget);
  struct ps_widget *imagebutton=ps_edittrdef_get_imagebutton(widget);
  struct ps_widget *namefield=ps_edittrdef_get_namefield(widget);
  struct ps_widget *tilefield=ps_edittrdef_get_tilefield(widget);
  struct ps_widget *imagefield=ps_edittrdef_get_imagefield(widget);
  int chw,chh,i;

  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  int y=menubar->h;
  #define ROW(tag) { \
    if (ps_widget_measure(&chw,&chh,tag##button,widget->w,widget->h)<0) return -1; \
    tag##button->x=PS_EDITTRDEF_BUTTONS_RIGHT-chw; \
    tag##button->y=y; \
    tag##button->w=chw; \
    tag##button->h=chh; \
    if (ps_widget_measure(&chw,&chh,tag##field,widget->w,widget->h)<0) return -1; \
    tag##field->x=PS_EDITTRDEF_FIELDS_LEFT; \
    tag##field->y=y; \
    tag##field->w=widget->w-tag##field->x; \
    tag##field->h=tag##button->h; \
    y+=tag##button->h; \
  }
  ROW(name)
  ROW(tile)
  ROW(image)
  #undef ROW
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_edittrdef={

  .name="edittrdef",
  .objlen=sizeof(struct ps_widget_edittrdef),

  .del=_ps_edittrdef_del,
  .init=_ps_edittrdef_init,

  .pack=_ps_edittrdef_pack,

};

/* Reread fields and populate full model.
 */

static int ps_edittrdef_populate_model_from_ui(struct ps_widget *widget) {
  const char *src;
  int srcc,n;

  if ((srcc=ps_widget_field_get_text(&src,ps_edittrdef_get_namefield(widget)))<0) return -1;
  if (srcc>PS_TRDEF_NAME_LIMIT) goto _fail_;
  memcpy(WIDGET->trdef->name,src,srcc);
  WIDGET->trdef->namec=srcc;

  if (ps_widget_field_get_integer(&n,ps_edittrdef_get_tilefield(widget))<0) goto _fail_;
  if ((n<0)||(n>0xffff)) goto _fail_;

  if (ps_widget_field_get_integer(&n,ps_edittrdef_get_imagefield(widget))<0) goto _fail_;
  if (n<0) goto _fail_;

  return 0;
 _fail_:
  ps_log(EDIT,ERROR,"trdef validation failed");
  return 0;
}

/* Set resource.
 */
 
int ps_widget_edittrdef_set_resource(struct ps_widget *widget,int id,struct ps_res_trdef *trdef,const char *name) {
  if (ps_edittrdef_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_edittrdef_get_menubar(widget);

  WIDGET->trdef=trdef;
  WIDGET->resid=id;

  if (ps_widget_menubar_set_title(menubar,name,-1)<0) return -1;
  if (ps_widget_field_set_text(ps_edittrdef_get_namefield(widget),WIDGET->trdef->name,WIDGET->trdef->namec)<0) return -1;

  char buf[32];
  int bufc=ps_hexuint_repr(buf,sizeof(buf),WIDGET->trdef->thumbnail_tileid);
  if (ps_widget_field_set_text(ps_edittrdef_get_tilefield(widget),buf,bufc)<0) return -1;

  bufc=ps_decsint_repr(buf,sizeof(buf),WIDGET->trdef->full_imageid);
  if (ps_widget_field_set_text(ps_edittrdef_get_imagefield(widget),buf,bufc)<0) return -1;
  
  return 0;
}

/* Dismiss self.
 */
 
static int ps_edittrdef_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Save.
 */

static int ps_edittrdef_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_TRDEF,WIDGET->resid,0);
  if ((pathc<0)||(pathc>=sizeof(path))) {
    ps_log(EDIT,ERROR,"Failed to compose path for trdef:%d",WIDGET->resid);
    return 0;
  }

  void *serial=0;
  int serialc=ps_res_trdef_encode(&serial,WIDGET->trdef);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to encode trdef.");
    return 0;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to write trdef:%d, %d bytes.",path,WIDGET->resid,serialc);
    free(serial);
    return 0;
  }

  ps_log(EDIT,INFO,"%s: Saved trdef:%d, %d bytes.",path,WIDGET->resid,serialc);
  free(serial);
  
  return 0;
}

/* Edit name.
 */

static int ps_edittrdef_cb_name_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);

  const char *src=0;
  int srcc=ps_widget_dialogue_get_text(&src,dialogue);
  if (srcc<0) return 0;
  if (srcc>PS_TRDEF_NAME_LIMIT) return 0;
  memcpy(WIDGET->trdef->name,src,srcc);
  WIDGET->trdef->namec=srcc;

  if (ps_widget_kill(dialogue)<0) return -1;

  if (ps_widget_field_set_text(ps_edittrdef_get_namefield(widget),WIDGET->trdef->name,WIDGET->trdef->namec)<0) return -1;

  return 0;
}

static int ps_edittrdef_cb_name(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_text(
    ps_widget_get_root(widget),
    "Presentation name:",-1,
    WIDGET->trdef->name,WIDGET->trdef->namec,
    ps_edittrdef_cb_name_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  
  return 0;
}

/* Edit tile.
 */

static int ps_edittrdef_cb_tile_ok(struct ps_widget *tileselect,struct ps_widget *widget) {
  WIDGET->trdef->thumbnail_tileid=ps_widget_tileselect_get_tileid(tileselect);
  
  struct ps_widget *tilefield=ps_edittrdef_get_tilefield(widget);
  char buf[32];
  int bufc=ps_hexuint_repr(buf,sizeof(buf),WIDGET->trdef->thumbnail_tileid);
  if (ps_widget_field_set_text(tilefield,buf,-1)<0) return -1;
  
  return 0;
}

static int ps_edittrdef_cb_tile(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *tileselect=ps_widget_spawn(root,&ps_widget_type_tileselect);
  if (!tileselect) return -1;
  if (ps_widget_tileselect_set_tileid(tileselect,WIDGET->trdef->thumbnail_tileid)<0) return -1;
  if (ps_widget_tileselect_set_callback(tileselect,ps_callback(ps_edittrdef_cb_tile_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Populate scrolllist with all image resources.
 */

static int ps_edittrdef_populate_image_list(struct ps_widget *widget,struct ps_widget *scrolllist) {
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_IMAGE);
  if (!restype) return -1;
  const struct ps_res *res=restype->resv;
  int i=0; for (;i<restype->resc;i++,res++) {

    char path[1024];
    int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_IMAGE,res->id,1);
    if ((pathc<0)||(pathc>=sizeof(path))) {
      pathc=ps_decsint_repr(path,sizeof(path),res->id);
    }
    const char *name=path;
    int namec=pathc;
    int j=pathc; while (j-->0) {
      if (path[j]=='/') {
        name=path+j+1;
        namec=pathc-j-1;
        break;
      }
    }

    if (!ps_widget_scrolllist_add_label(scrolllist,name,namec)) return -1;

    if (res->id==WIDGET->trdef->full_imageid) {
      if (ps_widget_scrolllist_set_selection(scrolllist,i)<0) return -1;
    }
    
  }
  return 0;
}

/* Edit image.
 */

static int ps_edittrdef_cb_image_ok(struct ps_widget *scrolllist,struct ps_widget *widget) {
  int resp=ps_widget_scrolllist_get_selection(scrolllist);

  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_IMAGE);
  if (!restype) return -1;
  if ((resp>=0)&&(resp<restype->resc)) {
    WIDGET->trdef->full_imageid=restype->resv[resp].id;
    char buf[32];
    int bufc=ps_decsint_repr(buf,sizeof(buf),WIDGET->trdef->full_imageid);
    if (ps_widget_field_set_text(ps_edittrdef_get_imagefield(widget),buf,bufc)<0) return -1;
  }
  
  if (ps_widget_plaindialogue_dismiss(scrolllist)<0) return -1;
  return 0;
}

static int ps_edittrdef_cb_image(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);

  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_plaindialogue);
  if (!dialogue) return -1;

  struct ps_widget *scrolllist=ps_widget_spawn(dialogue,&ps_widget_type_scrolllist);
  if (!scrolllist) return -1;
  if (ps_widget_scrolllist_enable_selection(scrolllist,ps_callback(ps_edittrdef_cb_image_ok,0,widget))<0) return -1;
  if (ps_edittrdef_populate_image_list(widget,scrolllist)<0) return -1;

  if (ps_widget_pack(root)<0) return -1;
  
  return 0;
}

/* Modify text.
 */

static int ps_edittrdef_cb_field_changed(struct ps_widget *field,struct ps_widget *widget) {
  if (ps_edittrdef_populate_model_from_ui(widget)<0) return -1;
  return 0;
}
