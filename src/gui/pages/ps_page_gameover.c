#include "ps.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "input/ps_input.h"
#include "util/ps_geometry.h"

/* Page definition.
 */

struct ps_page_gameover {
  struct ps_page hdr;
  struct ps_widget *menu;
};

#define PAGE ((struct ps_page_gameover*)page)

/* Delete.
 */

static void _ps_gameover_del(struct ps_page *page) {
  ps_widget_del(PAGE->menu);
}

/* Initialize.
 */

static int _ps_gameover_init(struct ps_page *page) {

  page->root->bgrgba=0x208040c0;

  struct ps_widget *packer=ps_widget_spawn(page->root,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,PS_TILESIZE)<0) return -1;

  if (!ps_widget_spawn_label(packer,"You win!",-1,0xffffffff)) return -1;
  
  PAGE->menu=ps_widget_spawn(packer,&ps_widget_type_menu);
  if (ps_widget_ref(PAGE->menu)<0) return -1;

  struct ps_widget *option;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Main menu",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Quit",-1))) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_gameover_move_cursor(struct ps_page *page,int dx,int dy) {
  if (dx) {
    if (ps_widget_menu_adjust_selection(PAGE->menu,dx)<0) return -1;
  }
  if (dy) {
    if (ps_widget_menu_move_cursor(PAGE->menu,dy)<0) return -1;
  }
  return 0;
}

static int _ps_gameover_activate(struct ps_page *page) {
  switch (ps_widget_menu_get_cursor(PAGE->menu)) {
    case 0: { // Main menu
        if (ps_gui_load_page_assemble(page->gui)<0) return -1;
      } break;
    case 1: { // Quit
        if (ps_input_request_termination()<0) return -1;
      } break;
  }
  return 0;
}

static int _ps_gameover_submit(struct ps_page *page) {
  return 0;
}

static int _ps_gameover_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_gameover_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_gameover={
  .name="gameover",
  .objlen=sizeof(struct ps_page_gameover),
  .init=_ps_gameover_init,
  .del=_ps_gameover_del,

  .move_cursor=_ps_gameover_move_cursor,
  .activate=_ps_gameover_activate,
  .submit=_ps_gameover_submit,
  .cancel=_ps_gameover_cancel,

  .update=_ps_gameover_update,
};
