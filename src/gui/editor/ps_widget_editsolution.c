/* ps_widget_editosolution.c
 * Modal dialogue for editing one solution in blueprint.
 *
 * Children:
 *  [0] playerclolabel
 *  [1] playerchilabel
 *  [2] difficultylabel
 *  [3] preferencelabel
 *  [4] playerclofield
 *  [5] playerchifield
 *  [6] difficultyfield
 *  [7] preferencefield
 *  [8] okbutton
 *  [9] cancelbutton
 *  [10] deletebutton
 *  [11..26] bittoggle
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "scenario/ps_blueprint.h"

#define PS_EDITSOLUTION_WIDTH        200
#define PS_EDITSOLUTION_HEIGHT       132 /* 7 rows and 4 margins */
#define PS_EDITSOLUTION_MARGIN         5
#define PS_EDITSOLUTION_ROW_HEIGHT    16
#define PS_EDITSOLUTION_LABELS_RIGHT 120
#define PS_EDITSOLUTION_FIELDS_LEFT  125

static int ps_editsolution_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsolution_cb_cancel(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsolution_cb_delete(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsolution_cb_bit(struct ps_widget *toggle,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editsolution {
  struct ps_widget hdr;
  struct ps_blueprint_solution solution;
  int index;
  int please_delete;
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_editsolution*)widget)

/* Delete.
 */

static void _ps_editsolution_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_editsolution_init(struct ps_widget *widget) {

  widget->bgrgba=0xa0c0e0ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // playerclolabel
  if (ps_widget_label_set_text(child,"Min players",-1)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // playerchilabel
  if (ps_widget_label_set_text(child,"Max players",-1)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // difficultylabel
  if (ps_widget_label_set_text(child,"Difficulty",-1)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // preferencelabel
  if (ps_widget_label_set_text(child,"Preference",-1)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // playerclofield
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // playerchifield
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // difficultyfield
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // preferencefield

  if (!(child=ps_widget_button_spawn(widget,0,"OK",2,ps_callback(ps_editsolution_cb_ok,0,widget)))) return -1; // okbutton
  if (!(child=ps_widget_button_spawn(widget,0,"Cancel",6,ps_callback(ps_editsolution_cb_cancel,0,widget)))) return -1; // cancelbutton
  if (!(child=ps_widget_button_spawn(widget,0,"Delete",6,ps_callback(ps_editsolution_cb_delete,0,widget)))) return -1; // deletebutton
  child->bgrgba=0xff0000ff;

  int i=0; for (;i<16;i++) {
    if (!(child=ps_widget_spawn(widget,&ps_widget_type_toggle))) return -1;
    if (!ps_widget_toggle_set_icon(child,0x0130+i)) return -1;
    if (ps_widget_toggle_set_callback(child,ps_callback(ps_editsolution_cb_bit,0,widget))<0) return -1;
  }

  return 0;
}

/* Structural helpers.
 */

static int ps_editsolution_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editsolution) return -1;
  if (widget->childc!=27) return -1;
  return 0;
}

static struct ps_widget *ps_editsolution_get_playerclolabel(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editsolution_get_playerchilabel(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editsolution_get_difficultylabel(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_editsolution_get_preferencelabel(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_editsolution_get_playerclofield(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_editsolution_get_playerchifield(const struct ps_widget *widget) { return widget->childv[5]; }
static struct ps_widget *ps_editsolution_get_difficultyfield(const struct ps_widget *widget) { return widget->childv[6]; }
static struct ps_widget *ps_editsolution_get_preferencefield(const struct ps_widget *widget) { return widget->childv[7]; }
static struct ps_widget *ps_editsolution_get_okbutton(const struct ps_widget *widget) { return widget->childv[8]; }
static struct ps_widget *ps_editsolution_get_cancelbutton(const struct ps_widget *widget) { return widget->childv[9]; }
static struct ps_widget *ps_editsolution_get_deletebutton(const struct ps_widget *widget) { return widget->childv[10]; }

static struct ps_widget *ps_editsolution_get_toggle(const struct ps_widget *widget,int index) {
  if ((index<0)||(index>=16)) return 0;
  return widget->childv[11+index];
}

/* Measure.
 */

static int _ps_editsolution_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_EDITSOLUTION_WIDTH;
  *h=PS_EDITSOLUTION_HEIGHT;
  return 0;
}

/* Pack.
 */

static int _ps_editsolution_pack(struct ps_widget *widget) {
  if (ps_editsolution_obj_validate(widget)<0) return -1;
  struct ps_widget *playerclolabel=ps_editsolution_get_playerclolabel(widget);
  struct ps_widget *playerchilabel=ps_editsolution_get_playerchilabel(widget);
  struct ps_widget *difficultylabel=ps_editsolution_get_difficultylabel(widget);
  struct ps_widget *preferencelabel=ps_editsolution_get_preferencelabel(widget);
  struct ps_widget *playerclofield=ps_editsolution_get_playerclofield(widget);
  struct ps_widget *playerchifield=ps_editsolution_get_playerchifield(widget);
  struct ps_widget *difficultyfield=ps_editsolution_get_difficultyfield(widget);
  struct ps_widget *preferencefield=ps_editsolution_get_preferencefield(widget);
  struct ps_widget *okbutton=ps_editsolution_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_editsolution_get_cancelbutton(widget);
  struct ps_widget *deletebutton=ps_editsolution_get_deletebutton(widget);
  int chw,chh,i,y;

  y=PS_EDITSOLUTION_MARGIN;

  #define ROW(tag) { \
    if (ps_widget_measure(&chw,&chh,tag##label,PS_EDITSOLUTION_LABELS_RIGHT-PS_EDITSOLUTION_MARGIN,PS_EDITSOLUTION_ROW_HEIGHT)<0) return -1; \
    tag##label->x=PS_EDITSOLUTION_LABELS_RIGHT-chw; \
    tag##label->y=y; \
    tag##label->w=chw; \
    tag##label->h=PS_EDITSOLUTION_ROW_HEIGHT; \
    tag##field->x=PS_EDITSOLUTION_FIELDS_LEFT; \
    tag##field->y=y; \
    tag##field->w=widget->w-PS_EDITSOLUTION_MARGIN-tag##field->x; \
    tag##field->h=PS_EDITSOLUTION_ROW_HEIGHT; \
    y+=PS_EDITSOLUTION_ROW_HEIGHT; \
  }

  ROW(playerclo)
  ROW(playerchi)
  ROW(difficulty)
  ROW(preference)

  #undef ROW

  y+=PS_EDITSOLUTION_MARGIN;
  int togglesleft=(widget->w>>1)-(PS_EDITSOLUTION_ROW_HEIGHT*4);
  for (i=0;i<8;i++) {
    struct ps_widget *toggle;
    if (!(toggle=ps_editsolution_get_toggle(widget,i))) return -1;
    toggle->x=togglesleft+i*PS_EDITSOLUTION_ROW_HEIGHT;
    toggle->y=y;
    toggle->w=PS_EDITSOLUTION_ROW_HEIGHT;
    toggle->h=PS_EDITSOLUTION_ROW_HEIGHT;
    if (!(toggle=ps_editsolution_get_toggle(widget,8+i))) return -1;
    toggle->x=togglesleft+i*PS_EDITSOLUTION_ROW_HEIGHT;
    toggle->y=y+PS_EDITSOLUTION_ROW_HEIGHT;
    toggle->w=PS_EDITSOLUTION_ROW_HEIGHT;
    toggle->h=PS_EDITSOLUTION_ROW_HEIGHT;
  }
  y+=PS_EDITSOLUTION_ROW_HEIGHT<<1;

  y+=PS_EDITSOLUTION_MARGIN;
  int x=widget->w-PS_EDITSOLUTION_MARGIN;
  if (ps_widget_measure(&chw,&chh,okbutton,x,PS_EDITSOLUTION_ROW_HEIGHT)<0) return -1;
  x-=chw;
  okbutton->x=x;
  okbutton->y=y;
  okbutton->w=chw;
  okbutton->h=PS_EDITSOLUTION_ROW_HEIGHT;
  if (ps_widget_measure(&chw,&chh,cancelbutton,x,PS_EDITSOLUTION_ROW_HEIGHT)<0) return -1;
  x-=chw;
  cancelbutton->x=x;
  cancelbutton->y=y;
  cancelbutton->w=chw;
  cancelbutton->h=PS_EDITSOLUTION_ROW_HEIGHT;
  if (ps_widget_measure(&chw,&chh,deletebutton,x,PS_EDITSOLUTION_ROW_HEIGHT)<0) return -1;
  x-=chw+5;
  deletebutton->x=x;
  deletebutton->y=y;
  deletebutton->w=chw;
  deletebutton->h=PS_EDITSOLUTION_ROW_HEIGHT;
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Digested input events.
 */

static int _ps_editsolution_activate(struct ps_widget *widget) {
  return ps_editsolution_cb_ok(0,widget);
}

static int _ps_editsolution_cancel(struct ps_widget *widget) {
  return ps_editsolution_cb_cancel(0,widget);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsolution={

  .name="editsolution",
  .objlen=sizeof(struct ps_widget_editsolution),

  .del=_ps_editsolution_del,
  .init=_ps_editsolution_init,

  .measure=_ps_editsolution_measure,
  .pack=_ps_editsolution_pack,

  .activate=_ps_editsolution_activate,
  .cancel=_ps_editsolution_cancel,

};

/* Populate UI from model.
 */

static int ps_editsolution_populate_ui(struct ps_widget *widget) {
  struct ps_widget *playerclofield=ps_editsolution_get_playerclofield(widget);
  struct ps_widget *playerchifield=ps_editsolution_get_playerchifield(widget);
  struct ps_widget *difficultyfield=ps_editsolution_get_difficultyfield(widget);
  struct ps_widget *preferencefield=ps_editsolution_get_preferencefield(widget);

  if (ps_widget_field_set_integer(playerclofield,WIDGET->solution.plo)<0) return -1;
  if (ps_widget_field_set_integer(playerchifield,WIDGET->solution.phi)<0) return -1;
  if (ps_widget_field_set_integer(difficultyfield,WIDGET->solution.difficulty)<0) return -1;
  if (ps_widget_field_set_integer(preferencefield,WIDGET->solution.preference)<0) return -1;

  int i=0; for (;i<16;i++) {
    struct ps_widget *toggle=ps_editsolution_get_toggle(widget,i);
    if (!toggle) return -1;
    if (ps_widget_toggle_set_value(toggle,WIDGET->solution.skills&(1<<i))<0) return -1;
  }
  
  return 0;
}

/* Populate model from UI.
 */

static int ps_editsolution_populate_model(struct ps_widget *widget) {
  struct ps_widget *playerclofield=ps_editsolution_get_playerclofield(widget);
  struct ps_widget *playerchifield=ps_editsolution_get_playerchifield(widget);
  struct ps_widget *difficultyfield=ps_editsolution_get_difficultyfield(widget);
  struct ps_widget *preferencefield=ps_editsolution_get_preferencefield(widget);

  #define READFIELD(fieldname,modelname,lo,hi) { \
    int n; \
    if ( \
      (ps_widget_field_get_integer(&n,fieldname)<0)|| \
      (n<lo)||(n>hi) \
    ) { \
      return -1; \
    } else { \
      WIDGET->solution.modelname=n; \
    } \
  }
  READFIELD(playerclofield,plo,1,8)
  READFIELD(playerchifield,phi,1,8)
  READFIELD(difficultyfield,difficulty,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX)
  READFIELD(preferencefield,preference,0,255)
  #undef READFIELD

  if (WIDGET->solution.plo>WIDGET->solution.phi) return -1;

  WIDGET->solution.skills=0;
  int i=0; for (;i<16;i++) {
    struct ps_widget *toggle=ps_editsolution_get_toggle(widget,i);
    if (!toggle) return -1;
    if (ps_widget_toggle_get_value(toggle)) {
      WIDGET->solution.skills|=(1<<i);
    }
  }
  
  return 0;
}

/* Accessors.
 */
 
int ps_widget_editsolution_set_solution(struct ps_widget *widget,const struct ps_blueprint_solution *solution) {
  if (ps_editsolution_obj_validate(widget)<0) return -1;
  if (!solution) return -1;
  memcpy(&WIDGET->solution,solution,sizeof(struct ps_blueprint_solution));
  if (ps_editsolution_populate_ui(widget)<0) return -1;
  return 0;
}

const struct ps_blueprint_solution *ps_widget_editsolution_get_solution(const struct ps_widget *widget) {
  if (ps_editsolution_obj_validate(widget)<0) return 0;
  return &WIDGET->solution;
}

int ps_widget_editsolution_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (ps_editsolution_obj_validate(widget)<0) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

int ps_widget_editsolution_set_index(struct ps_widget *widget,int index) {
  if (ps_editsolution_obj_validate(widget)<0) return -1;
  WIDGET->index=index;
  return 0;
}

int ps_widget_editsolution_get_index(const struct ps_widget *widget) {
  if (ps_editsolution_obj_validate(widget)<0) return -1;
  return WIDGET->index;
}

int ps_widget_editsolution_wants_deleted(const struct ps_widget *widget) {
  if (ps_editsolution_obj_validate(widget)<0) return 0;
  return WIDGET->please_delete;
}

/* Callbacks.
 */
 
static int ps_editsolution_cb_ok(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_editsolution_populate_model(widget)<0) {
    ps_log(EDIT,ERROR,"Invalid solution data, aborting submission.");
    return 0;
  }
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editsolution_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editsolution_cb_delete(struct ps_widget *button,struct ps_widget *widget) {
  WIDGET->please_delete=1;
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editsolution_cb_bit(struct ps_widget *toggle,struct ps_widget *widget) {
  return 0;
}
