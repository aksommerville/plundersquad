#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"
#include "input/ps_input.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"

static int ps_sconfig_generate_scenario(struct ps_page *page);

/* Page definition.
 */

struct ps_page_sconfig {
  struct ps_page hdr;
  struct ps_widget *menu;
};

#define PAGE ((struct ps_page_sconfig*)page)

/* Delete.
 */

static void _ps_sconfig_del(struct ps_page *page) {
  ps_widget_del(PAGE->menu);
}

/* Initialize.
 */

static int _ps_sconfig_init(struct ps_page *page) {

  page->root->bgrgba=0x104020ff;

  struct ps_widget *packer=ps_widget_spawn(page->root,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,10)<0) return -1;

  struct ps_widget *label=ps_widget_spawn(packer,&ps_widget_type_label);
  if (!label) return -1;
  label->fgrgba=0xffff00ff;
  if (ps_widget_label_set_text(label,"Configure new game",-1)<0) return -1;

  PAGE->menu=ps_widget_spawn(packer,&ps_widget_type_menu);
  if (ps_widget_ref(PAGE->menu)<0) return -1;

  struct ps_widget *option;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Begin",-1))) return -1;
  if (!(option=ps_widget_menu_add_slider(PAGE->menu,"Difficulty ",-1,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX))) return -1;
  if (ps_widget_slider_set_value(option,PS_DIFFICULTY_DEFAULT)<0) return -1;
  if (!(option=ps_widget_menu_add_slider(PAGE->menu,"    Length ",-1,PS_LENGTH_MIN,PS_LENGTH_MAX))) return -1;
  if (ps_widget_slider_set_value(option,PS_LENGTH_DEFAULT)<0) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Return to player setup",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Quit",-1))) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_sconfig_move_cursor(struct ps_page *page,int dx,int dy) {
  if (dx) {
    if (ps_widget_menu_adjust_selection(PAGE->menu,dx)<0) return -1;
  }
  if (dy) {
    if (ps_widget_menu_move_cursor(PAGE->menu,dy)<0) return -1;
  }
  return 0;
}

static int _ps_sconfig_activate(struct ps_page *page) {
  switch (ps_widget_menu_get_cursor(PAGE->menu)) {
    case 0: { // Begin
        PS_SFX_GUI_ACTIVATE
        if (ps_sconfig_generate_scenario(page)<0) return -1;
        if (ps_gui_unload_page(page->gui)<0) return -1;
      } break;
    case 3: { // Back
        PS_SFX_GUI_ACTIVATE
        if (ps_gui_load_page_assemble(page->gui)<0) return -1;
      } break;
    case 4: { // Quit
        if (ps_input_request_termination()<0) return -1;
      } break;
  }
  return 0;
}

static int _ps_sconfig_submit(struct ps_page *page) {
  return 0;
}

static int _ps_sconfig_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_sconfig_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_sconfig={
  .name="sconfig",
  .objlen=sizeof(struct ps_page_sconfig),
  .init=_ps_sconfig_init,
  .del=_ps_sconfig_del,

  .move_cursor=_ps_sconfig_move_cursor,
  .activate=_ps_sconfig_activate,
  .submit=_ps_sconfig_submit,
  .cancel=_ps_sconfig_cancel,

  .update=_ps_sconfig_update,
};

/* Generate scenario.
 */
 
static int ps_sconfig_generate_scenario(struct ps_page *page) {
  struct ps_game *game=ps_gui_get_game(page->gui);
  if (!game) return -1;

  int difficulty=ps_widget_menu_get_slider(PAGE->menu,1);
  int length=ps_widget_menu_get_slider(PAGE->menu,2);

  if (ps_game_set_difficulty(game,difficulty)<0) return -1;
  if (ps_game_set_length(game,length)<0) return -1;
  if (ps_game_generate(game)<0) return -1;
  if (ps_game_restart(game)<0) return -1;
  
  return 0;
}
