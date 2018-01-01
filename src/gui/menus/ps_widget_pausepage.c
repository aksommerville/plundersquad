/* ps_widget_pausepage.c
 * Menu for special actions during game play.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "game/ps_game.h"

static int ps_pausepage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_pausepage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_pausepage*)widget)

/* Delete.
 */

static void _ps_pausepage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_pausepage_init(struct ps_widget *widget) {

  widget->bgrgba=0x000000c0;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (!menupacker) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_pausepage_cb_menu,0,widget))<0) return -1;

  struct ps_widget *thumb=ps_widget_menu_get_thumb(menu);
  if (!thumb) return -1;
  thumb->bgrgba=0x206080ff;

  struct ps_widget *label;
  const uint32_t textcolor=0xffffffff;
  if (!(label=ps_widget_menu_spawn_label(menu,"Resume",6))) return -1;
  label->fgrgba=textcolor;
  if (!(label=ps_widget_menu_spawn_label(menu,"To beginning",12))) return -1;
  label->fgrgba=textcolor;
  if (!(label=ps_widget_menu_spawn_label(menu,"Restart",7))) return -1;
  label->fgrgba=textcolor;
  if (!(label=ps_widget_menu_spawn_label(menu,"End game",8))) return -1;
  label->fgrgba=textcolor;
  if (!(label=ps_widget_menu_spawn_label(menu,"Quit",4))) return -1;
  label->fgrgba=textcolor;
  if (!(label=ps_widget_menu_spawn_label(menu,"[DEBUG] Heal all",-1))) return -1;
  label->fgrgba=0xff0000ff;
  
  return 0;
}

/* Pack.
 */

static int _ps_pausepage_pack(struct ps_widget *widget) {
  if (widget->childc!=1) return -1;
  struct ps_widget *menu=widget->childv[0];
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,menu,widget->w,widget->h)<0) return -1;
  menu->x=(widget->w>>1)-(chw>>1);
  menu->y=(widget->h>>1)-(chh>>1);
  menu->w=chw;
  menu->h=chh;
  if (ps_widget_pack(menu)<0) return -1;
  return 0;
}

/* Input.
 */

static int _ps_pausepage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (widget->childc!=1) return -1;
  return ps_widget_userinput(widget->childv[0],plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_pausepage={

  .name="pausepage",
  .objlen=sizeof(struct ps_widget_pausepage),

  .del=_ps_pausepage_del,
  .init=_ps_pausepage_init,

  .pack=_ps_pausepage_pack,

  .userinput=_ps_pausepage_userinput,

};

/* Dismiss menu.
 */

static int ps_pausepage_resume(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_game_toggle_pause(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Go to beginning of game.
 */

static int ps_pausepage_to_beginning(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_game_return_to_start_screen(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Restart game.
 */

static int ps_pausepage_restart(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_game_restart(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Cancel game, return to menu.
 */

static int ps_pausepage_cancel(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_assemble(gui)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Quit.
 */

static int ps_pausepage_quit(struct ps_widget *widget) {
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

/* Heal all (TEMP for testing only)
 */

static int ps_pausepage_heal_all(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_game_heal_all_heroes(game)<0) return -1;
  if (ps_game_toggle_pause(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_pausepage_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_pausepage_resume(widget);
    case 1: return ps_pausepage_to_beginning(widget);
    case 2: return ps_pausepage_restart(widget);
    case 3: return ps_pausepage_cancel(widget);
    case 4: return ps_pausepage_quit(widget);
    case 5: return ps_pausepage_heal_all(widget);
  }
  return 0;
}
