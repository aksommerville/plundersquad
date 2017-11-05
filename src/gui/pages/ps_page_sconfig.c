#include "ps.h"
#include "gui/ps_gui.h"

/* Page definition.
 */

struct ps_page_sconfig {
  struct ps_page hdr;
};

#define PAGE ((struct ps_page_sconfig*)page)

/* Delete.
 */

static void _ps_sconfig_del(struct ps_page *page) {
}

/* Initialize.
 */

static int _ps_sconfig_init(struct ps_page *page) {
  return 0;
}

/* Unified input events.
 */

static int _ps_sconfig_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_sconfig_activate(struct ps_page *page) {
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
