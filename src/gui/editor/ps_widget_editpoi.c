/* ps_widget_editpoi.c
 * Dialogue box for editing POI details, from blueprint editor.
 * Labels change dynamically as you modify values
 *
 * Children:
 *  [0] positionlabel (x and y fields)
 *  [1] typelabel
 *  [2] arg0label
 *  [3] arg1label
 *  [4] arg2label
 *  [5] xfield
 *  [6] yfield
 *  [7] typefield
 *  [8] typedesclabel (next to typefield)
 *  [9] arg0field
 *  [10] arg1field
 *  [11] arg2field
 *  [12] messagelabel -- static text with extra assessment
 *  [13] okbutton
 *  [14] cancelbutton
 *  [15] deletebutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_text.h"
#include "res/ps_resmgr.h"
#include "game/ps_sprite.h"

#define PS_EDITPOI_MARGIN 5
#define PS_EDITPOI_LABELS_RIGHT 100
#define PS_EDITPOI_FIELDS_LEFT 105
#define PS_EDITPOI_ROW_HEIGHT 16
#define PS_EDITPOI_FIELD_BGCOLOR_ERROR 0xff8080ff
#define PS_EDITPOI_FIELD_BGCOLOR_OK    0xffffffff

static int ps_editpoi_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_editpoi_cb_cancel(struct ps_widget *button,struct ps_widget *widget);
static int ps_editpoi_cb_delete(struct ps_widget *button,struct ps_widget *widget);
static int ps_editpoi_cb_text_changed(struct ps_widget *field,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editpoi {
  struct ps_widget hdr;
  struct ps_blueprint_poi poi;
  struct ps_callback cb;
  int valid;
  int index;
  struct ps_blueprint *blueprint;
  int please_delete;
};

#define WIDGET ((struct ps_widget_editpoi*)widget)

/* Delete.
 */

static void _ps_editpoi_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
  ps_blueprint_del(WIDGET->blueprint);
}

/* Initialize.
 */

static int _ps_editpoi_init(struct ps_widget *widget) {

  widget->bgrgba=0xa0c0a0ff;

  struct ps_widget *child;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // positionlabel
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // typelabel
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // arg0label
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // arg1label
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // arg2label
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // xfield
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // yfield
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // typefield
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // typedesclabel
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // arg0field
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // arg1field
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // arg2field
  if (ps_widget_field_set_cb_change(child,ps_callback(ps_editpoi_cb_text_changed,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // messagelabel
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // okbutton
  if (ps_widget_button_set_text(child,"OK",2)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editpoi_cb_ok,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // cancelbutton
  if (ps_widget_button_set_text(child,"Cancel",6)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editpoi_cb_cancel,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // deletebutton
  if (ps_widget_button_set_text(child,"Delete",6)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_editpoi_cb_delete,0,widget))<0) return -1;
  child->bgrgba=0xff0000ff;

  return 0;
}

/* Structural helpers.
 */

static int ps_editpoi_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editpoi) return -1;
  if (widget->childc!=16) return -1;
  return 0;
}

static struct ps_widget *ps_editpoi_get_positionlabel(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editpoi_get_typelabel(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editpoi_get_arg0label(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_editpoi_get_arg1label(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_editpoi_get_arg2label(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_editpoi_get_xfield(const struct ps_widget *widget) { return widget->childv[5]; }
static struct ps_widget *ps_editpoi_get_yfield(const struct ps_widget *widget) { return widget->childv[6]; }
static struct ps_widget *ps_editpoi_get_typefield(const struct ps_widget *widget) { return widget->childv[7]; }
static struct ps_widget *ps_editpoi_get_typedesclabel(const struct ps_widget *widget) { return widget->childv[8]; }
static struct ps_widget *ps_editpoi_get_arg0field(const struct ps_widget *widget) { return widget->childv[9]; }
static struct ps_widget *ps_editpoi_get_arg1field(const struct ps_widget *widget) { return widget->childv[10]; }
static struct ps_widget *ps_editpoi_get_arg2field(const struct ps_widget *widget) { return widget->childv[11]; }
static struct ps_widget *ps_editpoi_get_messagelabel(const struct ps_widget *widget) { return widget->childv[12]; }
static struct ps_widget *ps_editpoi_get_okbutton(const struct ps_widget *widget) { return widget->childv[13]; }
static struct ps_widget *ps_editpoi_get_cancelbutton(const struct ps_widget *widget) { return widget->childv[14]; }
static struct ps_widget *ps_editpoi_get_deletebutton(const struct ps_widget *widget) { return widget->childv[15]; }

/* Measure.
 * It would be a lot of tedious work to measure correctly.
 * Fuck it, let's just take a fixed size.
 */

static int _ps_editpoi_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SCREENW>>1;
  *h=PS_EDITPOI_ROW_HEIGHT*7+(PS_EDITPOI_MARGIN<<1);
  return 0;
}

/* Pack.
 */

static int _ps_editpoi_pack(struct ps_widget *widget) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  struct ps_widget *positionlabel=ps_editpoi_get_positionlabel(widget);
  struct ps_widget *typelabel=ps_editpoi_get_typelabel(widget);
  struct ps_widget *arg0label=ps_editpoi_get_arg0label(widget);
  struct ps_widget *arg1label=ps_editpoi_get_arg1label(widget);
  struct ps_widget *arg2label=ps_editpoi_get_arg2label(widget);
  struct ps_widget *xfield=ps_editpoi_get_xfield(widget);
  struct ps_widget *yfield=ps_editpoi_get_yfield(widget);
  struct ps_widget *typefield=ps_editpoi_get_typefield(widget);
  struct ps_widget *typedesclabel=ps_editpoi_get_typedesclabel(widget);
  struct ps_widget *arg0field=ps_editpoi_get_arg0field(widget);
  struct ps_widget *arg1field=ps_editpoi_get_arg1field(widget);
  struct ps_widget *arg2field=ps_editpoi_get_arg2field(widget);
  struct ps_widget *messagelabel=ps_editpoi_get_messagelabel(widget);
  struct ps_widget *okbutton=ps_editpoi_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_editpoi_get_cancelbutton(widget);
  struct ps_widget *deletebutton=ps_editpoi_get_deletebutton(widget);
  int chw,chh;
  int y=PS_EDITPOI_MARGIN;

  #define LEFT_LABEL(name) { \
    if (ps_widget_measure(&chw,&chh,name,widget->w,widget->h)<0) return -1; \
    name->x=PS_EDITPOI_LABELS_RIGHT-chw; \
    name->y=y; \
    name->w=chw; \
    name->h=PS_EDITPOI_ROW_HEIGHT; \
  }
  #define RIGHT_FIELD(name) { \
    name->x=PS_EDITPOI_FIELDS_LEFT; \
    name->y=y; \
    name->w=widget->w-PS_EDITPOI_MARGIN-name->x; \
    name->h=PS_EDITPOI_ROW_HEIGHT; \
  }
  
  LEFT_LABEL(positionlabel)
  int separator=PS_EDITPOI_FIELDS_LEFT+((widget->w-PS_EDITPOI_MARGIN-PS_EDITPOI_FIELDS_LEFT)>>1);
  xfield->x=PS_EDITPOI_FIELDS_LEFT;
  xfield->y=y;
  xfield->w=separator-2-xfield->x;
  xfield->h=PS_EDITPOI_ROW_HEIGHT;
  yfield->x=xfield->x+xfield->w+5;
  yfield->y=y;
  yfield->w=widget->w-PS_EDITPOI_MARGIN-yfield->x;
  yfield->h=PS_EDITPOI_ROW_HEIGHT;
  y+=PS_EDITPOI_ROW_HEIGHT;

  LEFT_LABEL(typelabel)
  typefield->x=PS_EDITPOI_FIELDS_LEFT;
  typefield->w=24;
  typefield->y=y;
  typefield->h=PS_EDITPOI_ROW_HEIGHT;
  typedesclabel->x=typefield->x+typefield->w+5;
  typedesclabel->y=y;
  typedesclabel->w=widget->w-PS_EDITPOI_MARGIN-typedesclabel->x;
  typedesclabel->h=PS_EDITPOI_ROW_HEIGHT;
  y+=PS_EDITPOI_ROW_HEIGHT;

  LEFT_LABEL(arg0label)
  RIGHT_FIELD(arg0field)
  y+=PS_EDITPOI_ROW_HEIGHT;

  LEFT_LABEL(arg1label)
  RIGHT_FIELD(arg1field)
  y+=PS_EDITPOI_ROW_HEIGHT;

  LEFT_LABEL(arg2label)
  RIGHT_FIELD(arg2field)
  y+=PS_EDITPOI_ROW_HEIGHT;

  messagelabel->x=PS_EDITPOI_MARGIN;
  messagelabel->y=y;
  messagelabel->w=widget->w-(PS_EDITPOI_MARGIN<<1);
  messagelabel->h=PS_EDITPOI_ROW_HEIGHT;
  y+=PS_EDITPOI_ROW_HEIGHT;

  int x=widget->w-PS_EDITPOI_ROW_HEIGHT;
  if (ps_widget_measure(&chw,&chh,okbutton,x,PS_EDITPOI_ROW_HEIGHT)<0) return -1;
  okbutton->x=x-chw;
  okbutton->y=y;
  okbutton->w=chw;
  okbutton->h=PS_EDITPOI_ROW_HEIGHT;
  x-=chw;
  if (ps_widget_measure(&chw,&chh,cancelbutton,x,PS_EDITPOI_ROW_HEIGHT)<0) return -1;
  cancelbutton->x=x-chw;
  cancelbutton->y=y;
  cancelbutton->w=chw;
  cancelbutton->h=PS_EDITPOI_ROW_HEIGHT;
  x-=chw+5;
  if (ps_widget_measure(&chw,&chh,deletebutton,x,PS_EDITPOI_ROW_HEIGHT)<0) return -1;
  deletebutton->x=x-chw;
  deletebutton->y=y;
  deletebutton->w=chw;
  deletebutton->h=PS_EDITPOI_ROW_HEIGHT;

  #undef LEFT_LABEL
  #undef RIGHT_FIELD
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Digested input events.
 */

static int _ps_editpoi_activate(struct ps_widget *widget) {
  if (!WIDGET->valid) return 0;
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int _ps_editpoi_cancel(struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editpoi={

  .name="editpoi",
  .objlen=sizeof(struct ps_widget_editpoi),

  .del=_ps_editpoi_del,
  .init=_ps_editpoi_init,

  .measure=_ps_editpoi_measure,
  .pack=_ps_editpoi_pack,

  .activate=_ps_editpoi_activate,
  .cancel=_ps_editpoi_cancel,

};

/* Get sprdef name.
 */

static int ps_editpoi_get_sprdef_name(char *dst,int dsta,int sprdefid) {
  if (!ps_res_get(PS_RESTYPE_SPRDEF,sprdefid)) {
    return ps_strcpy(dst,dsta,"Resource not found.",-1);
  }
  char buf[1024];
  int bufc=ps_res_get_path_for_resource(buf,sizeof(buf),PS_RESTYPE_SPRDEF,sprdefid,1);
  if (bufc<0) {
    return ps_strcpy(dst,dsta,"Failed to compose path.",-1);
  }
  if (bufc>=sizeof(buf)) {
    return ps_strcpy(dst,dsta,"Path too long.",-1);
  }
  int i=bufc; while (i-->0) {
    if (buf[i]=='/') {
      return ps_strcpy(dst,dsta,buf+i+1,bufc-i-1);
    }
  }
  return ps_strcpy(dst,dsta,buf,bufc);
}

/* Validate BARRIER POI.
 */

static int ps_editpoi_is_solid_foursquare(const uint8_t *cellv,int x,int y) {
  uint8_t n[9];
  int dstp=0,subx,suby;
  for (suby=-1;suby<=1;suby++) {
    int srcy=y+suby;
    if (srcy<0) srcy=0; else if (srcy>=PS_BLUEPRINT_ROWC) srcy=PS_BLUEPRINT_ROWC-1;
    for (subx=-1;subx<=1;subx++,dstp++) {
      int srcx=x+subx;
      if (srcx<0) srcx=0; else if (srcx>=PS_BLUEPRINT_COLC) srcx=PS_BLUEPRINT_COLC-1;
      n[dstp]=(cellv[srcy*PS_BLUEPRINT_COLC+srcx]==PS_BLUEPRINT_CELL_SOLID);
    }
  }
  if (n[0]&&n[1]&&n[3]&&n[4]) return 1;
  if (n[1]&&n[2]&&n[4]&&n[5]) return 1;
  if (n[3]&&n[4]&&n[6]&&n[7]) return 1;
  if (n[4]&&n[5]&&n[7]&&n[8]) return 1;
  return 0;
}

static int ps_editpoi_validate_barrier(const struct ps_widget *widget) {

  // If we don't have a blueprint, assume it's ok.
  if (!WIDGET->blueprint) return 1;

  // Invalid coordinates should not reach the model, but let's be sure.
  if ((WIDGET->poi.x>=PS_BLUEPRINT_COLC)||(WIDGET->poi.y>=PS_BLUEPRINT_ROWC)) return 0;

  // The underlying cell will typically be SOLID.
  // I'll only call it suspect if it's VACANT, which makes no sense at all.
  uint8_t cell=WIDGET->blueprint->cellv[WIDGET->poi.y*PS_BLUEPRINT_COLC+WIDGET->poi.x];
  if (cell==PS_BLUEPRINT_CELL_VACANT) return 0;

  // Here's the doozy: If it's part of a SOLID foursquare, the skinner might join it.
  if (cell==PS_BLUEPRINT_CELL_SOLID) {
    if (ps_editpoi_is_solid_foursquare(WIDGET->blueprint->cellv,WIDGET->poi.x,WIDGET->poi.y)) return 0;
  }

  // Can we also check for potential skinny joins? That's a bit more subtle...
  
  return 1;
}

/* Try to get sprite argument labels from the sprtype.
 */

static int ps_editpoi_populate_sprite_argument_labels(struct ps_widget *arg1label,struct ps_widget *arg2label,int sprdefid) {
  const char *name1=0,*name2=0;
  const struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
  if (sprdef&&sprdef->type&&sprdef->type->get_configure_argument_name) {
    name1=sprdef->type->get_configure_argument_name(0);
    name2=sprdef->type->get_configure_argument_name(1);
  }
  if (ps_widget_label_set_text(arg1label,name1?name1:"sprtype arg 0",-1)<0) return -1;
  if (ps_widget_label_set_text(arg2label,name2?name2:"sprtype arg 1",-1)<0) return -1;
  return 0;
}

/* Is this a home screen?
 */

static int ps_editpoi_is_home_screen(const struct ps_widget *widget) {
  const struct ps_blueprint_poi *poi=WIDGET->blueprint->poiv;
  int i=WIDGET->blueprint->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_HERO) return 1;
  }
  return 0;
}

/* Is this grid position ok for a status report?
 */

static int ps_editpoi_4_statusreports(const uint8_t *cellv) {
  if (cellv[0]!=PS_BLUEPRINT_CELL_STATUSREPORT) return 0;
  if (cellv[1]!=PS_BLUEPRINT_CELL_STATUSREPORT) return 0;
  if (cellv[2]!=PS_BLUEPRINT_CELL_STATUSREPORT) return 0;
  if (cellv[3]!=PS_BLUEPRINT_CELL_STATUSREPORT) return 0;
  return 1;
}

static int ps_editpoi_ok_statusreport_position(const struct ps_widget *widget) {
  const struct ps_blueprint_poi *poi=&WIDGET->poi;
  if ((poi->x<0)||(poi->x>=PS_BLUEPRINT_COLC)) return 0;
  if ((poi->y<0)||(poi->y>=PS_BLUEPRINT_ROWC)) return 0;
  int x=poi->x,y=poi->y,w=1,h=1;
  if (WIDGET->blueprint->cellv[y*PS_BLUEPRINT_COLC+x]!=PS_BLUEPRINT_CELL_STATUSREPORT) return 0;
  while ((x>0)&&(WIDGET->blueprint->cellv[y*PS_BLUEPRINT_COLC+(x-1)]==PS_BLUEPRINT_CELL_STATUSREPORT)) { x--; w++; }
  while ((x+w<PS_GRID_COLC)&&(WIDGET->blueprint->cellv[y*PS_BLUEPRINT_COLC+x+w]==PS_BLUEPRINT_CELL_STATUSREPORT)) w++;
  if (w!=4) return 0;
  while ((y>0)&&ps_editpoi_4_statusreports(WIDGET->blueprint->cellv+(y-1)*PS_BLUEPRINT_COLC+x)) { y--; h++; }
  while ((y+h<PS_GRID_ROWC)&&ps_editpoi_4_statusreports(WIDGET->blueprint->cellv+(y+h)*PS_BLUEPRINT_COLC+x)) h++;
  if (h!=3) return 0;
  return 1;
}

/* Refresh UI from model.
 */

static int ps_editpoi_refresh_ui(struct ps_widget *widget,int populate_fields) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  struct ps_widget *positionlabel=ps_editpoi_get_positionlabel(widget);
  struct ps_widget *typelabel=ps_editpoi_get_typelabel(widget);
  struct ps_widget *arg0label=ps_editpoi_get_arg0label(widget);
  struct ps_widget *arg1label=ps_editpoi_get_arg1label(widget);
  struct ps_widget *arg2label=ps_editpoi_get_arg2label(widget);
  struct ps_widget *xfield=ps_editpoi_get_xfield(widget);
  struct ps_widget *yfield=ps_editpoi_get_yfield(widget);
  struct ps_widget *typefield=ps_editpoi_get_typefield(widget);
  struct ps_widget *typedesclabel=ps_editpoi_get_typedesclabel(widget);
  struct ps_widget *arg0field=ps_editpoi_get_arg0field(widget);
  struct ps_widget *arg1field=ps_editpoi_get_arg1field(widget);
  struct ps_widget *arg2field=ps_editpoi_get_arg2field(widget);
  struct ps_widget *messagelabel=ps_editpoi_get_messagelabel(widget);

  if (ps_widget_label_set_text(positionlabel,"Position",-1)<0) return -1;
  if (ps_widget_label_set_text(typelabel,"Type",-1)<0) return -1;
  
  if (populate_fields) {
    if (ps_widget_field_set_integer(xfield,WIDGET->poi.x)<0) return -1;
    if (ps_widget_field_set_integer(yfield,WIDGET->poi.y)<0) return -1;
    if (ps_widget_field_set_integer(typefield,WIDGET->poi.type)<0) return -1;
    if (ps_widget_field_set_integer(arg0field,WIDGET->poi.argv[0])<0) return -1;
    if (ps_widget_field_set_integer(arg1field,WIDGET->poi.argv[1])<0) return -1;
    if (ps_widget_field_set_integer(arg2field,WIDGET->poi.argv[2])<0) return -1;
  }

  switch (WIDGET->poi.type) {
    case PS_BLUEPRINT_POI_NOOP: {
        if (ps_widget_label_set_text(typedesclabel,"NOOP",4)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(messagelabel,"This POI will be ignored.",-1)<0) return -1;
      } break;
    case PS_BLUEPRINT_POI_SPRITE: {
        if (ps_widget_label_set_text(typedesclabel,"SPRITE",6)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"sprdef ID",-1)<0) return -1;
        int sprdefid;
        if (ps_widget_field_get_integer(&sprdefid,arg0field)<0) sprdefid=-1;
        if (ps_editpoi_populate_sprite_argument_labels(arg1label,arg2label,sprdefid)<0) return -1;
        char buf[64];
        int bufc=ps_editpoi_get_sprdef_name(buf,sizeof(buf),WIDGET->poi.argv[0]);
        if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
        if (ps_widget_label_set_text(messagelabel,buf,bufc)<0) return -1;
      } break;
    case PS_BLUEPRINT_POI_TREASURE: {
        if (ps_widget_label_set_text(typedesclabel,"TREASURE",8)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"Treasure ID",-1)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"[1] (unused)",-1)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"[2] (unused)",-1)<0) return -1;
        if (ps_widget_label_set_text(messagelabel,"",-1)<0) return -1;
      } break;
    case PS_BLUEPRINT_POI_HERO: {
        if (ps_widget_label_set_text(typedesclabel,"HERO",4)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"Player ID",-1)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"[0] (unused)",-1)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"[1] (unused)",-1)<0) return -1;
        if ((WIDGET->poi.argv[0]<1)||(WIDGET->poi.argv[0]>PS_PLAYER_LIMIT)) {
          if (ps_widget_label_set_text(messagelabel,"Invalid player ID.",-1)<0) return -1;
        } else {
          if (ps_widget_label_set_text(messagelabel,"",-1)<0) return -1;
        }
      } break;
    case PS_BLUEPRINT_POI_BARRIER: {
        if (ps_widget_label_set_text(typedesclabel,"BARRIER",7)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"ID",2)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"unused",6)<0) return -1;
        if (ps_editpoi_validate_barrier(widget)) {
          if (ps_widget_label_set_text(messagelabel,"Looks OK.",-1)<0) return -1;
        } else {
          if (ps_widget_label_set_text(messagelabel,"Possible barrier fault.",-1)<0) return -1;
        }
      } break;
    case PS_BLUEPRINT_POI_DEATHGATE: {
        if (ps_widget_label_set_text(typedesclabel,"DEATHGATE",9)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"Blocked tile",-1)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"Open tile",-1)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"unused",6)<0) return -1;
        if (WIDGET->poi.argv[0]==WIDGET->poi.argv[1]) {
          if (ps_widget_label_set_text(messagelabel,"No effect if same tiles.",-1)<0) return -1;
        } else {
          if (ps_widget_label_set_text(messagelabel,"Looks OK.",-1)<0) return -1;
        }
      } break;
    case PS_BLUEPRINT_POI_STATUSREPORT: {
        if (ps_widget_label_set_text(typedesclabel,"STATUSREPORT",12)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"unused",6)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"unused",6)<0) return -1;
        if (!ps_editpoi_is_home_screen(widget)) {
          if (ps_widget_label_set_text(messagelabel,"Use only on HOME screen.",-1)<0) return -1;
        } else if (!ps_editpoi_ok_statusreport_position(widget)) {
          if (ps_widget_label_set_text(messagelabel,"Must be in 4x3 STATUSREPORT cells.",-1)<0) return -1;
        } else {
          if (ps_widget_label_set_text(messagelabel,"",0)<0) return -1;
        }
      } break;
    default: {
        if (ps_widget_label_set_text(typedesclabel,"unknown",7)<0) return -1;
        if (ps_widget_label_set_text(arg0label,"argv[0]",7)<0) return -1;
        if (ps_widget_label_set_text(arg1label,"argv[1]",7)<0) return -1;
        if (ps_widget_label_set_text(arg2label,"argv[2]",7)<0) return -1;
        if (ps_widget_label_set_text(messagelabel,"Type unknown.",-1)<0) return -1;
      } break;
  }

  return 0;
}

/* Refresh model from UI.
 */

static int ps_editpoi_refresh_model(struct ps_widget *widget) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  struct ps_widget *xfield=ps_editpoi_get_xfield(widget);
  struct ps_widget *yfield=ps_editpoi_get_yfield(widget);
  struct ps_widget *typefield=ps_editpoi_get_typefield(widget);
  struct ps_widget *arg0field=ps_editpoi_get_arg0field(widget);
  struct ps_widget *arg1field=ps_editpoi_get_arg1field(widget);
  struct ps_widget *arg2field=ps_editpoi_get_arg2field(widget);
  int n;

  WIDGET->valid=1;

  #define READFIELD(fieldname,poimember,lo,hi) { \
    if ( \
      (ps_widget_field_get_integer(&n,fieldname)<0)|| \
      (n<lo)||(n>hi) \
    ) { \
      fieldname->bgrgba=PS_EDITPOI_FIELD_BGCOLOR_ERROR; \
      WIDGET->valid=0; \
    } else { \
      fieldname->bgrgba=PS_EDITPOI_FIELD_BGCOLOR_OK; \
      WIDGET->poi.poimember=n; \
    } \
  }
  READFIELD(xfield,x,0,PS_BLUEPRINT_COLC-1)
  READFIELD(yfield,y,0,PS_BLUEPRINT_ROWC-1)
  READFIELD(typefield,type,0,255)
  READFIELD(arg0field,argv[0],INT_MIN,INT_MAX)
  READFIELD(arg1field,argv[1],INT_MIN,INT_MAX)
  READFIELD(arg2field,argv[2],INT_MIN,INT_MAX)
  #undef READFIELD
  
  return 0;
}

/* Accessors.
 */
 
int ps_widget_editpoi_set_poi(struct ps_widget *widget,const struct ps_blueprint_poi *poi) {
  if (!poi) return -1;
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  memcpy(&WIDGET->poi,poi,sizeof(struct ps_blueprint_poi));
  if (ps_editpoi_refresh_ui(widget,1)<0) return -1;
  WIDGET->valid=1;
  return 0;
}

const struct ps_blueprint_poi *ps_widget_editpoi_get_poi(const struct ps_widget *widget) {
  if (ps_editpoi_obj_validate(widget)<0) return 0;
  return &WIDGET->poi;
}

int ps_widget_editpoi_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

int ps_widget_editpoi_set_index(struct ps_widget *widget,int index) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  WIDGET->index=index;
  return 0;
}

int ps_widget_editpoi_get_index(const struct ps_widget *widget) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  return WIDGET->index;
}

int ps_widget_editpoi_set_blueprint(struct ps_widget *widget,struct ps_blueprint *blueprint) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  if (ps_blueprint_ref(blueprint)<0) return -1;
  ps_blueprint_del(WIDGET->blueprint);
  WIDGET->blueprint=blueprint;
  return 0;
}

int ps_widget_editpoi_wants_deleted(const struct ps_widget *widget) {
  if (ps_editpoi_obj_validate(widget)<0) return -1;
  return WIDGET->please_delete;
}

/* Callbacks.
 */
 
static int ps_editpoi_cb_ok(struct ps_widget *button,struct ps_widget *widget) {
  if (!WIDGET->valid) return 0;
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editpoi_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editpoi_cb_delete(struct ps_widget *button,struct ps_widget *widget) {
  WIDGET->please_delete=1;
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editpoi_cb_text_changed(struct ps_widget *field,struct ps_widget *widget) {
  if (ps_editpoi_refresh_model(widget)<0) return -1;
  if (ps_editpoi_refresh_ui(widget,0)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}
