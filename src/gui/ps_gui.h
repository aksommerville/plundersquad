/* ps_gui.h
 * Container for interactive menus and such.
 * There are three major classes:
 *  - ps_gui is the global container.
 *  - ps_page is a single screen of content with controller logic.
 *  - ps_widget is the basic element of which pages are composed.
 */

#ifndef PS_GUI_H
#define PS_GUI_H

#include "ps_page.h"
#include "ps_widget.h"
#include "ps_widget_types.h"

/* Global GUI.
 * Only one ps_gui needs to be instantiated, but you're free to make as many as you like.
 */

struct ps_gui *ps_gui_new();
void ps_gui_del(struct ps_gui *gui);

int ps_gui_update(struct ps_gui *gui);

int ps_gui_load_page(struct ps_gui *gui,struct ps_page *page);

/* Conveniences to instantiate a new page and load it.
 */

int ps_gui_load_page_assemble(struct ps_gui *gui);
int ps_gui_load_page_sconfig(struct ps_gui *gui);
int ps_gui_load_page_pconfig(struct ps_gui *gui);
int ps_gui_load_page_pause(struct ps_gui *gui);
int ps_gui_load_page_debug(struct ps_gui *gui);
int ps_gui_load_page_gameover(struct ps_gui *gui);

#endif
