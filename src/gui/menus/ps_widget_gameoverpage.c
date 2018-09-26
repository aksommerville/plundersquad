/* ps_widget_gameoverpage.c
 * Post-game report and menu.
 * Children:
 *  [0] report
 *  [1] menu
 *  [2] tshirt banner
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "input/ps_input.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "game/ps_sound_effects.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"

// (us) Must wait so long after construction before selecting from menu.
#define PS_GAMEOVERPAGE_INITIAL_INPUT_SUPPRESSION_TIME (1*1000000)

// Very important that difficulty and length here agree with setuppage -- otherwise we might break a promise!
#define PS_GAMEOVERPAGE_TSHIRT_ID         19
#define PS_GAMEOVERPAGE_TSHIRT_DIFFICULTY  5
#define PS_GAMEOVERPAGE_TSHIRT_LENGTH      3

static int ps_gameoverpage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_gameoverpage {
  struct ps_widget hdr;
  int64_t starttime;
};

#define WIDGET ((struct ps_widget_gameoverpage*)widget)

/* Delete.
 */

static void _ps_gameoverpage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_gameoverpage_init(struct ps_widget *widget) {

  WIDGET->starttime=ps_time_now();

  widget->bgrgba=0x40b060c0;

  struct ps_widget *report=ps_widget_spawn(widget,&ps_widget_type_report);
  if (!report) return -1;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  if (!menu) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_gameoverpage_cb_menu,0,widget))<0) return -1;
  struct ps_widget *item;
  if (!(item=ps_widget_menu_spawn_label(menu,"Main menu",9))) return -1;
  if (!(item=ps_widget_menu_spawn_label(menu,"Quit",4))) return -1;
  
  struct ps_widget *banner=ps_widget_spawn(widget,&ps_widget_type_texture);
  if (!banner) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_gameoverpage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_gameoverpage) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_gameoverpage_get_report(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_gameoverpage_get_menu(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_gameoverpage_get_banner(const struct ps_widget *widget) {
  return widget->childv[2];
}

/* Pack.
 */

static int _ps_gameoverpage_pack(struct ps_widget *widget) {
  if (ps_gameoverpage_obj_validate(widget)<0) return -1;
  struct ps_widget *report=ps_gameoverpage_get_report(widget);
  struct ps_widget *menu=ps_gameoverpage_get_menu(widget);
  struct ps_widget *banner=ps_gameoverpage_get_banner(widget);
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
  
  /* Banner centered in right half, whatever remains above menu. */
  if (ps_widget_measure(&chw,&chh,banner,128,128)<0) return -1;
  banner->x=(widget->w*3)/4-(chw>>1);
  banner->y=(menu->y>>1)-(chh>>1);
  banner->w=chw;
  banner->h=chh;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Test initial input suppression.
 */

static int ps_gameoverpage_initial_input_suppression_in_force(struct ps_widget *widget) {

  // Already checked, and suppression is expired.
  if (!WIDGET->starttime) return 0;

  int64_t now=ps_time_now();
  int64_t elapsed=now-WIDGET->starttime;
  if (elapsed>=PS_GAMEOVERPAGE_INITIAL_INPUT_SUPPRESSION_TIME) {
    WIDGET->starttime=0; // Signal future calls that we are no longer suppressed.
    return 0;
  }

  return 1;
}

/* Forward input to menu.
 */

static int _ps_gameoverpage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {

  if (ps_gameoverpage_initial_input_suppression_in_force(widget)) {
    return 0;
  }

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
  struct ps_userconfig *userconfig=ps_widget_get_userconfig(widget);
  if (userconfig&&ps_userconfig_get_int(userconfig,"kiosk",5)) {
    ps_log(GUI,INFO,"Ignoring quit request due to kiosk mode.");
    PS_SFX_GUI_REJECT
    return 0;
  }
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
  
  /* Generate report (left side of screen). */
  struct ps_widget *report=ps_gameoverpage_get_report(widget);
  if (ps_widget_report_generate(report,game)<0) return -1;
  
  /* Check for tshirt banner. */
  if (ps_userconfig_get_int(ps_widget_get_userconfig(widget),"tshirt",6)>0) {
    struct ps_widget *banner=ps_gameoverpage_get_banner(widget);
    if ((game->difficulty>=PS_GAMEOVERPAGE_TSHIRT_DIFFICULTY)&&(game->length>=PS_GAMEOVERPAGE_TSHIRT_LENGTH)) {
      struct ps_res_IMAGE *image=ps_res_get(PS_RESTYPE_IMAGE,PS_GAMEOVERPAGE_TSHIRT_ID);
      if (image) {
        if (ps_widget_texture_set_texture(banner,image->texture)<0) {
          ps_log(GUI,ERROR,"Failed to set tshirt victory banner texture.");
        }
      }
    }
  }
  
  return 0;
}
