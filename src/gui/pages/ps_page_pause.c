#include "ps.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "game/ps_game.h"
#include "os/ps_fs.h"

static int ps_pause_restart(struct ps_page *page);
static int ps_pause_save(struct ps_page *page);

/* Page definition.
 */

struct ps_page_pause {
  struct ps_page hdr;
  struct ps_widget *menu;
};

#define PAGE ((struct ps_page_pause*)page)

/* Delete.
 */

static void _ps_pause_del(struct ps_page *page) {
  ps_widget_del(PAGE->menu);
}

/* Initialize.
 */

static int _ps_pause_init(struct ps_page *page) {

  page->root->bgrgba=0x201040c0;
  
  PAGE->menu=ps_widget_spawn(page->root,&ps_widget_type_menu);
  if (ps_widget_ref(PAGE->menu)<0) return -1;

  struct ps_widget *option;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Resume",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Return to start",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"End game",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Quit",-1))) return -1;
  if (!(option=ps_widget_menu_add_option(PAGE->menu,"Save (experimental)",-1))) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_pause_move_cursor(struct ps_page *page,int dx,int dy) {
  if (dx) {
    if (ps_widget_menu_adjust_selection(PAGE->menu,dx)<0) return -1;
  }
  if (dy) {
    if (ps_widget_menu_move_cursor(PAGE->menu,dy)<0) return -1;
  }
  return 0;
}

static int _ps_pause_activate(struct ps_page *page) {
  switch (ps_widget_menu_get_cursor(PAGE->menu)) {
    case 0: { // Resume
        if (ps_game_toggle_pause(ps_gui_get_game(page->gui))<0) return -1;
        if (ps_gui_unload_page(page->gui)<0) return -1;
      } break;
    case 1: { // Restart
        if (ps_pause_restart(page)<0) return -1;
        if (ps_gui_unload_page(page->gui)<0) return -1;
      } break;
    case 2: { // Abort
        if (ps_gui_load_page_assemble(page->gui)<0) return -1;
      } break;
    case 3: { // Quit
        if (ps_input_request_termination()<0) return -1;
      } break;
    case 4: { // Save
        if (ps_pause_save(page)<0) return -1;
      } break;
  }
  return 0;
}

static int _ps_pause_submit(struct ps_page *page) {
  return 0;
}

static int _ps_pause_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_pause_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_pause={
  .name="pause",
  .objlen=sizeof(struct ps_page_pause),
  .init=_ps_pause_init,
  .del=_ps_pause_del,

  .move_cursor=_ps_pause_move_cursor,
  .activate=_ps_pause_activate,
  .submit=_ps_pause_submit,
  .cancel=_ps_pause_cancel,

  .update=_ps_pause_update,
};

/* Restart game.
 */
 
static int ps_pause_restart(struct ps_page *page) {
  if (!page||(page->type!=&ps_page_type_pause)) return -1;
  struct ps_game *game=ps_gui_get_game(page->gui);
  if (!game) return -1;
  if (ps_game_return_to_start_screen(game)<0) return -1;
  return 0;
}

/* Save game.
 */

static int ps_pause_save(struct ps_page *page) {
  if (!page||(page->type!=&ps_page_type_pause)) return -1;

  struct ps_game *game=ps_gui_get_game(page->gui);
  if (!game) return -1;

  void *serial=0;
  int serialc=ps_game_encode(&serial,game);
  if ((serialc<0)||!serial) return -1;

  const char *path="out/savedgame"; 
  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(RES,ERROR,"%s: Failed to write saved game.",path);
    free(serial);
    return -1;
  }
  ps_log(RES,INFO,"Saved game to '%s'.",path);
  
  free(serial);
  return 0;
}
