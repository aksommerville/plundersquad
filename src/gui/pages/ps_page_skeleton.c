#include "ps.h"
#include "gui/ps_gui.h"

/* Page definition.
 */

struct ps_page_skeleton {
  struct ps_page hdr;
};

#define PAGE ((struct ps_page_skeleton*)page)

/* Delete.
 */

static void _ps_skeleton_del(struct ps_page *page) {
}

/* Initialize.
 */

static int _ps_skeleton_init(struct ps_page *page) {
  return 0;
}

/* Unified input events.
 */

static int _ps_skeleton_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_skeleton_activate(struct ps_page *page) {
  return 0;
}

static int _ps_skeleton_submit(struct ps_page *page) {
  return 0;
}

static int _ps_skeleton_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_skeleton_update(struct ps_page *page) {
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_skeleton={
  .name="skeleton",
  .objlen=sizeof(struct ps_page_skeleton),
  .init=_ps_skeleton_init,
  .del=_ps_skeleton_del,

  .move_cursor=_ps_skeleton_move_cursor,
  .activate=_ps_skeleton_activate,
  .submit=_ps_skeleton_submit,
  .cancel=_ps_skeleton_cancel,

  .update=_ps_skeleton_update,
};
