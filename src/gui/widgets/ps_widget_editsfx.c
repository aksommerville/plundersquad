/* ps_widget_editsfx.c
 * Container for sound effects editor.
 * We contain an editsfxchan for each channel in the wavegen.
 */

#include "ps.h"
#include "gui/ps_gui.h"
#include "resedit/ps_iwg.h"
#include "video/ps_video.h"
#include "akau/akau.h"
#include "akgl/akgl.h"

#define PS_EDITSFX_BGCOLOR          0x406040ff
#define PS_EDITSFX_ROW_BGCOLOR_EVEN 0x208020ff
#define PS_EDITSFX_ROW_BGCOLOR_ODD  0x309030ff

/* Widget definition.
 */

struct ps_widget_editsfx {
  struct ps_widget hdr;
  struct akau_ipcm *ipcm;
  struct ps_iwg *iwg;
  char *path;
};

#define WIDGET ((struct ps_widget_editsfx*)widget)

/* Delete.
 */

static void _ps_editsfx_del(struct ps_widget *widget) {
  akau_ipcm_del(WIDGET->ipcm);
  ps_iwg_del(WIDGET->iwg);
  if (WIDGET->path) free(WIDGET->path);
}

/* Initialize.
 */

static int _ps_editsfx_init(struct ps_widget *widget) {

  widget->bgrgba=PS_EDITSFX_BGCOLOR;

  if (!(WIDGET->iwg=ps_iwg_new())) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_editsfx_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 * Doesn't matter; our parent gives us the whole screen, minus the menu bar.
 */

static int _ps_editsfx_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 * Each child gets the full width and an equal share of the height.
 * TODO: If there are too many rows, we should implement scrolling.
 */

static int _ps_editsfx_pack(struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  int rowh_base=widget->h/widget->childc;
  int rowh_extra=widget->h%widget->childc;
  int y=0;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    child->x=0;
    child->w=widget->w;
    child->y=y;
    child->h=rowh_base;
    if (rowh_extra) {
      child->h++;
      rowh_extra--;
    }
    y+=child->h;
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Mouse events.
 */

static int _ps_editsfx_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfx_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfx_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_editsfx_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_editsfx_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsfx={
  .name="editsfx",
  .objlen=sizeof(struct ps_widget_editsfx),
  .del=_ps_editsfx_del,
  .init=_ps_editsfx_init,
  .draw=_ps_editsfx_draw,
  .measure=_ps_editsfx_measure,
  .pack=_ps_editsfx_pack,
  .mouseenter=_ps_editsfx_mouseenter,
  .mouseexit=_ps_editsfx_mouseexit,
  .mousedown=_ps_editsfx_mousedown,
  .mouseup=_ps_editsfx_mouseup,
  .mousewheel=_ps_editsfx_mousewheel,
};

/* Rebuild UI based on IWG.
 */

static int ps_editsfx_rebuild(struct ps_widget *widget) {
  if (ps_widget_remove_all_children(widget)<0) return -1;
  if (!WIDGET->iwg) return 0;
  const struct ps_iwg_channel *chan=WIDGET->iwg->chanv;
  int chanid=0; for (;chanid<WIDGET->iwg->chanc;chanid++,chan++) {
    struct ps_widget *child=ps_widget_spawn(widget,&ps_widget_type_editsfxchan);
    if (!child) return -1;
    child->bgrgba=(chanid&1)?PS_EDITSFX_ROW_BGCOLOR_ODD:PS_EDITSFX_ROW_BGCOLOR_EVEN;
    if (ps_widget_editsfxchan_set_chanid(child,chanid)<0) return -1;
  }
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Set ipcm.
 */
 
int ps_widget_editsfx_set_ipcm(struct ps_widget *widget,struct akau_ipcm *ipcm) {
  if (!widget||(widget->type!=&ps_widget_type_editsfx)) return -1;
  if (ipcm&&(akau_ipcm_ref(ipcm)<0)) return -1;
  akau_ipcm_del(WIDGET->ipcm);
  WIDGET->ipcm=ipcm;
  return 0;
}

struct akau_ipcm *ps_widget_editsfx_get_ipcm(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsfx)) return 0;
  if (WIDGET->iwg&&WIDGET->iwg->dirty) {
    struct akau_ipcm *ipcm=ps_iwg_to_ipcm(WIDGET->iwg);
    if (!ipcm) return 0;
    WIDGET->iwg->dirty=0;
    akau_ipcm_del(WIDGET->ipcm);
    WIDGET->ipcm=ipcm;
  }
  return WIDGET->ipcm;
}

/* Set path.
 */

int ps_widget_editsfx_set_path(struct ps_widget *widget,const char *path) {
  if (!widget||(widget->type!=&ps_widget_type_editsfx)) return -1;
  if (WIDGET->path) free(WIDGET->path);
  if (path) {
    if (!(WIDGET->path=strdup(path))) return -1;
    if (ps_iwg_read_file(WIDGET->iwg,path)<0) {
      ps_log(RES,ERROR,"%s: Failed to load wave generator.",path);
    }
  } else {
    WIDGET->path=0;
    if (ps_iwg_clear(WIDGET->iwg)<0) return -1;
  }
  if (ps_editsfx_rebuild(widget)<0) return -1;
  return 0;
}

/* Get wavegen model.
 */
 
struct ps_iwg *ps_widget_editsfx_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  if (widget->type!=&ps_widget_type_editsfx) return 0;
  return WIDGET->iwg;
}
