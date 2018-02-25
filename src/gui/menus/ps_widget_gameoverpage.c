/* ps_widget_gameoverpage.c
 * Post-game report and menu.
 * Children:
 *  [0] report
 *  [1] menu
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "input/ps_input.h"

static int ps_gameoverpage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_gameoverpage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_gameoverpage*)widget)

/* Delete.
 */

static void _ps_gameoverpage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_gameoverpage_init(struct ps_widget *widget) {

  widget->bgrgba=0x409060c0;

  struct ps_widget *report=ps_widget_spawn(widget,&ps_widget_type_report);
  if (!report) return -1;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  if (!menu) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_gameoverpage_cb_menu,0,widget))<0) return -1;
  struct ps_widget *item;
  if (!(item=ps_widget_menu_spawn_label(menu,"Main menu",9))) return -1;
  if (!(item=ps_widget_menu_spawn_label(menu,"Quit",4))) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_gameoverpage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_gameoverpage) return -1;
  if (widget->childc!=2) return -1;
  return 0;
}

static struct ps_widget *ps_gameoverpage_get_report(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_gameoverpage_get_menu(const struct ps_widget *widget) {
  return widget->childv[1];
}

/* Pack.
 */

static int _ps_gameoverpage_pack(struct ps_widget *widget) {
  if (ps_gameoverpage_obj_validate(widget)<0) return -1;
  struct ps_widget *report=ps_gameoverpage_get_report(widget);
  struct ps_widget *menu=ps_gameoverpage_get_menu(widget);
  int chw,chh;

  /* Report centered in left half of screen. */
  if (ps_widget_measure(&chw,&chh,report,widget->w>>1,widget->h)<0) return -1;
  report->x=(widget->w>>2)-(chw>>1);
  report->y=(widget->h>>1)-(chh>>1);
  report->w=chw;
  report->h=chh;

  /* Menu centered in right half of screen. */
  if (ps_widget_measure(&chw,&chh,menu,widget->w>>1,widget->h)<0) return -1;
  menu->x=(widget->w*3)/4-(chw>>1);
  menu->y=(widget->h>>1)-(chh>>1);
  menu->w=chw;
  menu->h=chh;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Forward input to menu.
 */

static int _ps_gameoverpage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  struct ps_widget *menu=ps_gameoverpage_get_menu(widget);
  return ps_widget_userinput(menu,plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_gameoverpage={

  .name="gameoverpage",
  .objlen=sizeof(struct ps_widget_gameoverpage),

  .del=_ps_gameoverpage_del,
  .init=_ps_gameoverpage_init,

  .pack=_ps_gameoverpage_pack,

  .userinput=_ps_gameoverpage_userinput,

};

/* Return to main menu.
 */

static int ps_gameoverpage_main_menu(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_widget_kill(widget)<0) return -1;
  if (ps_gui_load_page_assemble(gui)<0) return -1;
  return 0;
}

/* Quit.
 */

static int ps_gameoverpage_quit(struct ps_widget *widget) {
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_gameoverpage_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_gameoverpage_main_menu(widget);
    case 1: return ps_gameoverpage_quit(widget);
  }
  return 0;
}

/* Setup with game.
 */
 
int ps_widget_gameoverpage_setup(struct ps_widget *widget,const struct ps_game *game) {
  if (ps_gameoverpage_obj_validate(widget)<0) return -1;
  if (!game) return -1;
  struct ps_widget *report=ps_gameoverpage_get_report(widget);
  if (ps_widget_report_generate(report,game)<0) return -1;
  return 0;
}
