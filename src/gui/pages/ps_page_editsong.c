#include "ps.h"
#include "gui/ps_gui.h"
#include "akau/akau.h"
#include "resedit/ps_isong.h"
#include "os/ps_fs.h"

static int ps_editsong_reload_resource(struct ps_page *page,int index,int id,struct ps_isong *isong);

/* Page definition.
 */

struct ps_page_editsong {
  struct ps_page hdr;
  struct ps_widget *resedit;
};

#define PAGE ((struct ps_page_editsong*)page)

/* Delete.
 */

static void _ps_editsong_del(struct ps_page *page) {
  ps_widget_del(PAGE->resedit);
}

/* New resource.
 */

static int ps_editsong_res_new(struct ps_page *page) {
  ps_log(GUI,TRACE,"%s",__func__);
  if (!page||(page->type!=&ps_page_type_editsong)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int id,index;
  if (akau_store_get_unused_song_id(&id,&index,store)<0) return -1;
  struct akau_song *song=akau_song_new();
  if (!song) return -1;
  if (akau_store_add_song(store,song,id)<0) {
    akau_song_del(song);
    return -1;
  }
  akau_song_del(song);
  return index;
}

static int ps_editsong_res_del(struct ps_page *page,int index) {
  ps_log(GUI,TRACE,"%s",__func__);
  if (!page||(page->type!=&ps_page_type_editsong)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_song(store);
  if ((index<0)||(index>=resc)) return -1;
  int resid=akau_store_get_song_id_by_index(store,index);
  if (resid<0) return -1;
  ps_log(EDIT,ERROR,"akau store does not support resource deletion. Please delete song:%d manually.",resid);
  return 0;
}

static int ps_editsong_res_count(struct ps_page *page) {
  ps_log(GUI,TRACE,"%s",__func__);
  if (!page||(page->type!=&ps_page_type_editsong)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_song(store);
  return resc;
}

static int ps_editsong_res_load(struct ps_page *page,int index) {
  ps_log(GUI,TRACE,"%s",__func__);
  if (!page||(page->type!=&ps_page_type_editsong)) return -1;
  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  int resc=akau_store_count_song(store);
  if ((index<0)||(index>=resc)) {
    return ps_widget_resedit_set_editor(PAGE->resedit,0);
  }
  struct akau_song *song=akau_store_get_song_by_index(store,index);
  if (!song) return -1;
  int id=akau_store_get_song_id_by_index(store,index);
  if (id<0) return -1;
  struct ps_isong *isong=ps_isong_from_song(song);
  if (!isong) return -1;
  int err=ps_editsong_reload_resource(page,index,id,isong);
  ps_isong_del(isong);
  return err;
}

static int ps_editsong_res_save(struct ps_page *page,int index) {
  ps_log(GUI,TRACE,"%s",__func__);

  /* Acquire song. */
  struct ps_widget *editor=ps_widget_resedit_get_editor(PAGE->resedit);
  if (!editor) return -1;
  struct ps_isong *isong=ps_widget_editsong_get_isong(editor);
  if (!isong) return -1;
  struct akau_song *song=ps_song_from_isong(isong);
  if (!song) return -1;

  /* Replace in store. */
  struct akau_store *store=akau_get_store();
  if (!store) {
    akau_song_del(song);
    return -1;
  }
  int resid=akau_store_get_song_id_by_index(store,index);
  if (resid<0) {
    akau_song_del(song);
    return -1;
  }
  if (akau_store_replace_song(store,song,resid)<0) {
    akau_song_del(song);
    return -1;
  }
  akau_song_del(song);

  /* Rewrite file. */
  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"song",resid);
  if (pathc>=(int)sizeof(path)) return -1;
  if (pathc<0) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"song",resid);
    if ((pathc<0)||(pathc>=sizeof(path))) return -1;
    if (ps_file_write(path,"",0)<0) return -1;
  }
  void *serial=0;
  int serialc=ps_isong_encode_assembly(&serial,isong);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to encode song.");
    return -1;
  }
  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to write file.",path);
    free(serial);
    return -1;
  }
  ps_log(EDIT,INFO,"Wrote song:%d resource to '%s'.",resid,path);
  
  return 0;
}

/* Initialize.
 */

static int _ps_editsong_init(struct ps_page *page) {

  page->root->bgrgba=0x400060ff;

  if (!(PAGE->resedit=ps_widget_spawn(page->root,&ps_widget_type_resedit))) return -1;
  if (ps_widget_ref(PAGE->resedit)<0) {
    PAGE->resedit=0;
    return -1;
  }
  if (PAGE->resedit->childc<1) return -1;
  struct ps_widget *reseditmenu=PAGE->resedit->childv[0];

  struct ps_resedit_delegate delegate={
    .res_new=ps_editsong_res_new,
    .res_del=ps_editsong_res_del,
    .res_count=ps_editsong_res_count,
    .res_load=ps_editsong_res_load,
    .res_save=ps_editsong_res_save,
  };
  if (ps_widget_resedit_set_delegate(PAGE->resedit,&delegate)<0) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_editsong_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_editsong_activate(struct ps_page *page) {
  return 0;
}

static int _ps_editsong_submit(struct ps_page *page) {
  return 0;
}

static int _ps_editsong_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_editsong_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_editsong={
  .name="editsong",
  .objlen=sizeof(struct ps_page_editsong),
  .init=_ps_editsong_init,
  .del=_ps_editsong_del,

  .move_cursor=_ps_editsong_move_cursor,
  .activate=_ps_editsong_activate,
  .submit=_ps_editsong_submit,
  .cancel=_ps_editsong_cancel,

  .update=_ps_editsong_update,
};

/* Reload resource.
 */
 
static int ps_editsong_reload_resource(struct ps_page *page,int index,int id,struct ps_isong *isong) {

  struct akau_store *store=akau_get_store();
  if (!store) return -1;

  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"song",id);
  if (pathc>=(int)sizeof(path)) return -1;
  if (pathc<0) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"song",id);
    if ((pathc<0)||(pathc>=sizeof(path))) return -1;
    if (ps_file_write(path,"",0)<0) return -1;
  }

  struct ps_widget *editor=ps_widget_new(&ps_widget_type_editsong);
  if (!editor) return -1;
  if (ps_widget_resedit_set_editor(PAGE->resedit,editor)<0) {
    ps_widget_del(editor);
    return -1;
  }
  ps_widget_del(editor);

  if (ps_widget_editsong_set_isong(editor,isong)<0) return -1;
  if (ps_widget_editsong_set_path(editor,path)<0) return -1;

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
