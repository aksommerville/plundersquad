/* ps_menus.h
 */

#ifndef PS_MENUS_H
#define PS_MENUS_H

struct ps_input_device;
struct ps_game;

extern const struct ps_widget_type ps_widget_type_assemblepage; /* First screen, click in and choose a hero. */
extern const struct ps_widget_type ps_widget_type_setuppage; /* Second screen, configure scenario generator. */
extern const struct ps_widget_type ps_widget_type_pausepage; /* Popup menu during game play. */
extern const struct ps_widget_type ps_widget_type_gameoverpage; /* Post-game report and menu. */

extern const struct ps_widget_type ps_widget_type_heropacker; /* Principal component of assemblepage. */
extern const struct ps_widget_type ps_widget_type_heroselect; /* Single player box in assemblepage. */
extern const struct ps_widget_type ps_widget_type_sprite; /* General-purpose sprite view. */
extern const struct ps_widget_type ps_widget_type_report; /* Game-over report. */

/* Assemblepage.
 *****************************************************************************/

int ps_widget_heroselect_set_device(struct ps_widget *widget,struct ps_input_device *device);
struct ps_input_device *ps_widget_heroselect_get_device(const struct ps_widget *widget);
int ps_widget_heroselect_set_plrdefid(struct ps_widget *widget,int plrdefid);
int ps_widget_heroselect_get_plrdefid(const struct ps_widget *widget);
int ps_widget_heroselect_set_palette(struct ps_widget *widget,int palette);
int ps_widget_heroselect_get_palette(const struct ps_widget *widget);
int ps_widget_heroselect_is_pending(const struct ps_widget *widget); // => nonzero if user is choosing, not ready
int ps_widget_heroselect_is_ready(const struct ps_widget *widget); // => nonzero if selection is complete

/* Drop the current sprite and replace with a new one by instantiating a sprdef resource.
 */
int ps_widget_sprite_load_sprdef(struct ps_widget *widget,int sprdefid);

int ps_widget_sprite_set_plrdefid(struct ps_widget *widget,int plrdefid);
int ps_widget_sprite_set_palette(struct ps_widget *widget,int palette);
int ps_widget_sprite_get_plrdefid(const struct ps_widget *widget);
int ps_widget_sprite_get_palette(const struct ps_widget *widget);

/* Move value by one in either direction, and try to ensure that it remain valid.
 */
int ps_widget_sprite_modify_plrdefid(struct ps_widget *widget,int d);
int ps_widget_sprite_modify_palette(struct ps_widget *widget,int d);

/* Gameoverpage.
 *****************************************************************************/

int ps_widget_gameoverpage_setup(struct ps_widget *widget,const struct ps_game *game);
int ps_widget_report_generate(struct ps_widget *widget,const struct ps_game *game);

#endif