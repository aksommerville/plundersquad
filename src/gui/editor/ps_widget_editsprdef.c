/* ps_widget_editsprdef.c
 * Modal editor for sprdef resources.
 *
 * Children:
 *  [0] menubar
 *  [1] typelabel
 *  [2] typefield
 *  [3] tilelabel
 *  [4] tilefield
 *  [...] labelN,fieldN
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "util/ps_enums.h"
#include "util/ps_text.h"
#include "os/ps_fs.h"

#define PS_EDITSPRDEF_ROW_HEIGHT 16

#define PS_EDITSPRDEF_FIELD_COLOR_OK     0xffffffff
#define PS_EDITSPRDEF_FIELD_COLOR_ERROR  0xff8080ff

static int ps_editsprdef_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsprdef_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsprdef_cb_add_field(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsprdef_cb_edit_field(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsprdef_cb_modify(struct ps_widget *field,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editsprdef {
  struct ps_widget hdr;
  struct ps_sprdef *sprdef;
  int resid;
  int valid;
};

#define WIDGET ((struct ps_widget_editsprdef*)widget)

/* Delete.
 */

static void _ps_editsprdef_del(struct ps_widget *widget) {
  ps_sprdef_del(WIDGET->sprdef);
}

/* Initialize.
 */

static int _ps_editsprdef_init(struct ps_widget *widget) {

  widget->bgrgba=0x0080ffff;

  struct ps_widget *menubar=ps_widget_spawn(widget,&ps_widget_type_menubar);
  if (!menubar) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Back",4,ps_callback(ps_editsprdef_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Save",4,ps_callback(ps_editsprdef_cb_save,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"+Fld",4,ps_callback(ps_editsprdef_cb_add_field,0,widget))) return -1;

  struct ps_widget *child;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1;
  if (ps_widget_button_set_text(child,"Type",4)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editsprdef_cb_edit_field,0,widget))<0) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  child->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editsprdef_cb_modify,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1;
  if (ps_widget_button_set_text(child,"Tile",4)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editsprdef_cb_edit_field,0,widget))<0) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  child->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editsprdef_cb_modify,0,widget))<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_editsprdef_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editsprdef) return -1;
  if (widget->childc<5) return -1;
  if (!(widget->childc&1)) return -1;
  return 0;
}

static struct ps_widget *ps_editsprdef_get_menubar(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editsprdef_get_typelabel(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editsprdef_get_typefield(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_editsprdef_get_tilelabel(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_editsprdef_get_tilefield(const struct ps_widget *widget) { return widget->childv[4]; }

static int ps_editsprdef_count_extra_fields(const struct ps_widget *widget) {
  return (widget->childc-5)/2;
}

static struct ps_widget *ps_editsprdef_get_extralabel(const struct ps_widget *widget,int fldp) {
  if (fldp<0) return 0;
  int childp=5+(fldp<<1);
  if (childp>=widget->childc) return 0;
  return widget->childv[childp];
}

static struct ps_widget *ps_editsprdef_get_extrafield(const struct ps_widget *widget,int fldp) {
  if (fldp<0) return 0;
  int childp=6+(fldp<<1);
  if (childp>=widget->childc) return 0;
  return widget->childv[childp];
}

static struct ps_widget *ps_editsprdef_get_field_for_key(const struct ps_widget *widget,int k) {
  int fldp=ps_sprdef_fld_search(WIDGET->sprdef,k);
  return ps_editsprdef_get_extrafield(widget,fldp);
}

/* Pack.
 */

static int _ps_editsprdef_pack(struct ps_widget *widget) {
  if (ps_editsprdef_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editsprdef_get_menubar(widget);
  int chw,chh,i;

  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  /* Pack type and tile fields as if they are extras; there's really no difference. */
  int y=menubar->h;
  int labels_right=widget->w/4;
  for (i=1;i<widget->childc;i+=2) {
    struct ps_widget *label=widget->childv[i];
    struct ps_widget *field=widget->childv[i+1];
    if (ps_widget_measure(&chw,&chh,label,widget->w,PS_EDITSPRDEF_ROW_HEIGHT)<0) return -1;
    label->x=labels_right-chw;
    label->y=y;
    label->w=chw;
    label->h=PS_EDITSPRDEF_ROW_HEIGHT;
    field->x=labels_right;
    field->y=y;
    field->w=widget->w-field->x;
    field->h=PS_EDITSPRDEF_ROW_HEIGHT;
    y+=PS_EDITSPRDEF_ROW_HEIGHT;
  }

  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsprdef={

  .name="editsprdef",
  .objlen=sizeof(struct ps_widget_editsprdef),

  .del=_ps_editsprdef_del,
  .init=_ps_editsprdef_init,

  .pack=_ps_editsprdef_pack,

};

/* Populate UI from model.
 */

static int ps_editsprdef_populate_ui_from_model(struct ps_widget *widget) {
  if (ps_editsprdef_obj_validate(widget)<0) return -1;
  if (!WIDGET->sprdef) return -1;
  struct ps_widget *typefield=ps_editsprdef_get_typefield(widget);
  struct ps_widget *tilefield=ps_editsprdef_get_tilefield(widget);

  if (ps_widget_field_set_text(typefield,WIDGET->sprdef->type->name,-1)<0) return -1;
  
  char buf[256];
  snprintf(buf,sizeof(buf),"%04x",WIDGET->sprdef->tileid);
  if (ps_widget_field_set_text(tilefield,buf,-1)<0) return -1;

  /* Add or remove extra fields. */
  int extrafldc=ps_editsprdef_count_extra_fields(widget);
  while (extrafldc<WIDGET->sprdef->fldc) {
    struct ps_widget *child;
    if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1;
    if (ps_widget_button_set_callback(child,ps_callback(ps_editsprdef_cb_edit_field,0,widget))<0) return -1;
    if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
    child->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
    if (ps_widget_field_set_cb_change(child,ps_callback(ps_editsprdef_cb_modify,0,widget))<0) return -1;
    extrafldc++;
  }
  while (extrafldc>WIDGET->sprdef->fldc) {
    if (ps_widget_remove_child(widget,widget->childv[widget->childc-1])<0) return -1;
    if (ps_widget_remove_child(widget,widget->childv[widget->childc-1])<0) return -1;
    extrafldc--;
  }

  /* Populate extra fields. */
  int i=WIDGET->sprdef->fldc; while (i-->0) {
    struct ps_widget *label=ps_editsprdef_get_extralabel(widget,i);
    struct ps_widget *field=ps_editsprdef_get_extrafield(widget,i);
    if (!label||!field) return -1;

    const char *name=ps_sprdef_fld_k_repr(WIDGET->sprdef->fldv[i].k);
    if (!name) {
      snprintf(buf,sizeof(buf),"Field %d",WIDGET->sprdef->fldv[i].k);
      name=buf;
    }
    if (ps_widget_button_set_text(label,name,-1)<0) return -1;

    int bufc=ps_sprdef_fld_v_repr(buf,sizeof(buf),WIDGET->sprdef->fldv[i].k,WIDGET->sprdef->fldv[i].v);
    if ((bufc<0)||(bufc>=sizeof(buf))) {
      bufc=snprintf(buf,sizeof(buf),"%d",WIDGET->sprdef->fldv[i].v);
    }
    if (ps_widget_field_set_text(field,buf,bufc)<0) return -1;
  }
  
  return 0;
}

/* Populate model from UI.
 * Errors are real.
 * If model is invalid, we unset WIDGET->valid and highlight as necessary.
 * ps_sprdef is pre-allocated to the necessary size for its extra fields.
 * That's a pain in the ass here and I don't know why I made it that way.
 * Owing to that, we reallocate and replace WIDGET->sprdef when the field count changes.
 * That means, on save we must replace the shared resource too.
 */

static int ps_editsprdef_tile_eval(const char *src,int srcc) {
  if (srcc!=4) return -1;
  int tileid=0;
  for (;srcc-->0;src++) {
    tileid<<=4;
         if ((*src>='0')&&(*src<='9')) tileid|=*src-'0';
    else if ((*src>='a')&&(*src<='f')) tileid|=*src-'a'+10;
    else if ((*src>='A')&&(*src<='F')) tileid|=*src-'A'+10;
    else return -1;
  }
  return tileid;
}

static int ps_editsprdef_populate_model_from_ui(struct ps_widget *widget) {
  if (ps_editsprdef_obj_validate(widget)<0) return -1;
  struct ps_widget *field;
  const char *src;
  int srcc;
  WIDGET->valid=1;

  if (!(field=ps_editsprdef_get_typefield(widget))) return -1;
  if ((srcc=ps_widget_field_get_text(&src,field))<0) return -1;
  const struct ps_sprtype *type=ps_sprtype_by_name(src,srcc);
  if (type) {
    field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
    WIDGET->sprdef->type=type;
  } else {
    field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_ERROR;
    WIDGET->valid=0;
  }

  if (!(field=ps_editsprdef_get_tilefield(widget))) return -1;
  if ((srcc=ps_widget_field_get_text(&src,field))<0) return -1;
  int tileid=ps_editsprdef_tile_eval(src,srcc);
  if (tileid>=0) {
    field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
    WIDGET->sprdef->tileid=tileid;
  } else {
    field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_ERROR;
    WIDGET->valid=0;
  }

  int i=WIDGET->sprdef->fldc; while (i-->0) {
    if (!(field=ps_editsprdef_get_extrafield(widget,i))) return -1;
    if ((srcc=ps_widget_field_get_text(&src,field))<0) return -1;
    int v;
    if (ps_sprdef_fld_v_eval(&v,WIDGET->sprdef->fldv[i].k,src,srcc)>=0) {
      field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
      WIDGET->sprdef->fldv[i].v=v;
    } else {
      field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_ERROR;
      WIDGET->valid=0;
    }
  }
  
  return 0;
}

/* Set resource.
 */
 
int ps_widget_editsprdef_set_resource(struct ps_widget *widget,int id,struct ps_sprdef *sprdef,const char *name) {
  if (ps_editsprdef_obj_validate(widget)<0) return -1;
  if (ps_sprdef_ref(sprdef)<0) return -1;
  ps_sprdef_del(WIDGET->sprdef);
  WIDGET->sprdef=sprdef;
  WIDGET->resid=id;
  WIDGET->valid=1;
  if (ps_widget_menubar_set_title(ps_editsprdef_get_menubar(widget),name,-1)<0) return -1;
  if (ps_editsprdef_populate_ui_from_model(widget)<0) return -1;
  return 0;
}

/* Save to file.
 */

static int ps_editsprdef_save_to_file(struct ps_widget *widget) {

  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_SPRDEF,WIDGET->resid,0);
  if ((pathc<0)||(pathc>=sizeof(path))) {
    ps_log(EDIT,ERROR,"Failed to compose path for sprdef:%d",WIDGET->resid);
    return -1;
  }

  void *serial=0;
  int serialc=ps_sprdef_encode(&serial,WIDGET->sprdef);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to encode sprdef");
    return -1;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to save sprdef:%d",path,WIDGET->resid);
    free(serial);
    return -1;
  }

  ps_log(EDIT,INFO,"%s: Saved sprdef:%d, %d bytes.",path,WIDGET->resid,serialc);
  free(serial);
  return 0;
}

/* Replace resource in global manager.
 */

static int ps_editsprdef_replace_shared_resource(struct ps_widget *widget) {
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_SPRDEF);
  if (!restype) return -1;

  int resp=ps_restype_res_search(restype,WIDGET->resid);

  /* If it doesn't exist in the global store, insert it.
   * Insertion is a handoff, so we must retain it ourselves.
   */
  if (resp<0) {
    resp=-resp-1;
    if (ps_sprdef_ref(WIDGET->sprdef)<0) return -1;
    if (ps_restype_res_insert(restype,resp,WIDGET->resid,WIDGET->sprdef)<0) {
      ps_sprdef_del(WIDGET->sprdef);
      return -1;
    }
    return 0;
  }

  /* ps_restype does not provide 'replace' functionality, so we'll do it ourselves.
   * This makes me sad.
   */
  if (ps_sprdef_ref(WIDGET->sprdef)<0) return -1;
  ps_sprdef_del(restype->resv[resp].obj);
  restype->resv[resp].obj=WIDGET->sprdef;
  
  return 0;
}

/* Edit type interactively.
 */

static int ps_editsprdef_cb_type_selected(struct ps_widget *scrolllist,struct ps_widget *widget) {
  
  const char *selection=ps_widget_scrolllist_get_text(scrolllist);
  const struct ps_sprtype *sprtype=ps_sprtype_by_name(selection,-1);
  if (!sprtype) return 0;
  WIDGET->sprdef->type=sprtype;

  struct ps_widget *typefield=ps_editsprdef_get_typefield(widget);
  if (ps_widget_field_set_text(typefield,selection,-1)<0) return -1;
  typefield->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;

  if (ps_widget_plaindialogue_dismiss(scrolllist)<0) return -1;
  return 0;
}

static int ps_editsprdef_edit_type(struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_plaindialogue);
  if (!dialogue) return -1;

  struct ps_widget *scrolllist=ps_widget_spawn(dialogue,&ps_widget_type_scrolllist);
  if (!scrolllist) return -1;
  if (ps_widget_scrolllist_enable_selection(scrolllist,ps_callback(ps_editsprdef_cb_type_selected,0,widget))<0) return -1;

  int selection=-1,i=0;
  const struct ps_sprtype **sprtype=ps_all_sprtypes;
  for (;*sprtype;sprtype++,i++) {
    if (*sprtype==WIDGET->sprdef->type) selection=i;
    struct ps_widget *label=ps_widget_scrolllist_add_label(scrolllist,(*sprtype)->name,-1);
    if (!label) return -1;
  }
  if (ps_widget_scrolllist_set_selection_without_callback(scrolllist,selection)<0) return -1;

  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit tile interactively.
 */

static int ps_editsprdef_edit_tile_ok(struct ps_widget *tileselect,struct ps_widget *widget) {
  WIDGET->sprdef->tileid=ps_widget_tileselect_get_tileid(tileselect);
  struct ps_widget *tilefield=ps_editsprdef_get_tilefield(widget);
  char buf[5];
  snprintf(buf,sizeof(buf),"%04x",WIDGET->sprdef->tileid);
  if (ps_widget_field_set_text(tilefield,buf,-1)<0) return -1;
  tilefield->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
  return 0;
}

static int ps_editsprdef_edit_tile(struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *tileselect=ps_widget_spawn(root,&ps_widget_type_tileselect);
  if (!tileselect) return -1;
  if (ps_widget_tileselect_set_tileid(tileselect,WIDGET->sprdef->tileid)<0) return -1;
  if (ps_widget_tileselect_set_callback(tileselect,ps_callback(ps_editsprdef_edit_tile_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit group mask interactively.
 */

static int ps_editsprdef_edit_field_grpmask_ok(struct ps_widget *editgrpmask,struct ps_widget *widget) {
  uint32_t grpmask=ps_widget_editgrpmask_get_grpmask(editgrpmask);
  int fldp=ps_sprdef_fld_search(WIDGET->sprdef,PS_SPRDEF_FLD_grpmask);
  if (fldp<0) return -1;
  WIDGET->sprdef->fldv[fldp].v=grpmask;
  struct ps_widget *field=ps_editsprdef_get_extrafield(widget,fldp);
  if (!field) return -1;
  char buf[256];
  int bufc=ps_sprdef_fld_v_repr(buf,sizeof(buf),PS_SPRDEF_FLD_grpmask,grpmask);
  if ((bufc<0)||(bufc>=sizeof(buf))) {
    bufc=ps_decsint_repr(buf,sizeof(buf),grpmask);
  }
  if (ps_widget_field_set_text(field,buf,bufc)<0) return -1;
  field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;
  return 0;
}

static int ps_editsprdef_edit_field_grpmask(struct ps_widget *widget,int v) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editgrpmask=ps_widget_spawn(root,&ps_widget_type_editgrpmask);
  if (!editgrpmask) return -1;
  if (ps_widget_editgrpmask_set_grpmask(editgrpmask,v)<0) return -1;
  if (ps_widget_editgrpmask_set_callback(editgrpmask,ps_callback(ps_editsprdef_edit_field_grpmask_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit shape interactively.
 */

static int ps_editsprdef_cb_shape_selected(struct ps_widget *scrolllist,struct ps_widget *widget) {
  
  int selection=ps_widget_scrolllist_get_selection(scrolllist);
  int fldp=ps_sprdef_fld_search(WIDGET->sprdef,PS_SPRDEF_FLD_shape);
  if (fldp<0) return -1;

  struct ps_widget *field=ps_editsprdef_get_extrafield(widget,fldp);
  if (!field) return -1;
  if (ps_widget_field_set_text(field,ps_sprite_shape_repr(selection),-1)<0) return -1;
  field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;

  WIDGET->sprdef->fldv[fldp].v=selection;

  if (ps_widget_plaindialogue_dismiss(scrolllist)<0) return -1;
  return 0;
}

static int ps_editsprdef_edit_field_shape(struct ps_widget *widget,int v) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_plaindialogue);
  if (!dialogue) return -1;

  struct ps_widget *scrolllist=ps_widget_spawn(dialogue,&ps_widget_type_scrolllist);
  if (!scrolllist) return -1;
  if (ps_widget_scrolllist_enable_selection(scrolllist,ps_callback(ps_editsprdef_cb_shape_selected,0,widget))<0) return -1;

  struct ps_widget *label;
  if (!(label=ps_widget_scrolllist_add_label(scrolllist,"SQUARE",6))) return -1;
  if (!(label=ps_widget_scrolllist_add_label(scrolllist,"CIRCLE",6))) return -1;
  if (ps_widget_scrolllist_set_selection_without_callback(scrolllist,v)<0) return -1;

  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit generic field interactively.
 */

static int ps_editsprdef_edit_field_generic_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int k=ps_widget_dialogue_get_refnum1(dialogue);
  const char *src=0;
  int srcc=ps_widget_dialogue_get_text(&src,dialogue);
  if ((srcc<0)||!src) return 0;

  int v;
  if (ps_sprdef_fld_v_eval(&v,k,src,srcc)<0) {
    if (ps_int_eval(&v,src,srcc)<0) return 0;
  }

  int fldp=ps_sprdef_fld_search(WIDGET->sprdef,k);
  if (fldp<0) return 0;
  WIDGET->sprdef->fldv[fldp].v=v;

  struct ps_widget *field=ps_editsprdef_get_extrafield(widget,fldp);
  if (!field) return 0;
  if (ps_widget_field_set_text(field,src,srcc)<0) return -1;
  field->bgrgba=PS_EDITSPRDEF_FIELD_COLOR_OK;

  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editsprdef_edit_field_generic(struct ps_widget *widget,int k,int v) {

  char buf[256];
  int bufc=ps_sprdef_fld_v_repr(buf,sizeof(buf),k,v);
  if ((bufc<0)||(bufc>=sizeof(buf))) {
    bufc=ps_decsint_repr(buf,sizeof(buf),v);
  }

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_text(
    ps_widget_get_root(widget),
    ps_sprdef_fld_k_repr(k),-1,
    buf,bufc,
    ps_editsprdef_edit_field_generic_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  if (ps_widget_dialogue_set_refnum1(dialogue,k)<0) return -1;
  
  return 0;
}

/* Edit extra field interactively.
 */

static int ps_editsprdef_edit_field(struct ps_widget *widget,int fldp) {
  if ((fldp<0)||(fldp>=WIDGET->sprdef->fldc)) return 0;
  int k=WIDGET->sprdef->fldv[fldp].k;
  int v=WIDGET->sprdef->fldv[fldp].v;
  switch (k) {
    case PS_SPRDEF_FLD_grpmask: return ps_editsprdef_edit_field_grpmask(widget,v);
    case PS_SPRDEF_FLD_shape: return ps_editsprdef_edit_field_shape(widget,v);
    default: return ps_editsprdef_edit_field_generic(widget,k,v);
  }
  return 0;
}

/* Callbacks.
 */
 
static int ps_editsprdef_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editsprdef_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  /* If the model is invalid, double-check, then reject if it's still invalid.
   * Some edits might not fully revalidate.
   */
  if (!WIDGET->valid) {
    if (ps_editsprdef_populate_model_from_ui(widget)<0) return -1;
    if (!WIDGET->valid) {
      ps_log(EDIT,ERROR,"Model invalid, refusing to save sprdef.");
      return 0;
    }
  }
  
  if (ps_editsprdef_save_to_file(widget)<0) {
    ps_log(EDIT,ERROR,"Failed to save sprdef.");
    return 0;
  }
  if (ps_editsprdef_replace_shared_resource(widget)<0) {
    ps_log(EDIT,ERROR,"Failed to replace shared resource (file was written ok)");
    return 0;
  }
  return 0;
}

static int ps_editsprdef_cb_edit_field(struct ps_widget *button,struct ps_widget *widget) {
  if (!WIDGET->sprdef) return 0;
  int childp=ps_widget_get_index_of_child(widget,button);
  if (childp<0) return 0;
  if (childp==1) return ps_editsprdef_edit_type(widget);
  if (childp==3) return ps_editsprdef_edit_tile(widget);
  int fldp=(childp-5)>>1;
  if ((fldp>=0)&&(fldp<WIDGET->sprdef->fldc)) return ps_editsprdef_edit_field(widget,fldp);
  return 0;
}

static int ps_editsprdef_cb_modify(struct ps_widget *field,struct ps_widget *widget) {
  if (ps_editsprdef_populate_model_from_ui(widget)<0) return -1;
  return 0;
}

/* Add field.
 * This is dicey because the field list of a sprdef is fixed at instantiation.
 * We'll allocate a new one.
 * When we save changes, we must commit to the global resource manager.
 */

static int ps_editsprdef_cb_add_field_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  const char *src=0;
  int srcc=ps_widget_dialogue_get_text(&src,dialogue);
  if ((srcc<0)||!src) return 0;

  int k=ps_sprdef_fld_k_eval(src,srcc);
  if (k<0) {
    if (ps_int_eval(&k,src,srcc)) return 0;
  }
  if (!ps_sprdef_fld_k_repr(k)) return 0; // Ensure that it really is a field key.
  if (k==PS_SPRDEF_FLD_tileid) return 0; // Can't add a base field.
  if (k==PS_SPRDEF_FLD_type) return 0; // Can't add a base field.

  int fldp=ps_sprdef_fld_search(WIDGET->sprdef,k);
  if (fldp>=0) return 0; // Already defined.
  fldp=-fldp-1;

  /* Create a new sprdef with this field inserted.
   */
  struct ps_sprdef *nsprdef=ps_sprdef_new(WIDGET->sprdef->fldc+1);
  if (!nsprdef) return -1;
  nsprdef->type=WIDGET->sprdef->type;
  nsprdef->tileid=WIDGET->sprdef->tileid;
  nsprdef->fldc=WIDGET->sprdef->fldc+1;
  memcpy(nsprdef->fldv,WIDGET->sprdef->fldv,sizeof(struct ps_sprdef_fld)*fldp);
  memcpy(nsprdef->fldv+fldp+1,WIDGET->sprdef->fldv+fldp,sizeof(struct ps_sprdef_fld)*(WIDGET->sprdef->fldc-fldp));
  nsprdef->fldv[fldp].k=k;
  nsprdef->fldv[fldp].v=0;

  /* Swap out sprdefs.
   */
  ps_sprdef_del(WIDGET->sprdef);
  WIDGET->sprdef=nsprdef;

  if (ps_editsprdef_populate_ui_from_model(widget)<0) return -1;

  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editsprdef_cb_add_field(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_text(
    ps_widget_get_root(widget),
    "Field name or ID:",-1,
    "",0,
    ps_editsprdef_cb_add_field_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;

  return 0;
}
