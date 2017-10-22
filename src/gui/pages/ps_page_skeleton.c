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

/* Type definition.
 */

const struct ps_page_type ps_page_type_skeleton={
  .name="skeleton",
  .objlen=sizeof(struct ps_page_skeleton),
  .init=_ps_skeleton_init,
  .del=_ps_skeleton_del,
};
