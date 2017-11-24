#include "ps.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "game/ps_sound_effects.h"

/* Page definition.
 */

struct ps_page_edithome {
  struct ps_page hdr;
  struct ps_widget *menu;
};

#define PAGE ((struct ps_page_edithome*)page)

/* Delete.
 */

static void _ps_edithome_del(struct ps_page *page) {
  ps_widget_del(PAGE->menu);
}

/* Initialize.
 */

static int _ps_edithome_init(struct ps_page *page) {

  page->root->bgrgba=0x800020ff;
  
  PAGE->menu=ps_widget_spawn(page->root,&ps_widget_type_menu);
  if (ps_widget_ref(PAGE->menu)<0) return -1;

  struct ps_widget *option;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Sound effect",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Song XXX",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Blueprint XXX",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Sprite XXX",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Hero XXX",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Quit",-1))) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_edithome_move_cursor(struct ps_page *page,int dx,int dy) {
  if (dx) {
    if (ps_widget_menu_adjust_selection(PAGE->menu,dx)<0) return -1;
  }
  if (dy) {
    if (ps_widget_menu_move_cursor(PAGE->menu,dy)<0) return -1;
  }
  return 0;
}

static int _ps_edithome_activate(struct ps_page *page) {
  PS_SFX_GUI_ACTIVATE
  switch (ps_widget_menu_get_cursor(PAGE->menu)) {
    case 0: { // Sound effect
        if (ps_gui_load_page_editsfx(page->gui)<0) return -1;
      } break;
    case 1: { // Song
        if (ps_gui_load_page_editsong(page->gui)<0) return -1;
      } break;
    case 2: { // Blueprint
        if (ps_gui_load_page_editblueprint(page->gui)<0) return -1;
      } break;
    case 3: { // Sprite
        if (ps_gui_load_page_editsprdef(page->gui)<0) return -1;
      } break;
    case 4: { // hero
        if (ps_gui_load_page_editplrdef(page->gui)<0) return -1;
      } break;
    case 5: { // Quit
        if (ps_input_request_termination()<0) return -1;
      } break;
  }
  return 0;
}

static int _ps_edithome_submit(struct ps_page *page) {
  return 0;
}

static int _ps_edithome_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_edithome_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_edithome={
  .name="edithome",
  .objlen=sizeof(struct ps_page_edithome),
  .init=_ps_edithome_init,
  .del=_ps_edithome_del,

  .move_cursor=_ps_edithome_move_cursor,
  .activate=_ps_edithome_activate,
  .submit=_ps_edithome_submit,
  .cancel=_ps_edithome_cancel,

  .update=_ps_edithome_update,
};
