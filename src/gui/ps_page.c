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

  if (page->modalv) {
    while (page->modalc-->0) ps_widget_del(page->modalv[page->modalc]);
    free(page->modalv);
  }

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

/* Dispatch input events.
 */
 
int ps_page_move_cursor(struct ps_page *page,int dx,int dy) {
  if (!page) return -1;
  if (page->modalc) {
    //TODO general events to modals?
  } else {
    if (page->type->move_cursor) {
      if (page->type->move_cursor(page,dx,dy)<0) return -1;
    }
  }
  return 0;
}

int ps_page_activate(struct ps_page *page) {
  if (!page) return -1;
  if (page->modalc) {
    if (ps_widget_event_activate(page->modalv[page->modalc-1])<0) return -1;
  } else {
    if (page->type->activate) {
      if (page->type->activate(page)<0) return -1;
    } else if (page->type->submit) {
      if (page->type->submit(page)<0) return -1;
    }
  }
  return 0;
}

int ps_page_submit(struct ps_page *page) {
  if (!page) return -1;
  if (page->modalc) {
    if (ps_widget_event_activate(page->modalv[page->modalc-1])<0) return -1;
  } else {
    if (page->type->submit) {
      if (page->type->submit(page)<0) return -1;
    } else if (page->type->activate) {
      if (page->type->activate(page)<0) return -1;
    }
  }
  return 0;
}

int ps_page_cancel(struct ps_page *page) {
  if (!page) return -1;
  if (page->modalc) {
    if (ps_page_pop_modal(page)<0) return -1;
  } else {
    if (page->type->cancel) {
      if (page->type->cancel(page)<0) return -1;
    }
  }
  return 0;
}

/* Push modal.
 */

int ps_page_push_modal(struct ps_page *page,struct ps_widget *modal) {
  if (!page||!modal) return -1;
  if (modal->parent) return -1;

  if (page->modalc>=page->modala) {
    int na=page->modala+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(page->modalv,sizeof(void*)*na);
    if (!nv) return -1;
    page->modalv=nv;
    page->modala=na;
  }

  if (ps_widget_ref(modal)<0) return -1;
  page->modalv[page->modalc++]=modal;

  int chw,chh;
  if (ps_widget_measure(&chw,&chh,modal,PS_SCREENW,PS_SCREENH)<0) return -1;
  modal->w=chw;
  modal->h=chh;
  modal->x=(PS_SCREENW>>1)-(chw>>1);
  modal->y=(PS_SCREENH>>1)-(chh>>1);
  if (ps_widget_pack(modal)<0) return -1;

  return 0;
}

/* Pop modal.
 */

int ps_page_pop_modal(struct ps_page *page) {
  if (!page) return -1;
  if (page->modalc<1) return -1;
  struct ps_widget *modal=page->modalv[--(page->modalc)];
  ps_widget_del(modal);
  return 0;
}
