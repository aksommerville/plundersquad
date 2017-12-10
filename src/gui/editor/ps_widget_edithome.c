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

static int ps_edithome_rebuild_resource_list(struct ps_widget *widget,int typeindex);
static int ps_edithome_open_resource(struct ps_widget *widget,int typeindex,int resindex);

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

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; //TODO menubar
  if (ps_widget_label_set_text(child,"MENU BAR (TODO)",-1)<0) return -1;
  child->bgrgba=0xff0000ff;

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
    if ((pathc<0)||(pathc>=sizeof(path))) pathc=0;
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

//XXX stub resource editors -- If we substitute ps_widget_type_dialogue, the user will see a friendly error message.
#define ps_widget_type_editsong ps_widget_type_dialogue
#define ps_widget_type_editsoundeffect ps_widget_type_dialogue
#define ps_widget_type_editblueprint ps_widget_type_dialogue
#define ps_widget_type_editsprdef ps_widget_type_dialogue
#define ps_widget_type_editplrdef ps_widget_type_dialogue
#define ps_widget_type_editregion ps_widget_type_dialogue

/* Open song.
 */

static int ps_edithome_open_song(struct ps_widget *widget,int index) {
  struct akau_store *store=akau_get_store();
  int rid=akau_store_get_song_id_by_index(store,index);
  if (rid<0) return -1;
  struct akau_song *song=akau_store_get_song(store,rid);
  if (!song) return -1;
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,&ps_widget_type_editsong);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,rid,song)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Open sound effect.
 */

static int ps_edithome_open_soundeffect(struct ps_widget *widget,int index) {
  struct akau_store *store=akau_get_store();
  int rid=akau_store_get_ipcm_id_by_index(store,index);
  if (rid<0) return -1;
  struct akau_ipcm *ipcm=akau_store_get_ipcm(store,rid);
  if (!ipcm) return -1;
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,&ps_widget_type_editsoundeffect);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,rid,ipcm)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
  
}

/* Open PS resource.
 */

static int ps_edithome_open_ps_resource(struct ps_widget *widget,int tid,int index,const struct ps_widget_type *editortype) {
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(tid);
  if (!restype) return -1;
  if (index>=restype->resc) return -1;
  const struct ps_res *res=restype->resv+index;
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editor=ps_widget_spawn(root,editortype);
  if (!editor) return -1;
  if (ps_widget_editor_set_resource(editor,res->id,res->obj)<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Open new editor for one resource.
 */
 
static int ps_edithome_open_resource(struct ps_widget *widget,int typeindex,int resindex) {
  if (!widget||(widget->type!=&ps_widget_type_edithome)) return -1;
  if (resindex<0) return -1;

  switch (typeindex) {
    case 0: return ps_edithome_open_song(widget,resindex);
    case 1: return ps_edithome_open_soundeffect(widget,resindex);
    case 2: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_BLUEPRINT,resindex,&ps_widget_type_editblueprint);
    case 3: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_SPRDEF,resindex,&ps_widget_type_editsprdef);
    case 4: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_PLRDEF,resindex,&ps_widget_type_editplrdef);
    case 5: return ps_edithome_open_ps_resource(widget,PS_RESTYPE_REGION,resindex,&ps_widget_type_editregion);
  }

  return -1;
}

/* Dispatch resource load to specific editor types.
 */

static int ps_edithome_cb_dismiss(struct ps_widget *button,struct ps_widget *dialogue) {
  return ps_widget_kill(dialogue);
}

int ps_widget_editor_set_resource(struct ps_widget *widget,int id,void *obj) {
  if (!widget||(id<0)||!obj) return -1;

  if (widget->type==&ps_widget_type_dialogue) {
    char msg[256];
    int msgc=snprintf(msg,sizeof(msg),"Failed to load resource ID %d (%p). Editor not yet implemented.",id,obj);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=snprintf(msg,sizeof(msg),"Editor not implemented.");
    if (ps_widget_dialogue_set_message(widget,msg,msgc)<0) return -1;
    if (ps_widget_dialogue_add_button(widget,"OK",2,ps_callback(ps_edithome_cb_dismiss,0,widget))<0) return -1;
    return 0;
  }

  //TODO load resource to specific types.

  return -1;
}
