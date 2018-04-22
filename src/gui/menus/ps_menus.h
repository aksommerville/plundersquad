/* ps_menus.h
 */

#ifndef PS_MENUS_H
#define PS_MENUS_H

struct ps_input_device;
struct ps_game;
struct ps_res_trdef;

extern const struct ps_widget_type ps_widget_type_assemblepage; /* First screen, click in and choose a hero. */
extern const struct ps_widget_type ps_widget_type_setuppage; /* Second screen, configure scenario generator. */
extern const struct ps_widget_type ps_widget_type_pausepage; /* Popup menu during game play. */
extern const struct ps_widget_type ps_widget_type_gameoverpage; /* Post-game report and menu. */
extern const struct ps_widget_type ps_widget_type_inputcfgpage; /* Reconfigure input devices on the fly. */
extern const struct ps_widget_type ps_widget_type_treasurealert; /* Splash to show newly-collected treasure. */
extern const struct ps_widget_type ps_widget_type_debugmenu; /* Special pause menu with debug-only features. */
extern const struct ps_widget_type ps_widget_type_audiocfgpage; /* Set audio levels. */

extern const struct ps_widget_type ps_widget_type_heropacker; /* Principal component of assemblepage. */
extern const struct ps_widget_type ps_widget_type_heroselect; /* Single player box in assemblepage. */
extern const struct ps_widget_type ps_widget_type_sprite; /* General-purpose sprite view. */
extern const struct ps_widget_type ps_widget_type_report; /* Game-over report. */
extern const struct ps_widget_type ps_widget_type_inputcfgpacker; /* Principal component of inputcfgpage. */
extern const struct ps_widget_type ps_widget_type_inputcfg; /* Panel in inputcfgpacker, for one device. */
extern const struct ps_widget_type ps_widget_type_inputstatus; /* Shows current values from device. */
extern const struct ps_widget_type ps_widget_type_awards; /* Shows each player and his award. */

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

int ps_widget_heropacker_count_active_players(const struct ps_widget *widget);

/* Drop the current sprite and replace with a new one by instantiating a sprdef resource.
 */
int ps_widget_sprite_load_sprdef(struct ps_widget *widget,int sprdefid);

int ps_widget_sprite_set_plrdefid(struct ps_widget *widget,int plrdefid);
int ps_widget_sprite_set_palette(struct ps_widget *widget,int palette);
int ps_widget_sprite_get_plrdefid(const struct ps_widget *widget);
int ps_widget_sprite_get_palette(const struct ps_widget *widget);
int ps_widget_sprite_set_action_walk_down(struct ps_widget *widget);

/* Move value by one in either direction, and try to ensure that it remain valid.
 */
int ps_widget_sprite_modify_plrdefid(struct ps_widget *widget,int d);
int ps_widget_sprite_modify_palette(struct ps_widget *widget,int d);

/* Pausepage.
 *****************************************************************************/

/* Call early, after inserting in GUI.
 */
int ps_widget_pausepage_skin_for_player(struct ps_widget *widget);

/* Gameoverpage.
 *****************************************************************************/

int ps_widget_gameoverpage_setup(struct ps_widget *widget,const struct ps_game *game);
int ps_widget_report_generate(struct ps_widget *widget,const struct ps_game *game);
int ps_widget_awards_setup(struct ps_widget *widget,const struct ps_game *game);

/* Inputcfgpage.
 *****************************************************************************/

int ps_widget_inputcfgpage_bump(struct ps_widget *widget); // resets the idle counter
int ps_widget_inputcfg_setup(struct ps_widget *widget,struct ps_input_device *device);
struct ps_input_device *ps_widget_inputcfg_get_device(const struct ps_widget *widget);
int ps_inputcfg_cb_begin_reset(struct ps_widget *button,struct ps_widget *widget);
int ps_widget_inputcfg_set_reset_shortcut_name(struct ps_widget *widget,const char *name);
int ps_widget_inputstatus_set_device(struct ps_widget *widget,struct ps_input_device *device);
int ps_widget_inputstatus_set_manual(struct ps_widget *widget,int manual); // Do this to highlight buttons manually.
int ps_widget_inputstatus_highlight(struct ps_widget *widget,uint16_t btnid,int value);

/* Treasurealert.
 *****************************************************************************/

int ps_widget_treasurealert_setup(struct ps_widget *widget,const struct ps_res_trdef *trdef);

#endif
