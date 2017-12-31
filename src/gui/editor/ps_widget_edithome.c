/* ps_widget_edithome.c
 * Main menu for editor.
 * Children:
 *   [0] menu bar
 *   [1] type list
 *   [2] resource list
 */

#include "ps.h"
#include "../ps_widget.h"
#include "../corewidgets/ps_corewidgets.h"
#include "ps_editor.h"
#include "akau/akau.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "input/ps_input.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_region.h"
#include "game/ps_sprite.h"

static int ps_edithome_rebuild_resource_list(struct ps_widget *widget,int typeindex);
static int ps_edithome_open_resource(struct ps_widget *widget,int typeindex,int resindex);
static int ps_edithome_cb_quit(struct ps_widget *button,struct ps_widget *widget);
static int ps_edithome_cb_new(struct ps_widget *button,struct ps_widget *widget);
static int ps_edithome_cb_delete(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_edithome {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_edithome*)widget)

/* Select type.
 */
 
static int ps_edithome_cb_select_type(struct ps_widget *typelist,struct ps_widget *widget) {
  int selection=ps_widget_scrolllist_get_selection(typelist);
  if (ps_edithome_rebuild_resource_list(widget,selection)<0) return -1;
  return 0;
}

/* Select resource.
 */
 
static int ps_edithome_cb_select_resource(struct ps_widget *reslist,struct ps_widget *widget) {
  int resselection=ps_widget_scrolllist_get_selection(reslist);
  if (resselection<0) return 0; // Cleared selection.
  if (widget->childc!=3) return -1; // Inconsistent widget.
  struct ps_widget *typelist=widget->childv[1];
  int typeselection=ps_widget_scrolllist_get_selection(typelist);
  if (typeselection<0) return 0; // No type selected? That's weird... abort quietly.
  return ps_edithome_open_resource(widget,typeselection,resselection);
}

/* Delete.
 */

static void _ps_edithome_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_edithome_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1;
  { struct ps_widget *button;
    if (!(button=ps_widget_menubar_add_button(child,"Quit",4,ps_callback(ps_edithome_cb_quit,0,widget)))) return -1;
    if (!(button=ps_widget_menubar_add_button(child,"New",3,ps_callback(ps_edithome_cb_new,0,widget)))) return -1;
    if (!(button=ps_widget_menubar_add_button(child,"Delete",6,ps_callback(ps_edithome_cb_delete,0,widget)))) return -1;
  }

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1;
  if (ps_widget_scrolllist_enable_selection(child,ps_callback(ps_edithome_cb_select_type,0,widget))<0) return -1;
  child->bgrgba=0xb0b0b0ff;
  if (!ps_widget_scrolllist_add_label(child,"Song",4)) return -1;
  if (!ps_widget_scrolllist_add_label(child,"Sound Effect",-1)) return -1;
  if (!ps_widget_scrolllist_add_label(child,"Blueprint",-1)) return -1;
  if (!ps_widget_scrolllist_add_label(child,"Sprite",-1)) return -1;
  if (!ps_widget_scrolllist_add_label(child,"Hero",-1)) return -1;
  if (!ps_widget_scrolllist_add_label(child,"Region",-1)) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1;
  if (ps_widget_scrolllist_enable_selection(child,ps_callback(ps_edithome_cb_select_resource,0,widget))<0) return -1;
  child->bgrgba=0xd0d0d0ff;
  
  return 0;
}

/* Pack.
 */

static int _ps_edithome_pack(struct ps_widget *widget) {
  if (widget->childc!=3) return -1;
  int chw,chh;
  struct ps_widget *menubar=widget->childv[0];
  struct ps_widget *typelist=widget->childv[1];
  struct ps_widget *reslist=widget->childv[2];

  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;
  if (ps_widget_pack(menubar)<0) return -1;

  if (ps_widget_measure(&chw,&chh,typelist,widget->w,widget->h-menubar->h)<0) return -1;
  typelist->x=0;
  typelist->y=menubar->h;
  typelist->w=chw;
  typelist->h=widget->h-menubar->h;
  if (ps_widget_pack(typelist)<0) return -1;

  reslist->x=typelist->w;
  reslist->y=menubar->h;
  reslist->w=widget->w-typelist->w;
  reslist->h=widget->h-menubar->h;
  if (ps_widget_pack(reslist)<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_edithome={

  .name="edithome",
  .objlen=sizeof(struct ps_widget_edithome),

  .del=_ps_edithome_del,
  .init=_ps_edithome_init,

  .pack=_ps_edithome_pack,

};

/* Compose label for resource.
 */

static struct ps_widget *ps_edithome_spawn_resource_label(struct ps_widget *widget,struct ps_widget *reslist,int resid,const char *path,int pathc) {
  int i;
  for (i=pathc;i-->0;) if (path[i]=='/') {
    path+=i+1;
    pathc-=i+1;
    break;
  }
  struct ps_widget *label=ps_widget_spawn(reslist,&ps_widget_type_label);
  if (!label) return 0;

  /* Use basename if available, otherwise ID.
   * Don't use both because the basename always begins with the ID.
   */
  if (pathc>0) {
    if (ps_widget_label_set_text(label,path,pathc)<0) return 0;
  } else {
    if (ps_widget_label_set_textf(label,"%d",resid)<0) return 0;
  }
  
  return label;
}

/* List akau resources.
 */

static int ps_edithome_list_songs(struct ps_widget *widget) {
  struct ps_widget *reslist=widget->childv[2];
  struct akau_store *store=akau_get_store();
  char path[1024];
  int resc=akau_store_count_song(store);
  int resp=0; for (;resp<resc;resp++) {
    int resid=akau_store_get_song_id_by_index(store,resp);
    int pathc=akau_store_get_resource_path(path,sizeof(path),store,"song",resid);
    if ((pathc<0)||(pathc>=sizeof(path))) {
      pathc=snprintf(path,sizeof(path),"#%d",resid);
    }
    struct ps_widget *label=ps_edithome_spawn_resource_label(widget,reslist,resid,path,pathc);
    if (!label) return -1;
  }
  return 0;
}

static int ps_edithome_list_soundeffects(struct ps_widget *widget) {
  struct ps_widget *reslist=widget->childv[2];
  struct akau_store *store=akau_get_store();
  char path[1024];
  int resc=akau_store_count_ipcm(store);
  int resp=0; for (;resp<resc;resp++) {
    int resid=akau_store_get_ipcm_id_by_index(store,resp);
    int pathc=akau_store_get_resource_path(path,sizeof(path),store,"ipcm",resid);
    if ((pathc<0)||(pathc>=sizeof(path))) pathc=0;
    struct ps_widget *label=ps_edithome_spawn_resource_label(widget,reslist,resid,path,pathc);
    if (!label) return -1;
  }
  return 0;
}

/* List ps resources.
 */

static int ps_edithome_list_resources(struct ps_widget *widget,int tid) {
  struct ps_widget *reslist=widget->childv[2];
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(tid);
  if (!restype) return -1;
  char path[1024];
  const struct ps_res *res=restype->resv;
  int resp=0; for (;resp<restype->resc;resp++,res++) {
    int pathc=ps_res_get_path_for_resource(path,sizeof(path),tid,res->id,1);
    if ((pathc<0)||(pathc>sizeof(path))) pathc=0;
    struct ps_widget *label=ps_edithome_spawn_resource_label(widget,reslist,res->id,path,pathc);
    if (!label) return -1;
  }
  return 0;
}

/* Rebuild resource list.
 */
 
static int ps_edithome_rebuild_resource_list(struct ps_widget *widget,int typeindex) {
  if (!widget||(widget->type!=&ps_widget_type_edithome)||(widget->childc!=3)) return -1;
  struct ps_widget *reslist=widget->childv[2];
  
  if (ps_widget_remove_all_children(reslist)<0) return -1;
  if (ps_widget_scrolllist_set_selection(reslist,-1)<0) return -1;
  if (ps_widget_scrolllist_set_scroll_position(reslist,0)<0) return -1;

  switch (typeindex) {
    case 0: if (ps_edithome_list_songs(widget)<0) return -1; break;
    case 1: if (ps_edithome_list_soundeffects(widget)<0) return -1; break;
    case 2: if (ps_edithome_list_resources(widget,PS_RESTYPE_BLUEPRINT)<0) return -1; break;
    case 3: if (ps_edithome_list_resources(widget,PS_RESTYPE_SPRDEF)<0) return -1; break;
    case 4: if (ps_edithome_list_resources(widget,PS_RESTYPE_PLRDEF)<0) return -1; break;
    case 5: if (ps_edithome_list_resources(widget,PS_RESTYPE_REGION)<0) return -1; break;
  }

  if (ps_widget_pack(reslist)<0) return -1;

  return 0;
}

/* Open song.
 */

static int ps_edithome_open_song(struct ps_widget *widget,int index,const char *resname) {

  struct akau_store *store=akau_get_store();
  int rid=akau_store_get_song_id_by_index(store,index);
  if (rid<0) return -1;
  struct akau_song *song=akau_store_get_song(store,rid);
  if (!song) return -1;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,&ps_widget_type_editsong);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,rid,song,resname)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Open sound effect.
 */

static int ps_edithome_open_soundeffect(struct ps_widget *widget,int index,const char *resname) {

  struct akau_store *store=akau_get_store();
  int rid=akau_store_get_ipcm_id_by_index(store,index);
  if (rid<0) return -1;
  struct akau_ipcm *ipcm=akau_store_get_ipcm(store,rid);
  if (!ipcm) return -1;
  
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,&ps_widget_type_editsoundeffect);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,rid,ipcm,resname)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
  
}

/* Open PS resource.
 */

static int ps_edithome_open_ps_resource(struct ps_widget *widget,int tid,int index,const struct ps_widget_type *editortype,const char *resname) {

  const struct ps_restype *restype=ps_resmgr_get_type_by_id(tid);
  if (!restype) return -1;
  if (index>=restype->resc) return -1;
  const struct ps_res *res=restype->resv+index;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,editortype);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,res->id,res->obj,resname)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Open new editor for one resource.
 */
 
static int ps_edithome_open_resource(struct ps_widget *widget,int typeindex,int resindex) {
  if (!widget||(widget->type!=&ps_widget_type_edithome)||(widget->childc!=3)) return -1;
  if (resindex<0) return -1;

  struct ps_widget *reslist=widget->childv[2];
  const char *resname=ps_widget_scrolllist_get_text(reslist);

  switch (typeindex) {
    case 0: return ps_edithome_open_song(widget,resindex,resname);
    case 1: return ps_edithome_open_soundeffect(widget,resindex,resname);
    case 2: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_BLUEPRINT,resindex,&ps_widget_type_editblueprint,resname);
    case 3: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_SPRDEF,resindex,&ps_widget_type_editsprdef,resname);
    case 4: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_PLRDEF,resindex,&ps_widget_type_editplrdef,resname);
    case 5: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_REGION,resindex,&ps_widget_type_editregion,resname);
  }

  return -1;
}

/* Dispatch resource load to specific editor types.
 */

static int ps_edithome_cb_dismiss(struct ps_widget *button,struct ps_widget *dialogue) {
  return ps_widget_kill(dialogue);
}

int ps_widget_editor_set_resource(struct ps_widget *widget,int id,void *obj,const char *name) {
  if (!widget||(id<0)||!obj) return -1;

  if (widget->type==&ps_widget_type_editsoundeffect) {
    return ps_widget_editsoundeffect_set_resource(widget,id,obj,name);
  }

  if (widget->type==&ps_widget_type_editsong) {
    return ps_widget_editsong_set_resource(widget,id,obj,name);
  }

  if (widget->type==&ps_widget_type_editblueprint) {
    return ps_widget_editblueprint_set_resource(widget,id,obj,name);
  }

  if (widget->type==&ps_widget_type_editsprdef) {
    return ps_widget_editsprdef_set_resource(widget,id,obj,name);
  }

  if (widget->type==&ps_widget_type_editplrdef) {
    return ps_widget_editplrdef_set_resource(widget,id,obj,name);
  }

  if (widget->type==&ps_widget_type_editregion) {
    return ps_widget_editregion_set_resource(widget,id,obj,name);
  }

  return -1;
}

/* Create new song.
 */

static int ps_edithome_create_song(struct ps_widget *widget) {
  struct akau_store *store=akau_get_store();
  if (!store) return -1;

  int songid,songindex;
  if (akau_store_get_unused_song_id(&songid,&songindex,store)<0) return -1;
  
  struct akau_song *song=akau_song_new();
  if (!song) return -1;
  if (akau_store_add_song(store,song,songid)<0) {
    akau_song_del(song);
    return -1;
  }

  akau_song_del(song);
  return 0;
}

/* Create new sound effect.
 */

static int ps_edithome_create_sound_effect(struct ps_widget *widget) {
  struct akau_store *store=akau_get_store();
  if (!store) return -1;

  int ipcmid,ipcmindex;
  if (akau_store_get_unused_ipcm_id(&ipcmid,&ipcmindex,store)<0) return -1;
  
  struct akau_ipcm *ipcm=akau_ipcm_new(11025);
  if (!ipcm) return -1;
  if (akau_store_add_ipcm(store,ipcm,ipcmid)<0) {
    akau_ipcm_del(ipcm);
    return -1;
  }

  akau_ipcm_del(ipcm);
  return 0;
}

/* Create new blueprint.
 */

static int ps_edithome_create_blueprint(struct ps_widget *widget) {
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_BLUEPRINT);
  if (!restype) return -1;
  int resp=restype->rescontigc;
  int resid=resp;

  struct ps_blueprint *blueprint=ps_blueprint_new();
  if (!blueprint) return -1;

  if (ps_restype_res_insert(restype,resp,resid,blueprint)<0) return -1;
  return 0;
}

/* Create new sprdef.
 */

static int ps_edithome_create_sprdef(struct ps_widget *widget) {
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_SPRDEF);
  if (!restype) return -1;
  int resp=restype->rescontigc;
  int resid=resp;

  struct ps_sprdef *sprdef=ps_sprdef_new(0);
  if (!sprdef) return -1;
  sprdef->type=&ps_sprtype_dummy;
  sprdef->tileid=0x0400;

  if (ps_restype_res_insert(restype,resp,resid,sprdef)<0) return -1;
  return 0;
}

/* Create new region.
 */

static int ps_edithome_create_region(struct ps_widget *widget) {
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_REGION);
  if (!restype) return -1;
  int resp=restype->rescontigc;
  int resid=resp;

  struct ps_region *region=ps_region_new(0);
  if (!region) return -1;

  if (ps_restype_res_insert(restype,resp,resid,region)<0) return -1;
  return 0;
}

/* Menubar callbacks.
 */
 
static int ps_edithome_cb_quit(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

static int ps_edithome_cb_new(struct ps_widget *button,struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_edithome)||(widget->childc!=3)) return -1;
  struct ps_widget *typelist=widget->childv[1];

  int typeindex=ps_widget_scrolllist_get_selection(typelist);
  switch (typeindex) {
    case 0: if (ps_edithome_create_song(widget)<0) return -1; break;
    case 1: if (ps_edithome_create_sound_effect(widget)<0) return -1; break;
    case 2: if (ps_edithome_create_blueprint(widget)<0) return -1; break;
    case 3: if (ps_edithome_create_sprdef(widget)<0) return -1; break;
    case 4: return 0; // plrdef: We've already got the complete set, by the time I wrote this.
    case 5: if (ps_edithome_create_region(widget)<0) return -1; break;
  }

  if (ps_edithome_rebuild_resource_list(widget,typeindex)<0) return -1;
  return 0;
}

static int ps_edithome_cb_delete(struct ps_widget *button,struct ps_widget *widget) {
  ps_log(GUI,DEBUG,"%s TODO",__func__); // Delete resource is not critical, it can wait
  return 0;
}
