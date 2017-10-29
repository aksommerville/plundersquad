#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"
#include "input/ps_input.h"

/* Page definition.
 */

struct ps_page_assemble {
  struct ps_page hdr;
  struct ps_widget *menu;
};

#define PAGE ((struct ps_page_assemble*)page)

/* Delete.
 */

static void _ps_assemble_del(struct ps_page *page) {
  ps_widget_del(PAGE->menu);
}

/* Initialize.
 */

static int _ps_assemble_init(struct ps_page *page) {

  page->root->bgrgba=0x200060ff;

  struct ps_widget *packer=ps_widget_spawn(page->root,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,10)<0) return -1;

  struct ps_widget *label=ps_widget_spawn(packer,&ps_widget_type_label);
  if (!label) return -1;
  label->fgrgba=0xffff00ff;
  if (ps_widget_label_set_text(label,"Plunder Squad",-1)<0) return -1;

  if (!(PAGE->menu=ps_widget_new(&ps_widget_type_menu))) return -1;
  if (ps_widget_add_child(packer,PAGE->menu)<0) return -1;
  if (ps_widget_menu_add_option(PAGE->menu,"New Game",-1)<0) return -1;
  if (ps_widget_menu_add_option(PAGE->menu,"Resume Game",-1)<0) return -1;
  if (ps_widget_menu_add_option(PAGE->menu,"Settings",-1)<0) return -1;
  if (ps_widget_menu_add_option(PAGE->menu,"Quit",-1)<0) return -1;

  return 0;
}

/* Menu options.
 */

static int ps_assemble_opt_new_game(struct ps_page *page) {
  struct ps_game *game=ps_page_get_game(page);
  if (!game) return 0;
  //TODO Consider the initialization flow; we probably want to generate the scenario right here.
  if (ps_gui_unload_page(page->gui)<0) return -1;
  return 0;
}

static int ps_assemble_opt_resume_game(struct ps_page *page) {
  ps_log(GUI,DEBUG,"TODO: %s",__func__);
  return 0;
}

static int ps_assemble_opt_settings(struct ps_page *page) {
  ps_log(GUI,DEBUG,"TODO: %s",__func__);
  return 0;
}

static int ps_assemble_opt_quit(struct ps_page *page) {
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

/* Unified input events.
 */

static int _ps_assemble_move_cursor(struct ps_page *page,int dx,int dy) {
  if (ps_widget_menu_move_cursor(PAGE->menu,dy)<0) return -1;
  return 0;
}

static int _ps_assemble_activate(struct ps_page *page) {
  int optionp=ps_widget_menu_get_cursor(PAGE->menu);
  switch (optionp) {
    case 0: return ps_assemble_opt_new_game(page);
    case 1: return ps_assemble_opt_resume_game(page);
    case 2: return ps_assemble_opt_settings(page);
    case 3: return ps_assemble_opt_quit(page);
  }
  return 0;
}

static int _ps_assemble_submit(struct ps_page *page) {
  return 0;
}

static int _ps_assemble_cancel(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_assemble={
  .name="assemble",
  .objlen=sizeof(struct ps_page_assemble),
  .init=_ps_assemble_init,
  .del=_ps_assemble_del,

  .move_cursor=_ps_assemble_move_cursor,
  .activate=_ps_assemble_activate,
  .submit=_ps_assemble_submit,
  .cancel=_ps_assemble_cancel,
};
