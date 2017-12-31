/* ps_widget_editregion.c
 * Modal editor for region resources.
 *
 * Children:
 *  [0] menubar
 *  [1] monsterlist
 *  [2] shapelist
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "scenario/ps_region.h"
#include "res/ps_restype.h"
#include "res/ps_resmgr.h"
#include "os/ps_fs.h"
#include "util/ps_text.h"
#include "akau/akau.h"

#define PS_EDITREGION_SHAPE_COLOR_EVEN  0x80a0e0ff
#define PS_EDITREGION_SHAPE_COLOR_ODD   0xa0c0e0ff

static int ps_editregion_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_tilesheet(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_song(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_add_monster(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_add_shape(struct ps_widget *button,struct ps_widget *widget);
static int ps_editregion_cb_edit_monster(struct ps_widget *monsterlist,struct ps_widget *widget);
static int ps_editregion_cb_delete_shape(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editregion {
  struct ps_widget hdr;
  int resid;
  struct ps_region *region; // WEAK
};

#define WIDGET ((struct ps_widget_editregion*)widget)

/* Delete.
 */

static void _ps_editregion_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_editregion_init(struct ps_widget *widget) {

  widget->bgrgba=0x408050ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1; // menubar
  if (!ps_widget_menubar_add_button(child,"Back",4,ps_callback(ps_editregion_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Save",4,ps_callback(ps_editregion_cb_save,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Tilesheet",9,ps_callback(ps_editregion_cb_tilesheet,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Song",4,ps_callback(ps_editregion_cb_song,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"+Monster",8,ps_callback(ps_editregion_cb_add_monster,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"+Shape",6,ps_callback(ps_editregion_cb_add_shape,0,widget))) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1; // monsterlist
  if (ps_widget_scrolllist_enable_selection(child,ps_callback(ps_editregion_cb_edit_monster,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1; // shapelist

  return 0;
}

/* Structural helpers.
 */

static int ps_editregion_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editregion) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_editregion_get_menubar(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editregion_get_monsterlist(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editregion_get_shapelist(const struct ps_widget *widget) { return widget->childv[2]; }

/* Draw.
 */

static int _ps_editregion_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Pack.
 */

static int _ps_editregion_pack(struct ps_widget *widget) {
  if (ps_editregion_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editregion_get_menubar(widget);
  struct ps_widget *monsterlist=ps_editregion_get_monsterlist(widget);
  struct ps_widget *shapelist=ps_editregion_get_shapelist(widget);
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  monsterlist->x=0;
  monsterlist->y=menubar->h;
  monsterlist->w=widget->w>>1;
  monsterlist->h=widget->h-monsterlist->y;

  shapelist->x=monsterlist->w;
  shapelist->y=monsterlist->y;
  shapelist->w=widget->w-shapelist->x;
  shapelist->h=monsterlist->h;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editregion={

  .name="editregion",
  .objlen=sizeof(struct ps_widget_editregion),

  .del=_ps_editregion_del,
  .init=_ps_editregion_init,

  .draw=_ps_editregion_draw,
  .pack=_ps_editregion_pack,

};

/* Compose sprite name for random  monster list.
 */

static int ps_editregion_repr_sprdefid(char *dst,int dsta,int sprdefid) {
  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_SPRDEF,sprdefid,1);
  if ((pathc<1)||(pathc>sizeof(path))) return -1;
  int slashp=-1,i;
  for (i=pathc;i-->0;) {
    if (path[i]=='/') {
      slashp=i;
      break;
    }
  }
  return ps_strcpy(dst,dsta,path+slashp+1,pathc-slashp-1);
}

/* Populate UI from model.
 */

static int ps_editregion_populate_ui_from_model(struct ps_widget *widget) {
  struct ps_widget *monsterlist=ps_editregion_get_monsterlist(widget);
  struct ps_widget *shapelist=ps_editregion_get_shapelist(widget);
  int i;

  if (ps_widget_remove_all_children(monsterlist)<0) return -1;
  int monsterc=ps_region_count_monsters(WIDGET->region);
  for (i=0;i<monsterc;i++) {
    int sprdefid=ps_region_get_monster(WIDGET->region,i);
    char name[256];
    int namec=ps_editregion_repr_sprdefid(name,sizeof(name),sprdefid);
    if ((namec<0)||(namec>sizeof(name))) namec=ps_decsint_repr(name,sizeof(name),sprdefid);
    if (!ps_widget_scrolllist_add_label(monsterlist,name,namec)) return -1;
  }

  if (ps_widget_remove_all_children(shapelist)<0) return -1;
  const struct ps_region_shape *shape=WIDGET->region->shapev;
  for (i=WIDGET->region->shapec;i-->0;shape++) {
    struct ps_widget *regionshape=ps_widget_spawn(shapelist,&ps_widget_type_regionshape);
    if (!regionshape) return -1;
    regionshape->bgrgba=(i&1)?PS_EDITREGION_SHAPE_COLOR_ODD:PS_EDITREGION_SHAPE_COLOR_EVEN;
    if (ps_widget_regionshape_set_shape(regionshape,shape)<0) return -1;
    if (ps_widget_regionshape_set_tsid(regionshape,WIDGET->region->tsid)<0) return -1;
    if (ps_widget_regionshape_set_cb_delete(regionshape,ps_callback(ps_editregion_cb_delete_shape,0,widget))<0) return -1;
  }

  return 0;
}

/* Reallocate model object and install it globally.
 * As luck would have it, regions are not reference-counted and contain no pointers.
 * So it's really just a matter of allocate and memcpy.
 */

static int ps_editregion_reallocate_model_at_new_shapec(struct ps_widget *widget,int nshapec) {
  struct ps_region *nregion=ps_region_new(nshapec);
  if (!nregion) return -1;
  int smallershapec=(WIDGET->region->shapec<nshapec)?WIDGET->region->shapec:nshapec;
  memcpy(nregion,WIDGET->region,sizeof(struct ps_region)+sizeof(struct ps_region_shape)*smallershapec);
  nregion->shapec=nshapec;

  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_REGION);
  if (!restype) return -1;
  int resp=ps_restype_res_search(restype,WIDGET->resid);
  if (resp<0) {
    if (ps_restype_res_insert(restype,-resp-1,WIDGET->resid,nregion)<0) {
      ps_region_del(nregion);
      return -1;
    }
  } else {
    // This hurts my feelings.
    restype->resv[resp].obj=nregion;
  }
  
  ps_region_del(WIDGET->region);
  WIDGET->region=nregion;
  return 0;
}

/* Populate model from UI.
 * If it is necessary to re-allocate the region object, we replace the shared resource too.
 * This returns <0 for real errors, 0 if validation fails, or >0 on success.
 * Tile ID, song ID, and monster list stay in sync.
 * We only need to copy the shape list.
 */

static int ps_editregion_populate_model_from_ui(struct ps_widget *widget) {
  struct ps_widget *shapelist=ps_editregion_get_shapelist(widget);

  if (shapelist->childc!=WIDGET->region->shapec) {
    if (ps_editregion_reallocate_model_at_new_shapec(widget,shapelist->childc)<0) return -1;
  }

  int i=shapelist->childc; while (i-->0) {
    struct ps_widget *regionshape=shapelist->childv[i];
    if (ps_widget_regionshape_get_shape(WIDGET->region->shapev+i,regionshape)<0) return -1;
  }
  
  return 1;
}

/* Dismiss.
 */

static int ps_editregion_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Save.
 */

static int ps_editregion_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  int err=ps_editregion_populate_model_from_ui(widget);
  if (err<0) return -1;
  if (!err) {
    ps_log(EDIT,ERROR,"Failed to generate region object. Confirm sanity of data in editor.");
    return 0;
  }

  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_REGION,WIDGET->resid,0);
  if ((pathc<0)||(pathc>sizeof(path))) {
    ps_log(EDIT,ERROR,"Failed to acquire path for region:%d",WIDGET->resid);
    return 0;
  }

  void *serial=0;
  int serialc=ps_region_encode(&serial,WIDGET->region);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to serialize region.");
    return 0;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to write serialized region.",path);
    free(serial);
    return 0;
  }

  ps_log(EDIT,INFO,"%s: Wrote region:%d, %d bytes.",path,WIDGET->resid,serialc);
  free(serial);
  return 0;
}

/* Property edit callbacks.
 */

static int ps_editregion_cb_tilesheet_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int ntsid;
  if (ps_widget_dialogue_get_number(&ntsid,dialogue)<0) return 0;
  if (!ps_res_get(PS_RESTYPE_TILESHEET,ntsid)) return 0;
  WIDGET->region->tsid=ntsid;
  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editregion_cb_song_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int nsongid;
  if (ps_widget_dialogue_get_number(&nsongid,dialogue)<0) return 0;
  if (nsongid&&!akau_store_get_song(akau_get_store(),nsongid)) return 0;
  WIDGET->region->songid=nsongid;
  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editregion_cb_tilesheet(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Tilesheet ID:",-1,
    WIDGET->region->tsid,
    ps_editregion_cb_tilesheet_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  return 0;
}

static int ps_editregion_cb_song(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Song ID:",-1,
    WIDGET->region->songid,
    ps_editregion_cb_song_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  return 0;
}

/* Menu callbacks to add object to list.
 */

static int ps_editregion_cb_add_monster(struct ps_widget *button,struct ps_widget *widget) {

  if (ps_editregion_populate_model_from_ui(widget)<=0) {
    ps_log(EDIT,ERROR,"Inconsistent model. Please fix before adding monster.");
    return 0;
  }

  /* This is fallible in a few different ways. */
  int sprdefid=ps_region_add_any_valid_monster(WIDGET->region);
  if (sprdefid<0) {
    if (ps_region_count_monsters(WIDGET->region)==PS_REGION_MONSTER_LIMIT) {
      ps_log(EDIT,ERROR,"Region can not support any more random monsters.");
    } else {
      ps_log(EDIT,ERROR,"Failed to add monster.");
    }
    return 0;
  }

  if (ps_editregion_populate_ui_from_model(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  
  return 0;
}

static int ps_editregion_cb_add_shape(struct ps_widget *button,struct ps_widget *widget) {
  /* Unlike monsters, we don't keep shapes in sync with the model at all times. */

  struct ps_widget *shapelist=ps_editregion_get_shapelist(widget);
  struct ps_widget *regionshape=ps_widget_spawn(shapelist,&ps_widget_type_regionshape);
  if (!regionshape) return -1;
  regionshape->bgrgba=(shapelist->childc&1)?PS_EDITREGION_SHAPE_COLOR_EVEN:PS_EDITREGION_SHAPE_COLOR_ODD;
  if (ps_widget_regionshape_set_tsid(regionshape,WIDGET->region->tsid)<0) return -1;
  // No need to call ps_widget_regionshape_set_shape(). We don't have a shape for it, so accept the default.
  if (ps_widget_regionshape_set_cb_delete(regionshape,ps_callback(ps_editregion_cb_delete_shape,0,widget))<0) return -1;
  if (ps_widget_pack(shapelist)<0) return -1;
  
  return 0;
}

/* Delete shape.
 */

static int ps_editregion_cb_delete_shape(struct ps_widget *button,struct ps_widget *widget) {

  /* Walk up to the regionshape widget, and remove it from its parent. */
  while (button&&(button->type!=&ps_widget_type_regionshape)) button=button->parent;
  if (!button) return 0;
  if (ps_widget_remove_child(button->parent,button)<0) return -1;

  if (ps_widget_pack(ps_editregion_get_shapelist(widget))<0) return -1;

  return 0;
}

/* Edit random monster.
 */

static int ps_editregion_cb_edit_monster_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int oldsprdefid=ps_widget_dialogue_get_refnum1(dialogue);
  int newsprdefid;
  if (ps_widget_dialogue_get_number(&newsprdefid,dialogue)<0) return 0;
  if (newsprdefid&&!ps_res_get(PS_RESTYPE_SPRDEF,newsprdefid)) return 0; // Permit ID zero, to remove from list.

  int i=PS_REGION_MONSTER_LIMIT; while (i-->0) {
    if (WIDGET->region->monster_sprdefidv[i]==oldsprdefid) {
      WIDGET->region->monster_sprdefidv[i]=newsprdefid;
      if (ps_editregion_populate_model_from_ui(widget)<0) return -1;
      if (ps_editregion_populate_ui_from_model(widget)<0) return -1;
      if (ps_widget_pack(widget)<0) return -1;
      if (ps_widget_kill(dialogue)<0) return -1;
      return 0;
    }
  }

  return 0;
}

static int ps_editregion_cb_edit_monster(struct ps_widget *monsterlist,struct ps_widget *widget) {

  /* Gather sprdefid from the selected text.
   * Do not assume that index in this list means anything.
   */
  const char *name=ps_widget_scrolllist_get_text(monsterlist);
  if (!name) return 0;
  int sprdefid=0;
  while ((*name>='0')&&(*name<='9')) {
    sprdefid*=10;
    sprdefid+=(*name)-'0';
    name++;
  }

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Sprdef ID:",-1,
    sprdefid,
    ps_editregion_cb_edit_monster_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  if (ps_widget_dialogue_set_refnum1(dialogue,sprdefid)<0) return -1;
  
  return 0;
}

/* Set resource.
 */
 
int ps_widget_editregion_set_resource(struct ps_widget *widget,int id,struct ps_region *region,const char *name) {
  if (ps_editregion_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editregion_get_menubar(widget);
  WIDGET->resid=id;
  WIDGET->region=region;
  if (ps_widget_menubar_set_title(menubar,name,-1)<0) return -1;
  if (ps_editregion_populate_ui_from_model(widget)<0) return -1;
  return 0;
}
