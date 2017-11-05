#include "ps_gui_internal.h"

/* Object lifecycle.
 */

struct ps_page *ps_page_new(const struct ps_page_type *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ps_page)) return 0;
  
  struct ps_page *page=calloc(1,type->objlen);
  if (!page) return 0;

  page->type=type;
  page->refc=1;

  if (!(page->root=ps_widget_new(&ps_widget_type_root))) {
    ps_page_del(page);
    return 0;
  }
  if (ps_widget_root_set_page(page->root,page)<0) {
    ps_page_del(page);
    return 0;
  }

  if (type->init) {
    if (type->init(page)<0) {
      ps_page_del(page);
      return 0;
    }
  }

  return page;
}

void ps_page_del(struct ps_page *page) {
  if (!page) return;
  if (page->refc-->1) return;

  ps_widget_del(page->root);
  if (page->type->del) page->type->del(page);

  free(page);
}

int ps_page_ref(struct ps_page *page) {
  if (!page) return -1;
  if (page->refc<1) return -1;
  if (page->refc==INT_MAX) return -1;
  page->refc++;
  return 0;
}

/* Update.
 */

int ps_page_update(struct ps_page *page) {
  if (!page) return -1;
  if (!page->type->update) return 0;
  return page->type->update(page);
}

/* Get game.
 */
 
struct ps_game *ps_page_get_game(const struct ps_page *page) {
  if (!page) return 0;
  if (!page->gui) return 0;
  return page->gui->game;
}
