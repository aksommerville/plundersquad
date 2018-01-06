/* ps_widget_editblueprint.c
 * Main controller for blueprint editor.
 *
 * Children:
 *  [0] menubar
 *  [1] metadata (scrolllist)
 *  [2] bpchart
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_text.h"
#include "res/ps_resmgr.h"
#include "os/ps_fs.h"

static int ps_editblueprint_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_cb_add_poi(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_cb_add_solution(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_cb_monsterclo(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_cb_monsterchi(struct ps_widget *button,struct ps_widget *widget);
static int ps_editblueprint_rebuild_metadata(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editblueprint {
  struct ps_widget hdr;
  struct ps_blueprint *blueprint;
  int resid;
};

#define WIDGET ((struct ps_widget_editblueprint*)widget)

/* Delete.
 */

static void _ps_editblueprint_del(struct ps_widget *widget) {
  ps_blueprint_del(WIDGET->blueprint);
}

/* Initialize.
 */

static int _ps_editblueprint_init(struct ps_widget *widget) {

  widget->bgrgba=0x000000ff;

  struct ps_widget *menubar=ps_widget_spawn(widget,&ps_widget_type_menubar);
  if (!menubar) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Back",4,ps_callback(ps_editblueprint_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"Save",4,ps_callback(ps_editblueprint_cb_save,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"+POI",4,ps_callback(ps_editblueprint_cb_add_poi,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(menubar,"+Sln",4,ps_callback(ps_editblueprint_cb_add_solution,0,widget))) return -1;

  struct ps_widget *metadata=ps_widget_spawn(widget,&ps_widget_type_scrolllist);
  if (!metadata) return -1;
  struct ps_widget *metarow;
  if (!(metarow=ps_widget_button_spawn(metadata,0,"monstr lo: ?",-1,ps_callback(ps_editblueprint_cb_monsterclo,0,widget)))) return -1;
  if (!(metarow=ps_widget_button_spawn(metadata,0,"monstr hi: ?",-1,ps_callback(ps_editblueprint_cb_monsterchi,0,widget)))) return -1;

  struct ps_widget *bpchart=ps_widget_spawn(widget,&ps_widget_type_bpchart);
  if (!bpchart) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_editblueprint_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editblueprint) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_editblueprint_get_menubar(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_editblueprint_get_metadata(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_editblueprint_get_bpchart(const struct ps_widget *widget) {
  return widget->childv[2];
}

/* Draw.
 */

static int _ps_editblueprint_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Pack.
 */

static int _ps_editblueprint_pack(struct ps_widget *widget) {
  if (ps_editblueprint_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editblueprint_get_menubar(widget);
  struct ps_widget *metadata=ps_editblueprint_get_metadata(widget);
  struct ps_widget *bpchart=ps_editblueprint_get_bpchart(widget);
  int chw,chh;

  /* Menubar along the top. */
  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  /* Metadata on the left. */
  if (ps_widget_measure(&chw,&chh,metadata,widget->w,widget->h-menubar->h)<0) return -1;
  metadata->x=0;
  metadata->y=menubar->h;
  metadata->w=chw;
  metadata->h=widget->h-menubar->h;

  /* Bpchart in whatever is left. */
  bpchart->x=metadata->w;
  bpchart->y=menubar->h;
  bpchart->w=widget->w-metadata->w;
  bpchart->h=widget->h-menubar->h;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_editblueprint_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_editblueprint_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_editblueprint_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_editblueprint_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_editblueprint_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_editblueprint_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editblueprint_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editblueprint_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_editblueprint_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_editblueprint_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_editblueprint_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_editblueprint_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editblueprint={

  .name="editblueprint",
  .objlen=sizeof(struct ps_widget_editblueprint),

  .del=_ps_editblueprint_del,
  .init=_ps_editblueprint_init,

  .draw=_ps_editblueprint_draw,
  .pack=_ps_editblueprint_pack,

  .mousemotion=_ps_editblueprint_mousemotion,
  .mousebutton=_ps_editblueprint_mousebutton,
  .mousewheel=_ps_editblueprint_mousewheel,
  .key=_ps_editblueprint_key,
  .userinput=_ps_editblueprint_userinput,

  .mouseenter=_ps_editblueprint_mouseenter,
  .mouseexit=_ps_editblueprint_mouseexit,
  .activate=_ps_editblueprint_activate,
  .cancel=_ps_editblueprint_cancel,
  .adjust=_ps_editblueprint_adjust,
  .focus=_ps_editblueprint_focus,
  .unfocus=_ps_editblueprint_unfocus,

};

/* Remove poi from blueprint.
 */

static int ps_editblueprint_remove_poi(struct ps_blueprint *blueprint,int poip) {
  if (!blueprint) return -1;
  if ((poip<0)||(poip>=blueprint->poic)) return -1;
  blueprint->poic--;
  memmove(blueprint->poiv+poip,blueprint->poiv+poip+1,sizeof(struct ps_blueprint_poi)*(blueprint->poic-poip));
  return 0;
}

static int ps_editblueprint_remove_solution(struct ps_blueprint *blueprint,int solutionp) {
  if (!blueprint) return -1;
  if ((solutionp<0)||(solutionp>=blueprint->solutionc)) return -1;
  blueprint->solutionc--;
  memmove(blueprint->solutionv+solutionp,blueprint->solutionv+solutionp+1,sizeof(struct ps_blueprint_solution)*(blueprint->solutionc-solutionp));
  return 0;
}

/* Click on POI button.
 */

static int ps_editblueprint_cb_poi_ok(struct ps_widget *editpoi,struct ps_widget *widget) {
  int poip=ps_widget_editpoi_get_index(editpoi);
  const struct ps_blueprint_poi *poi=ps_widget_editpoi_get_poi(editpoi);
  if (!poi) return 0;
  if ((poip<0)||(poip>=WIDGET->blueprint->poic)) return 0;
  if (ps_widget_editpoi_wants_deleted(editpoi)) {
    if (ps_editblueprint_remove_poi(WIDGET->blueprint,poip)<0) return -1;
  } else {
    memcpy(WIDGET->blueprint->poiv+poip,poi,sizeof(struct ps_blueprint_poi));
  }
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_poi(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *metadata=ps_editblueprint_get_metadata(widget);
  int childp=ps_widget_get_index_of_child(metadata,button);
  if (childp<0) return 0;
  int poip=childp-2;
  if ((poip<0)||(poip>=WIDGET->blueprint->poic)) return 0;
  struct ps_blueprint_poi *poi=WIDGET->blueprint->poiv+poip;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editpoi=ps_widget_spawn(root,&ps_widget_type_editpoi);
  if (!editpoi) return -1;
  if (ps_widget_editpoi_set_index(editpoi,poip)<0) return -1;
  if (ps_widget_editpoi_set_blueprint(editpoi,WIDGET->blueprint)<0) return -1;
  if (ps_widget_editpoi_set_poi(editpoi,poi)<0) return -1;
  if (ps_widget_editpoi_set_callback(editpoi,ps_callback(ps_editblueprint_cb_poi_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  
  return 0;
}

/* Click on solution button.
 */

static int ps_editblueprint_cb_solution_ok(struct ps_widget *editsolution,struct ps_widget *widget) {
  int solutionp=ps_widget_editsolution_get_index(editsolution);
  const struct ps_blueprint_solution *solution=ps_widget_editsolution_get_solution(editsolution);
  if (!solution) return 0;
  if ((solutionp<0)||(solutionp>=WIDGET->blueprint->solutionc)) return 0;
  if (ps_widget_editsolution_wants_deleted(editsolution)) {
    if (ps_editblueprint_remove_solution(WIDGET->blueprint,solutionp)<0) return -1;
  } else {
    memcpy(WIDGET->blueprint->solutionv+solutionp,solution,sizeof(struct ps_blueprint_solution));
  }
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_solution(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *metadata=ps_editblueprint_get_metadata(widget);
  int childp=ps_widget_get_index_of_child(metadata,button);
  if (childp<0) return 0;
  int solutionp=childp-WIDGET->blueprint->poic-2;
  if ((solutionp<0)||(solutionp>=WIDGET->blueprint->solutionc)) return 0;
  struct ps_blueprint_solution *solution=WIDGET->blueprint->solutionv+solutionp;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editsolution=ps_widget_spawn(root,&ps_widget_type_editsolution);
  if (!editsolution) return -1;
  if (ps_widget_editsolution_set_solution(editsolution,solution)<0) return -1;
  if (ps_widget_editsolution_set_index(editsolution,solutionp)<0) return -1;
  if (ps_widget_editsolution_set_callback(editsolution,ps_callback(ps_editblueprint_cb_solution_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  
  return 0;
}

/* Compose label for POI or solution.
 */

static int ps_editblueprint_repr_poi(char *dst,int dsta,const struct ps_blueprint_poi *poi) {
  int dstc=0;
  
  const char *name=0;
  switch (poi->type) {//TODO enum for poi type
    case PS_BLUEPRINT_POI_NOOP: name="NOOP"; break;
    case PS_BLUEPRINT_POI_SPRITE: name="SPRITE"; break;
    case PS_BLUEPRINT_POI_TREASURE: name="TREASURE"; break;
    case PS_BLUEPRINT_POI_HERO: name="HERO"; break;
    case PS_BLUEPRINT_POI_BARRIER: name="BARRIER"; break;
    case PS_BLUEPRINT_POI_DEATHGATE: name="DEATHGATE"; break;
    case PS_BLUEPRINT_POI_STATUSREPORT: name="STATUSREPORT"; break;
  }
  if (name) dstc=ps_strcpy(dst,dsta,name,-1);
  else dstc=ps_decsint_repr(dst,dsta,poi->type);

  /* We could be smart about it and populate the label based on type.
   * But for the most part, argv[0] is the only thing we want to see.
   */
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=ps_decsint_repr(dst+dstc,dsta-dstc,poi->argv[0]);

  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

static int ps_editblueprint_repr_solution(char *dst,int dsta,const struct ps_blueprint_solution *solution) {
  int dstc=snprintf(dst,dsta,"%d,%d,%d,%04x",solution->plo,solution->phi,solution->difficulty,solution->skills);
  return dstc;
}

/* Rebuild metadata.
 */

static int ps_editblueprint_rebuild_metadata(struct ps_widget *widget) {
  if (!WIDGET->blueprint) return -1;
  
  struct ps_widget *metadata=ps_editblueprint_get_metadata(widget);
  while (metadata->childc>2) {
    if (ps_widget_remove_child(metadata,metadata->childv[metadata->childc-1])<0) return -1;
  }
  if (metadata->childc<2) return -1;

  char buf[32];
  int bufc;
  struct ps_widget *button=metadata->childv[0];
  bufc=snprintf(buf,sizeof(buf),"monster lo: %d",WIDGET->blueprint->monsterc_min);
  if (ps_widget_button_set_text(button,buf,bufc)<0) return -1;

  button=metadata->childv[1];
  bufc=snprintf(buf,sizeof(buf),"monster hi: %d",WIDGET->blueprint->monsterc_max);
  if (ps_widget_button_set_text(button,buf,bufc)<0) return -1;

  const struct ps_blueprint_poi *poi=WIDGET->blueprint->poiv;
  int i=WIDGET->blueprint->poic; for (;i-->0;poi++) {
    bufc=ps_editblueprint_repr_poi(buf,sizeof(buf),poi);
    if (bufc>(int)sizeof(buf)) bufc=0;
    if (!(button=ps_widget_button_spawn(metadata,0,buf,bufc,ps_callback(ps_editblueprint_cb_poi,0,widget)))) return -1;
  }

  const struct ps_blueprint_solution *solution=WIDGET->blueprint->solutionv;
  for (i=WIDGET->blueprint->solutionc;i-->0;solution++) {
    bufc=ps_editblueprint_repr_solution(buf,sizeof(buf),solution);
    if (bufc>(int)sizeof(buf)) bufc=0;
    if (!(button=ps_widget_button_spawn(metadata,0,buf,bufc,ps_callback(ps_editblueprint_cb_solution,0,widget)))) return -1;
  }

  return 0;
}

/* Set resource.
 */
 
int ps_widget_editblueprint_set_resource(struct ps_widget *widget,int id,struct ps_blueprint *blueprint,const char *name) {
  if (ps_editblueprint_obj_validate(widget)<0) return -1;

  if (ps_blueprint_ref(blueprint)<0) return -1;
  ps_blueprint_del(WIDGET->blueprint);
  WIDGET->blueprint=blueprint;
  WIDGET->resid=id;
  if (ps_widget_menubar_set_title(ps_editblueprint_get_menubar(widget),name,-1)<0) return -1;
  
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;

  return 0;
}

/* Menubar callbacks.
 */
 
static int ps_editblueprint_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_BLUEPRINT,WIDGET->resid,0);
  if ((pathc<1)||(pathc>=sizeof(path))) {
    ps_log(EDIT,ERROR,"Failed to compose resource path for blueprint:%d",WIDGET->resid);
    return 0;
  }
  
  int seriala=1024;
  int serialc=0;
  void *serial=malloc(seriala);
  if (!serial) return -1;
  while (1) {
    if ((serialc=ps_blueprint_encode(serial,seriala,WIDGET->blueprint))<0) {
      ps_log(EDIT,ERROR,"Failed to encode blueprint!");
      free(serial);
      return 0;
    }
    if (serialc<=seriala) break;
    free(serial);
    if (!(serial=malloc(seriala=serialc))) return -1;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to write %d-byte encoded blueprint",path,serialc);
    free(serial);
    return 0;
  }

  ps_log(EDIT,INFO,"%s: Wrote blueprint, %d bytes.",path,serialc);
  free(serial);
  return 0;
}

static int ps_editblueprint_cb_add_poi(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_blueprint_poi *poi=ps_blueprint_add_poi(WIDGET->blueprint);
  if (!poi) return 0;
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_add_solution(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_blueprint_add_solution(WIDGET->blueprint,1,8,1,255,0)<0) return -1;
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Edit monster counts.
 */

static int ps_editblueprint_cb_monsterclo_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int n;
  if (ps_widget_dialogue_get_number(&n,dialogue)<0) return 0;
  if ((n<0)||(n>255)||(n>WIDGET->blueprint->monsterc_max)) return 0;
  WIDGET->blueprint->monsterc_min=n;
  if (ps_widget_kill(dialogue)<0) return -1;
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_monsterchi_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int n;
  if (ps_widget_dialogue_get_number(&n,dialogue)<0) return 0;
  if ((n<0)||(n>255)||(n<WIDGET->blueprint->monsterc_min)) return 0;
  WIDGET->blueprint->monsterc_max=n;
  if (ps_widget_kill(dialogue)<0) return -1;
  if (ps_editblueprint_rebuild_metadata(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editblueprint_cb_monsterclo(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Minimum count of random monsters:",-1,
    WIDGET->blueprint->monsterc_min,
    ps_editblueprint_cb_monsterclo_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  
  return 0;
}

static int ps_editblueprint_cb_monsterchi(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Maximum count of random monsters:",-1,
    WIDGET->blueprint->monsterc_max,
    ps_editblueprint_cb_monsterchi_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  
  return 0;
}

/* Get blueprint.
 */
 
struct ps_blueprint *ps_widget_editblueprint_get_blueprint(const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editblueprint)) widget=widget->parent;
  if (!widget) return 0;
  return WIDGET->blueprint;
}

/* Highlight POI entries in metadata list for the given coordinates.
 */

static int ps_editblueprint_highlight_poi(struct ps_widget *metadata,int childp) {
  if ((childp<0)||(childp>=metadata->childc)) return -1;
  struct ps_widget *button=metadata->childv[childp];
  button->bgrgba=0xc0a060ff;
  return 0;
}

static int ps_editblueprint_unhighlight_poi(struct ps_widget *metadata,int childp) {
  if ((childp<0)||(childp>=metadata->childc)) return -1;
  struct ps_widget *button=metadata->childv[childp];
  button->bgrgba=0x808080ff;
  return 0;
}
 
int ps_widget_editblueprint_highlight_poi_for_cell(struct ps_widget *widget,int col,int row) {
  while (widget&&(widget->type!=&ps_widget_type_editblueprint)) widget=widget->parent;
  if (ps_editblueprint_obj_validate(widget)<0) return -1;
  struct ps_widget *metadata=ps_editblueprint_get_metadata(widget);
  int i=WIDGET->blueprint->poic; while (i-->0) {
    const struct ps_blueprint_poi *poi=WIDGET->blueprint->poiv+i;
    if ((poi->x==col)&&(poi->y==row)) {
      if (ps_editblueprint_highlight_poi(metadata,2+i)<0) return -1;
    } else {
      if (ps_editblueprint_unhighlight_poi(metadata,2+i)<0) return -1;
    }
  }
  return 0;
}
