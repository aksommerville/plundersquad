#include "ps.h"
#include "gui/ps_gui.h"
#include "resedit/ps_iwg.h"
#include "akau/akau.h"
#include "os/ps_fs.h"

static int ps_editsfx_reload_resource(struct ps_page *page,int index,int id,struct akau_ipcm *ipcm);
static int ps_editsfx_play(struct ps_page *page);

/* Page definition.
 */

struct ps_page_editsfx {
  struct ps_page hdr;
  struct ps_widget *resedit; // Dude, remember ResEdit from back in the day? Good times.
};

#define PAGE ((struct ps_page_editsfx*)page)

/* Delete.
 */

static void _ps_editsfx_del(struct ps_page *page) {
  ps_widget_del(PAGE->resedit);
}

/* Create new resource.
 */

static int ps_editsfx_res_new(struct ps_page *page) {
  if (!page||(page->type!=&ps_page_type_editsfx)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int id,index;
  if (akau_store_get_unused_ipcm_id(&id,&index,store)<0) return -1;
  struct akau_ipcm *ipcm=akau_ipcm_new(1000);
  if (!ipcm) return -1;
  if (akau_store_add_ipcm(store,ipcm,id)<0) {
    akau_ipcm_del(ipcm);
    return -1;
  }
  akau_ipcm_del(ipcm);
  return index;
}

/* Delete resource.
 */

static int ps_editsfx_res_del(struct ps_page *page,int index) {
  if (!page||(page->type!=&ps_page_type_editsfx)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_ipcm(store);
  if ((index<0)||(index>=resc)) return -1;
  int resid=akau_store_get_ipcm_id_by_index(store,index);
  if (resid<0) return -1;
  ps_log(EDIT,ERROR,"akau store does not support resource deletion. Please delete ipcm:%d manually.",resid);
  return 0;
}

/* Count resources.
 */

static int ps_editsfx_res_count(struct ps_page *page) {
  if (!page||(page->type!=&ps_page_type_editsfx)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_ipcm(store);
  return resc;
}

/* Load resource.
 */

static int ps_editsfx_res_load(struct ps_page *page,int index) {
  if (!page||(page->type!=&ps_page_type_editsfx)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_ipcm(store);
  if ((index<0)||(index>=resc)) {
    return ps_widget_resedit_set_editor(PAGE->resedit,0);
  }
  struct akau_ipcm *ipcm=akau_store_get_ipcm_by_index(store,index);
  if (!ipcm) return -1;
  int id=akau_store_get_ipcm_id_by_index(store,index);
  if (id<0) return -1;
  return ps_editsfx_reload_resource(page,index,id,ipcm);
}

/* Save.
 */

static int ps_editsfx_res_save(struct ps_page *page,int index) {

  /* Acquire IPCM. */
  struct ps_widget *editor=ps_widget_resedit_get_editor(PAGE->resedit);
  if (!editor) return -1;
  struct akau_ipcm *ipcm=ps_widget_editsfx_get_ipcm(editor);
  if (!ipcm) return -1;
  struct ps_iwg *iwg=ps_widget_editsfx_get_iwg(editor);
  if (!iwg) return -1;

  /* Replace in store. */
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resid=akau_store_get_ipcm_id_by_index(store,index);
  if (resid<0) return -1;
  if (akau_store_replace_ipcm(store,ipcm,resid)<0) return -1;

  /* Rewrite file. */
  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"ipcm",resid);
  if (pathc>=(int)sizeof(path)) return -1;
  if (pathc<0) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"ipcm",resid);
    if ((pathc<0)||(pathc>=sizeof(path))) return -1;
    if (ps_file_write(path,"",0)<0) return -1;
  }
  if (ps_iwg_write_file(path,iwg)<0) return -1;
  ps_log(EDIT,INFO,"Wrote ipcm:%d resource to '%s'.",resid,path);
  
  return 0;
}

/* Initialize.
 */

static int _ps_editsfx_init(struct ps_page *page) {

  page->root->bgrgba=0x00a0e0ff;

  if (!(PAGE->resedit=ps_widget_spawn(page->root,&ps_widget_type_resedit))) return -1;
  if (ps_widget_ref(PAGE->resedit)<0) {
    PAGE->resedit=0;
    return -1;
  }
  if (PAGE->resedit->childc<1) return -1;
  struct ps_widget *reseditmenu=PAGE->resedit->childv[0];

  struct ps_resedit_delegate delegate={
    .res_new=ps_editsfx_res_new,
    .res_del=ps_editsfx_res_del,
    .res_count=ps_editsfx_res_count,
    .res_load=ps_editsfx_res_load,
    .res_save=ps_editsfx_res_save,
  };
  if (ps_widget_resedit_set_delegate(PAGE->resedit,&delegate)<0) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_editsfx_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_editsfx_activate(struct ps_page *page) {
  return 0;
}

static int _ps_editsfx_submit(struct ps_page *page) {
  if (ps_editsfx_play(page)<0) return -1;
  return 0;
}

static int _ps_editsfx_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_editsfx_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_editsfx={
  .name="editsfx",
  .objlen=sizeof(struct ps_page_editsfx),
  .init=_ps_editsfx_init,
  .del=_ps_editsfx_del,

  .move_cursor=_ps_editsfx_move_cursor,
  .activate=_ps_editsfx_activate,
  .submit=_ps_editsfx_submit,
  .cancel=_ps_editsfx_cancel,

  .update=_ps_editsfx_update,
};

/* Reload resource.
 */

static int ps_editsfx_reload_resource(struct ps_page *page,int index,int id,struct akau_ipcm *ipcm) {

  struct akau_store *store=akau_get_store();
  if (!store) return -1;

  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"ipcm",id);
  if (pathc>=(int)sizeof(path)) return -1;
  if (pathc<0) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"ipcm",id);
    if ((pathc<0)||(pathc>=sizeof(path))) return -1;
    if (ps_file_write(path,"",0)<0) return -1;
  }

  struct ps_widget *editor=ps_widget_new(&ps_widget_type_editsfx);
  if (!editor) return -1;
  if (ps_widget_resedit_set_editor(PAGE->resedit,editor)<0) {
    ps_widget_del(editor);
    return -1;
  }
  ps_widget_del(editor);

  if (ps_widget_editsfx_set_ipcm(editor,ipcm)<0) return -1;
  if (ps_widget_editsfx_set_path(editor,path)<0) return -1;

  struct ps_widget *reseditmenu=0;
  if (PAGE->resedit->childc>=1) reseditmenu=PAGE->resedit->childv[0];
  if (reseditmenu) {
    const char *base=path;
    int basec=pathc;
    int i=0; for (;i<pathc;i++) if (path[i]=='/') {
      base=path+i+1;
      basec=pathc-i-1;
    }
    if (ps_widget_reseditmenu_set_name(reseditmenu,base,basec)<0) return -1;
  }

  return 0;
}

/* Play sound.
 */

static int ps_editsfx_play(struct ps_page *page) {
  struct ps_widget *editor=ps_widget_resedit_get_editor(PAGE->resedit);
  if (!editor) return -1;
  struct akau_ipcm *ipcm=ps_widget_editsfx_get_ipcm(editor);
  if (!ipcm) return -1;
  struct akau_mixer *mixer=akau_get_mixer();
  if (!mixer) return -1;
  if (akau_mixer_play_ipcm(mixer,ipcm,0x80,0,0)<0) return -1;
  return 0;
}
