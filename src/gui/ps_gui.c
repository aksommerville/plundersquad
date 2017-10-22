#include "ps_gui_internal.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* Draw, master.
 */

static int _ps_gui_layer_draw(struct ps_video_layer *layer) {
  struct ps_gui *gui=((struct ps_gui_layer*)layer)->gui;

  if (gui->page) {
    if (gui->page->root) {
      if (ps_widget_draw(gui->page->root,0,0)<0) return -1;
    }
  }

  return 0;
}

/* Initialize video layer.
 */

static int ps_gui_init_layer(struct ps_gui *gui) {

  gui->layer=ps_video_layer_new(sizeof(struct ps_gui_layer));
  if (!gui->layer) return -1;

  gui->layer->blackout=1;
  gui->layer->draw=_ps_gui_layer_draw;
  LAYER->gui=gui;

  if (ps_video_install_layer(gui->layer,-1)<0) return -1;

  return 0;
}

/* GUI object lifecycle.
 */

struct ps_gui *ps_gui_new() {

  struct ps_gui *gui=calloc(1,sizeof(struct ps_gui));
  if (!gui) return 0;

  if (ps_gui_init_layer(gui)<0) {
    ps_gui_del(gui);
    return 0;
  }

  return gui;
}

void ps_gui_del(struct ps_gui *gui) {
  if (!gui) return;

  ps_page_del(gui->page);
  ps_video_uninstall_layer(gui->layer);
  ps_video_layer_del(gui->layer);

  free(gui);
}

/* Update.
 */

int ps_gui_update(struct ps_gui *gui) {
  if (!gui) return -1;
  //TODO update gui
  return 0;
}

/* Load page.
 */

int ps_gui_load_page(struct ps_gui *gui,struct ps_page *page) {
  if (!gui||!page) return -1;
  if (!page->root) return -1;
  if (gui->page==page) return 0; // or should we restart it, or something?

  //TODO transitions and events on page load

  /* Unload the existing page first. */
  if (gui->page) {
    ps_page_del(gui->page);
    gui->page=0;
  }

  /* Retain the incoming page. */
  if (ps_page_ref(page)<0) return -1;
  gui->page=page;

  /* Pack the page's root.
   * This is nice and easy because the global framebuffer has a constant size known at compile time.
   */
  page->root->x=0;
  page->root->y=0;
  page->root->w=PS_SCREENW;
  page->root->h=PS_SCREENH;
  if (ps_widget_pack(page->root)<0) return -1;

  return 0;
}

/* Conveniences to instantiate and load a page.
 */

#define LOADER(tag) \
  int ps_gui_load_page_##tag(struct ps_gui *gui) { \
    struct ps_page *page=ps_page_new(&ps_page_type_##tag); \
    if (!page) return -1; \
    if (ps_gui_load_page(gui,page)<0) { \
      ps_page_del(page); \
      return -1; \
    } \
    ps_page_del(page); \
    return 0; \
  }
#define STUBLOADER(tag) int ps_gui_load_page_##tag(struct ps_gui *gui) { return -1; }

LOADER(assemble)
STUBLOADER(sconfig)
STUBLOADER(pconfig)
STUBLOADER(pause)
STUBLOADER(debug)
STUBLOADER(gameover)

#undef LOADER
#undef STUBLOADER
