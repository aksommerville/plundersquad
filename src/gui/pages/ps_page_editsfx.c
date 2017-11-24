#include "ps.h"
#include "gui/ps_gui.h"

/* Page definition.
 */

struct ps_page_editsfx {
  struct ps_page hdr;
  struct ps_widget *resedit; // Dude, remember ResEdit from back in the day? Good times.
};

#define PAGE ((struct ps_page_editsfx*)page)

/* Delete.
 */

static void _ps_editsfx_del(struct ps_page *page) {
  ps_widget_del(PAGE->resedit);
}

/* Initialize.
 */

static int _ps_editsfx_init(struct ps_page *page) {

  page->root->bgrgba=0x00a0e0ff;

  if (!(PAGE->resedit=ps_widget_spawn(page->root,&ps_widget_type_resedit))) return -1;
  if (ps_widget_ref(PAGE->resedit)<0) {
    PAGE->resedit=0;
    return -1;
  }

  return 0;
}

/* Unified input events.
 */

static int _ps_editsfx_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_editsfx_activate(struct ps_page *page) {
  return 0;
}

static int _ps_editsfx_submit(struct ps_page *page) {
  return 0;
}

static int _ps_editsfx_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_editsfx_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_editsfx={
  .name="editsfx",
  .objlen=sizeof(struct ps_page_editsfx),
  .init=_ps_editsfx_init,
  .del=_ps_editsfx_del,

  .move_cursor=_ps_editsfx_move_cursor,
  .activate=_ps_editsfx_activate,
  .submit=_ps_editsfx_submit,
  .cancel=_ps_editsfx_cancel,

  .update=_ps_editsfx_update,
};
