/* ps_widget_setuppage.c
 * Modal controller for the scenario generator config screen.
 * Users see this right after all players have joined.
 * When we finish, the game is ready to play.
 *
 * Children:
 *   [0] menu
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "ps_menus.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "input/ps_input.h"

static int ps_setuppage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_setuppage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_setuppage*)widget)

/* Delete.
 */

static void _ps_setuppage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_setuppage_init(struct ps_widget *widget) {
  struct ps_widget *child,*menu,*menupacker;

  widget->bgrgba=0x406020ff;

  if (!(menu=ps_widget_spawn(widget,&ps_widget_type_menu))) return -1;
  if (!(menupacker=ps_widget_menu_get_packer(menu))) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_setuppage_cb_menu,0,widget))<0) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Begin",5)) return -1;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=0x40404080;
  if (ps_widget_slider_set_variable_color(child,0x00ff00ff,0xff0000ff)<0) return -1;
  if (ps_widget_slider_set_text(child," Difficulty",-1)<0) return -1;
  if (ps_widget_slider_set_limits(child,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX)<0) return -1;
  if (ps_widget_slider_set_value(child,PS_DIFFICULTY_DEFAULT)<0) return -1;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=0x40404080;
  if (ps_widget_slider_set_variable_color(child,0x00ff00ff,0xff0000ff)<0) return -1;
  if (ps_widget_slider_set_text(child,"     Length",-1)<0) return -1;
  if (ps_widget_slider_set_limits(child,PS_LENGTH_MIN,PS_LENGTH_MAX)<0) return -1;
  if (ps_widget_slider_set_value(child,PS_LENGTH_DEFAULT)<0) return -1;
  
  if (!ps_widget_menu_spawn_label(menu,"Return to player selection",-1)) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Quit",4)) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_setuppage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_setuppage) return -1;
  if (widget->childc!=1) return -1;
  return 0;
}

static struct ps_widget *ps_setuppage_get_menu(const struct ps_widget *widget) {
  return widget->childv[0];
}

/* Pack.
 */

static int _ps_setuppage_pack(struct ps_widget *widget) {
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    child->x=(widget->w>>1)-(chw>>1);
    child->y=(widget->h>>1)-(chh>>1);
    child->w=chw;
    child->h=chh;
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Forward input to menu.
 */

static int _ps_setuppage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return ps_widget_userinput(ps_setuppage_get_menu(widget),plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_setuppage={

  .name="setuppage",
  .objlen=sizeof(struct ps_widget_setuppage),

  .del=_ps_setuppage_del,
  .init=_ps_setuppage_init,

  .pack=_ps_setuppage_pack,

  .userinput=_ps_setuppage_userinput,

};

/* Return to player select.
 */

static int ps_setuppage_return_to_assemble(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_widget_kill(widget)<0) return -1;
  if (ps_gui_load_page_assemble(gui)<0) return -1;
  return 0;
}

/* Read value of menu sliders.
 */

static int ps_setuppage_get_difficulty(const struct ps_widget *widget) {
  struct ps_widget *menu=ps_setuppage_get_menu(widget);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (menupacker->childc<2) return -1;
  struct ps_widget *slider=menupacker->childv[1];
  return ps_widget_slider_get_value(slider);
}

static int ps_setuppage_get_length(const struct ps_widget *widget) {
  struct ps_widget *menu=ps_setuppage_get_menu(widget);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (menupacker->childc<3) return -1;
  struct ps_widget *slider=menupacker->childv[2];
  return ps_widget_slider_get_value(slider);
}

/* Commit selections and proceed to the game.
 */

static int ps_setuppage_finish(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);

  if (ps_game_set_difficulty(game,ps_setuppage_get_difficulty(widget))<0) return -1;
  if (ps_game_set_length(game,ps_setuppage_get_length(widget))<0) return -1;
  if (ps_game_generate(game)<0) {
    ps_log(GUI,ERROR,"Failed to generate scenario.");
    return -1;
  }
  if (ps_game_restart(game)<0) {
    ps_log(GUI,ERROR,"Failed to start game.");
    return -1;
  }

  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_setuppage_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_setuppage_finish(widget);
    case 1: break; // difficulty
    case 2: break; // length
    case 3: return ps_setuppage_return_to_assemble(widget);
    case 4: return ps_input_request_termination();
  }
  return 0;
}
