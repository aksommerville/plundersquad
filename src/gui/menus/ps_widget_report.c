/* ps_widget_report.c
 * Final report, at game over.
 *
 * Children:
 *  [0] header "Game over"
 *  [1] discussion
 *
 * TODO Per-player stats and awards.
 * TODO Show all the treasures collected.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "game/ps_game.h"
#include "util/ps_buffer.h"

/* Object definition.
 */

struct ps_widget_report {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_report*)widget)

/* Delete.
 */

static void _ps_report_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_report_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
  if (ps_widget_label_set_text(child,"GAME OVER",9)<0) return -1;
  if (ps_widget_label_set_size(child,24)<0) return -1;
  child->fgrgba=0xffe0c0ff;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_textblock))) return -1;
  child->fgrgba=0xe0e0e0ff;

  return 0;
}

/* Structural helpers.
 */

static int ps_report_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_report) return -1;
  if (widget->childc!=2) return -1;
  return 0;
}

static struct ps_widget *ps_report_get_header(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_report_get_discussion(const struct ps_widget *widget) {
  return widget->childv[1];
}

/* Measure.
 */

static int _ps_report_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (ps_report_obj_validate(widget)<0) return -1;
  struct ps_widget *header=ps_report_get_header(widget);
  struct ps_widget *discussion=ps_report_get_discussion(widget);
  int chw,chh;

  *w=*h=0;

  if (ps_widget_measure(&chw,&chh,header,maxw,maxh)<0) return -1;
  if (chw>*w) *w=chw;
  (*h)+=chh;

  if (ps_widget_measure(&chw,&chh,discussion,maxw,maxh-*h)<0) return -1;
  if (chw>*w) *w=chw;
  (*h)+=chh;
  
  return 0;
}

/* Pack.
 */

static int _ps_report_pack(struct ps_widget *widget) {
  if (ps_report_obj_validate(widget)<0) return -1;
  struct ps_widget *header=ps_report_get_header(widget);
  struct ps_widget *discussion=ps_report_get_discussion(widget);
  int chw,chh;

  /* Header gets its preferred height and the full width, at the top. */
  if (ps_widget_measure(&chw,&chh,header,widget->w,widget->h)<0) return -1;
  header->x=0;
  header->y=0;
  header->w=widget->w;
  header->h=chh;

  /* Discussion gets centered in the remainder. */
  if (ps_widget_measure(&chw,&chh,discussion,widget->w,widget->h-header->h)<0) return -1;
  discussion->x=(widget->w>>1)-(chw>>1);
  discussion->y=header->h+((widget->h-header->h)>>1)-(chh>>1);
  discussion->w=chw;
  discussion->h=chh;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_report={

  .name="report",
  .objlen=sizeof(struct ps_widget_report),

  .del=_ps_report_del,
  .init=_ps_report_init,

  .measure=_ps_report_measure,
  .pack=_ps_report_pack,

};

/* Generate report text.
 */

static int ps_report_compose_text(struct ps_buffer *buffer,const struct ps_game *game) {

  int playtime_f=game->playtime%60;
  int playtime_ms=(playtime_f*1000)/60;
  int playtime_s=game->playtime/60;
  int playtime_m=playtime_s/60;
  playtime_s%=60;
  int playtime_h=playtime_m/60;
  playtime_m%=60;
  if (playtime_h) {
    if (ps_buffer_appendf(buffer,"Active time: %d:%02d:%02d.%03d\n",playtime_h,playtime_m,playtime_s,playtime_ms)<0) return -1;
  } else {
    if (ps_buffer_appendf(buffer,"Active time: %d:%02d.%03d\n",playtime_m,playtime_s,playtime_ms)<0) return -1;
  }

  if (ps_buffer_appendf(buffer,"Difficulty: %d\n",game->difficulty)<0) return -1;
  if (ps_buffer_appendf(buffer,"Length: %d\n",game->length)<0) return -1;

  return 0;
}

/* Generate report text and apply to UI.
 */
 
int ps_widget_report_generate(struct ps_widget *widget,const struct ps_game *game) {
  if (ps_report_obj_validate(widget)<0) return -1;
  if (!game) return -1;
  struct ps_widget *discussion=ps_report_get_discussion(widget);
  struct ps_buffer buffer={0};
  if (ps_report_compose_text(&buffer,game)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  if (ps_widget_textblock_set_text(discussion,buffer.v,buffer.c)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  ps_buffer_cleanup(&buffer);
  return 0;
}
