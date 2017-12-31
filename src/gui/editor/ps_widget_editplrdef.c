/* ps_widget_editplrdef.c
 * Modal editor for plrdef resource.
 * plrdef objects are not reference-counted (bad Past Andy, bad!).
 * So we take a weak reference and trust the resource manager to keep it alive.
 *
 * Children:
 *  [0] menubar
 *  [1] headtilebutton
 *  [2] bodytilebutton
 *  [3] headontoptoggle
 *  [4] palettelist
 *  [5..20] skilltoggle
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "game/ps_plrdef.h"
#include "res/ps_resmgr.h"
#include "os/ps_fs.h"

static int ps_editplrdef_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_add_palette(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_head(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_body(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_headontop(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_skill(struct ps_widget *button,struct ps_widget *widget);
static int ps_editplrdef_cb_palette(struct ps_widget *scrolllist,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editplrdef {
  struct ps_widget hdr;
  struct ps_plrdef *plrdef;
  int resid;
};

#define WIDGET ((struct ps_widget_editplrdef*)widget)

/* Delete.
 */

static void _ps_editplrdef_del(struct ps_widget *widget) {
  // (plrdef) is weak
}

/* Initialize.
 */

static int _ps_editplrdef_init(struct ps_widget *widget) {

  widget->bgrgba=0xc08040ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1;
  if (!ps_widget_menubar_add_button(child,"Back",4,ps_callback(ps_editplrdef_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Save",4,ps_callback(ps_editplrdef_cb_save,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"+Palette",8,ps_callback(ps_editplrdef_cb_add_palette,0,widget))) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // headtilebutton
  if (ps_widget_button_set_icon(child,0x035f)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editplrdef_cb_head,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // bodytilebutton
  if (ps_widget_button_set_icon(child,0x035f)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editplrdef_cb_body,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_toggle))) return -1; // headontoptoggle
  if (ps_widget_toggle_set_text(child,"Head on top",-1)<0) return -1;
  if (ps_widget_toggle_set_callback(child,ps_callback(ps_editplrdef_cb_headontop,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1; // palettelist
  if (ps_widget_scrolllist_enable_selection(child,ps_callback(ps_editplrdef_cb_palette,0,widget))<0) return -1;

  int i=0; for (;i<16;i++) { // skilltoggle
    if (!(child=ps_widget_spawn(widget,&ps_widget_type_toggle))) return -1;
    if (ps_widget_toggle_set_icon(child,0x0130+i)<0) return -1;
    if (ps_widget_toggle_set_callback(child,ps_callback(ps_editplrdef_cb_skill,0,widget))<0) return -1;
  }

  return 0;
}

/* Structural helpers.
 */

static int ps_editplrdef_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editplrdef) return -1;
  if (widget->childc!=21) return -1;
  return 0;
}

static struct ps_widget *ps_editplrdef_get_menubar(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editplrdef_get_headtilebutton(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editplrdef_get_bodytilebutton(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_editplrdef_get_headontoptoggle(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_editplrdef_get_palettelist(const struct ps_widget *widget) { return widget->childv[4]; }

static struct ps_widget *ps_editplrdef_get_skilltoggle(const struct ps_widget *widget,int index) {
  if ((index<0)||(index>=16)) return 0;
  return widget->childv[5+index];
}

static int ps_editplrdef_get_skill_index_for_widget(const struct ps_widget *widget,const struct ps_widget *toggle) {
  int childp=ps_widget_get_index_of_child(widget,toggle);
  childp-=5;
  if ((childp<0)||(childp>=16)) return -1;
  return childp;
}

/* Pack.
 */

static int _ps_editplrdef_pack(struct ps_widget *widget) {
  if (ps_editplrdef_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editplrdef_get_menubar(widget);
  struct ps_widget *headtilebutton=ps_editplrdef_get_headtilebutton(widget);
  struct ps_widget *bodytilebutton=ps_editplrdef_get_bodytilebutton(widget);
  struct ps_widget *headontoptoggle=ps_editplrdef_get_headontoptoggle(widget);
  struct ps_widget *palettelist=ps_editplrdef_get_palettelist(widget);
  int chw,chh,i;

  /* Menubar at the top, full width. */
  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  /* Skill toggles directly below menu bar in a row, packed and centered. */
  int skillsw=PS_TILESIZE*16;
  int skillsx=(widget->w>>1)-(skillsw>>1);
  int skillsy=menubar->h+5;
  for (i=0;i<16;i++) {
    struct ps_widget *toggle=ps_editplrdef_get_skilltoggle(widget,i);
    if (!toggle) return -1;
    toggle->x=skillsx+i*PS_TILESIZE;
    toggle->y=skillsy;
    toggle->w=PS_TILESIZE;
    toggle->h=PS_TILESIZE;
  }

  /* Head and body tile buttons left, with a modest margin. */
  headtilebutton->x=5;
  headtilebutton->y=menubar->h+PS_TILESIZE+10;
  headtilebutton->w=PS_TILESIZE+4;
  headtilebutton->h=PS_TILESIZE+4;
  bodytilebutton->x=headtilebutton->x;
  bodytilebutton->y=headtilebutton->y+headtilebutton->h+5;
  bodytilebutton->w=headtilebutton->w;
  bodytilebutton->h=headtilebutton->h;

  /* Head-on-top toggle right of tile buttons. */
  if (ps_widget_measure(&chw,&chh,headontoptoggle,widget->w,widget->h)<0) return -1;
  headontoptoggle->x=headtilebutton->x+headtilebutton->w+5;
  headontoptoggle->y=headtilebutton->y;
  headontoptoggle->w=chw;
  headontoptoggle->h=chh;

  /* Snuggle palette list into the elbow of tile buttons and head-on-top toggle. Take remaining space. */
  palettelist->x=headontoptoggle->x;
  palettelist->y=headontoptoggle->y+headontoptoggle->h+5;
  palettelist->w=widget->w-palettelist->x-5;
  palettelist->h=widget->h-palettelist->y-5;
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editplrdef={

  .name="editplrdef",
  .objlen=sizeof(struct ps_widget_editplrdef),

  .del=_ps_editplrdef_del,
  .init=_ps_editplrdef_init,

  .pack=_ps_editplrdef_pack,

};

/* Populate UI from model.
 */

static int ps_editplrdef_populate_ui_from_model(struct ps_widget *widget) {
  if (!WIDGET->plrdef) return -1;
  struct ps_widget *headtilebutton=ps_editplrdef_get_headtilebutton(widget);
  struct ps_widget *bodytilebutton=ps_editplrdef_get_bodytilebutton(widget);
  struct ps_widget *headontoptoggle=ps_editplrdef_get_headontoptoggle(widget);
  struct ps_widget *palettelist=ps_editplrdef_get_palettelist(widget);
  int i;

  /* Tile buttons are easy. */
  if (ps_widget_button_set_icon(headtilebutton,0x0300|WIDGET->plrdef->tileid_head)<0) return -1;
  if (ps_widget_button_set_icon(bodytilebutton,0x0300|WIDGET->plrdef->tileid_body)<0) return -1;
  if (ps_widget_toggle_set_value(headontoptoggle,WIDGET->plrdef->head_on_top_always)<0) return -1;
  
  /* Wipe the palette list and rebuild it. */
  const struct ps_plrdef_palette *pal=WIDGET->plrdef->palettev;
  if (ps_widget_remove_all_children(palettelist)<0) return -1;
  for (i=0;i<WIDGET->plrdef->palettec;i++,pal++) {
    struct ps_widget *palette=ps_widget_spawn(palettelist,&ps_widget_type_palette);
    if (!palette) return -1;
    if (ps_widget_palette_setup(palette,pal->rgba_head,pal->rgba_body)<0) return -1;
  }

  for (i=0;i<16;i++) {
    struct ps_widget *toggle=ps_editplrdef_get_skilltoggle(widget,i);
    if (!toggle) return -1;
    if (ps_widget_toggle_set_value(toggle,WIDGET->plrdef->skills&(1<<i))<0) return -1;
  }
  
  return 0;
}

/* Dismiss.
 */
 
static int ps_editplrdef_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Save.
 */

static int ps_editplrdef_cb_save(struct ps_widget *button,struct ps_widget *widget) {
  ps_log(EDIT,TRACE,"%s",__func__);

  char path[1024];
  int pathc=ps_res_get_path_for_resource(path,sizeof(path),PS_RESTYPE_PLRDEF,WIDGET->resid,0);
  if ((pathc<0)||(pathc>=sizeof(path))) {
    ps_log(EDIT,ERROR,"Failed to compose path for plrdef:%d",WIDGET->resid);
    return 0;
  }

  void *serial=0;
  int serialc=ps_plrdef_encode(&serial,WIDGET->plrdef);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to encode plrdef.");
    return 0;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"%s: Failed to write plrdef",path);
    free(serial);
    return 0;
  }

  ps_log(EDIT,INFO,"%s: Wrote plrdef:%d, %d bytes.",path,WIDGET->resid,serialc);
  free(serial);
  
  return 0;
}

/* Add palette.
 */

static int ps_editplrdef_cb_add_palette(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_plrdef_add_palette(WIDGET->plrdef,0x808080ff,0x808080ff)<0) return -1;
  struct ps_widget *palettelist=ps_editplrdef_get_palettelist(widget);
  struct ps_widget *palette=ps_widget_spawn(palettelist,&ps_widget_type_palette);
  if (!palette) return -1;
  if (ps_widget_palette_setup(palette,0x808080ff,0x808080ff)<0) return -1;
  if (ps_widget_pack(palettelist)<0) return -1;
  return 0;
}

/* Edit tiles.
 */

static int ps_editplrdef_head_ok(struct ps_widget *tileselect,struct ps_widget *widget) {
  WIDGET->plrdef->tileid_head=ps_widget_tileselect_get_tileid(tileselect);
  struct ps_widget *button=ps_editplrdef_get_headtilebutton(widget);
  if (ps_widget_button_set_icon(button,0x0300|WIDGET->plrdef->tileid_head)<0) return -1;
  return 0;
}

static int ps_editplrdef_cb_head(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *tileselect=ps_widget_spawn(root,&ps_widget_type_tileselect);
  if (!tileselect) return -1;
  if (ps_widget_tileselect_set_tileid(tileselect,0x0300|WIDGET->plrdef->tileid_head)<0) return -1;
  if (ps_widget_tileselect_set_callback(tileselect,ps_callback(ps_editplrdef_head_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

static int ps_editplrdef_body_ok(struct ps_widget *tileselect,struct ps_widget *widget) {
  WIDGET->plrdef->tileid_body=ps_widget_tileselect_get_tileid(tileselect);
  struct ps_widget *button=ps_editplrdef_get_bodytilebutton(widget);
  if (ps_widget_button_set_icon(button,0x0300|WIDGET->plrdef->tileid_body)<0) return -1;
  return 0;
}

static int ps_editplrdef_cb_body(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *tileselect=ps_widget_spawn(root,&ps_widget_type_tileselect);
  if (!tileselect) return -1;
  if (ps_widget_tileselect_set_tileid(tileselect,0x0300|WIDGET->plrdef->tileid_body)<0) return -1;
  if (ps_widget_tileselect_set_callback(tileselect,ps_callback(ps_editplrdef_body_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Toggle head_on_top_always.
 */

static int ps_editplrdef_cb_headontop(struct ps_widget *button,struct ps_widget *widget) {
  WIDGET->plrdef->head_on_top_always=ps_widget_toggle_get_value(button);
  return 0;
}

/* Toggle skill bit.
 */

static int ps_editplrdef_cb_skill(struct ps_widget *button,struct ps_widget *widget) {
  int index=ps_editplrdef_get_skill_index_for_widget(widget,button);
  if (index<0) return 0;
  uint16_t mask=1<<index;
  if (ps_widget_toggle_get_value(button)) {
    WIDGET->plrdef->skills|=mask;
  } else {
    WIDGET->plrdef->skills&=~mask;
  }
  return 0;
}

/* Open palette editor.
 */

static int ps_editplrdef_cb_palette_ok(struct ps_widget *editpalette,struct ps_widget *widget) {
  int palettep=ps_widget_editpalette_get_refnum1(editpalette);
  if ((palettep<0)||(palettep>=WIDGET->plrdef->palettec)) return 0;
  
  WIDGET->plrdef->palettev[palettep].rgba_head=ps_widget_editpalette_get_rgba_head(editpalette);
  WIDGET->plrdef->palettev[palettep].rgba_body=ps_widget_editpalette_get_rgba_body(editpalette);

  struct ps_widget *palettelist=ps_editplrdef_get_palettelist(widget);
  if (palettep<palettelist->childc) {
    struct ps_widget *palette=palettelist->childv[palettep];
    if (ps_widget_palette_setup(palette,WIDGET->plrdef->palettev[palettep].rgba_head,WIDGET->plrdef->palettev[palettep].rgba_body)<0) return -1;
  }
  
  return 0;
}

static int ps_editplrdef_cb_palette(struct ps_widget *palettelist,struct ps_widget *widget) {
  int palettep=ps_widget_scrolllist_get_selection(palettelist);
  if ((palettep<0)||(palettep>=WIDGET->plrdef->palettec)) return 0;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *editpalette=ps_widget_spawn(root,&ps_widget_type_editpalette);
  if (!editpalette) return -1;

  if (ps_widget_editpalette_setup(
    editpalette,
    WIDGET->plrdef->tileid_head,WIDGET->plrdef->tileid_body,
    WIDGET->plrdef->palettev[palettep].rgba_head,WIDGET->plrdef->palettev[palettep].rgba_body,
    ps_callback(ps_editplrdef_cb_palette_ok,0,widget)
  )<0) return -1;
  if (ps_widget_editpalette_set_refnum1(editpalette,palettep)<0) return -1;

  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Set resource.
 */

int ps_widget_editplrdef_set_resource(struct ps_widget *widget,int id,struct ps_plrdef *plrdef,const char *name) {
  if (ps_editplrdef_obj_validate(widget)<0) return -1;
  WIDGET->resid=id;
  WIDGET->plrdef=plrdef;
  if (ps_widget_menubar_set_title(ps_editplrdef_get_menubar(widget),name,-1)<0) return -1;
  if (ps_editplrdef_populate_ui_from_model(widget)<0) return -1;
  return 0;
}
