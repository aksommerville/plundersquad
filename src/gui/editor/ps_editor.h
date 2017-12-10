/* ps_editor.h
 * Widgets specific to the data editor.
 */

#ifndef PS_EDITOR_H
#define PS_EDITOR_H

struct akau_ipcm;
struct ps_iwg;

extern const struct ps_widget_type ps_widget_type_edithome;
extern const struct ps_widget_type ps_widget_type_editsoundeffect;

// TODO:
// editsong
// editblueprint
// editsprdef
// editplrdef
// editregion

extern const struct ps_widget_type ps_widget_type_sfxchan;
extern const struct ps_widget_type ps_widget_type_sfxgraph;
extern const struct ps_widget_type ps_widget_type_sfxpoint;

/* Edithome.
 *****************************************************************************/

/* (widget) can be any of the resource-specific controller types.
 * It can also be a dialogue, in which case we populate it with a friendly error message.
 * This function lives in ps_widget_edithome.c.
 */
int ps_widget_editor_set_resource(struct ps_widget *widget,int id,void *obj,const char *name);

/* Editsoundeffect.
 *****************************************************************************/

int ps_widget_editsoundeffect_set_resource(struct ps_widget *widget,int id,struct akau_ipcm *ipcm,const char *name);

// The iwg object is shared across editsoundeffect and its descendants.
struct ps_iwg *ps_widget_editsoundeffect_get_iwg(const struct ps_widget *widget);

int ps_widget_editsoundeffect_rebuild_graphs(struct ps_widget *widget);

/* Sfxchan.
 *****************************************************************************/

int ps_widget_sfxchan_setup(struct ps_widget *widget,int chanid);
int ps_widget_sfxchan_get_chanid(const struct ps_widget *widget);
int ps_widget_sfxchan_rebuild_graphs(struct ps_widget *widget);

/* Sfxgraph.
 *****************************************************************************/

int ps_widget_sfxgraph_set_key(struct ps_widget *widget,int key); // AKAU_WAVEGEN_K_*
int ps_widget_sfxgraph_set_value_limit(struct ps_widget *widget,double v);
double ps_widget_sfxgraph_get_value_limit(const struct ps_widget *widget);

int ps_widget_sfxgraph_rebuild(struct ps_widget *widget);

/* Sfxpoint.
 *****************************************************************************/

int ps_widget_sfxpoint_set_cmdp(struct ps_widget *widget,int cmdp);
int ps_widget_sfxpoint_set_time(struct ps_widget *widget,int time);
int ps_widget_sfxpoint_set_value(struct ps_widget *widget,double value);
int ps_widget_sfxpoint_get_cmdp(const struct ps_widget *widget);
int ps_widget_sfxpoint_get_time(const struct ps_widget *widget);
double ps_widget_sfxpoint_get_value(const struct ps_widget *widget);

#endif
