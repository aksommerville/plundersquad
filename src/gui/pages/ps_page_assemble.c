#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"

/* Page definition.
 */

struct ps_page_assemble {
  struct ps_page hdr;
};

#define PAGE ((struct ps_page_assemble*)page)

/* Delete.
 */

static void _ps_assemble_del(struct ps_page *page) {
}

/* Initialize.
 */

static int _ps_assemble_init(struct ps_page *page) {

  page->root->bgrgba=0x200060ff;

  struct ps_widget *packer=ps_widget_spawn(page->root,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,0)<0) return -1;

  #define ADDLABEL(text) { \
    struct ps_widget *label=ps_widget_spawn(packer,&ps_widget_type_label); \
    if (!label) return -1; \
    if (ps_widget_label_set_text(label,text,sizeof(text)-1)<0) return -1; \
  }

  ADDLABEL("Plunder Squad")
  ADDLABEL("This is the assemble page.")
  ADDLABEL("The quick brown fox jumps")
  ADDLABEL("over the lazy dog.")

  #undef ADDLABEL

  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_assemble={
  .name="assemble",
  .objlen=sizeof(struct ps_page_assemble),
  .init=_ps_assemble_init,
  .del=_ps_assemble_del,
};
