/* ps_editor.h
 * Widgets specific to the data editor.
 */

#ifndef PS_EDITOR_H
#define PS_EDITOR_H

#include "util/ps_callback.h"

struct akau_ipcm;
struct ps_iwg;

extern const struct ps_widget_type ps_widget_type_edithome;
extern const struct ps_widget_type ps_widget_type_editsoundeffect;
extern const struct ps_widget_type ps_widget_type_wave; /* Modal. For instruments and ipcm voices. */

// TODO:
// editsong
// editblueprint
// editsprdef
// editplrdef
// editregion

extern const struct ps_widget_type ps_widget_type_sfxchan;
extern const struct ps_widget_type ps_widget_type_sfxgraph;
extern const struct ps_widget_type ps_widget_type_sfxpoint;
extern const struct ps_widget_type ps_widget_type_harmonics;
extern const struct ps_widget_type ps_widget_type_waveview;

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
int ps_widget_editsoundeffect_reassign_channel_ids(struct ps_widget *widget);

/* Sfxchan.
 *****************************************************************************/

int ps_widget_sfxchan_setup(struct ps_widget *widget,int chanid);
int ps_widget_sfxchan_get_chanid(const struct ps_widget *widget);
int ps_widget_sfxchan_set_chanid(struct ps_widget *widget,int chanid);
int ps_widget_sfxchan_rebuild_graphs(struct ps_widget *widget);

/* Sfxgraph.
 *****************************************************************************/

int ps_widget_sfxgraph_set_key(struct ps_widget *widget,int key); // AKAU_WAVEGEN_K_*
int ps_widget_sfxgraph_set_value_limit(struct ps_widget *widget,double v);
double ps_widget_sfxgraph_get_value_limit(const struct ps_widget *widget);

int ps_widget_sfxgraph_rebuild(struct ps_widget *widget);

// Used internally, but we expose for no particular reason.
// (x,y) are relative to this widget, ie the same way mousemotion is reported.
// We quietly reject points outside the widget.
int ps_widget_sfxgraph_add_point(struct ps_widget *widget,int x,int y);

/* Sfxpoint.
 *****************************************************************************/

int ps_widget_sfxpoint_set_cmdp(struct ps_widget *widget,int cmdp);
int ps_widget_sfxpoint_set_time(struct ps_widget *widget,int time);
int ps_widget_sfxpoint_set_value(struct ps_widget *widget,double value);
int ps_widget_sfxpoint_get_cmdp(const struct ps_widget *widget);
int ps_widget_sfxpoint_get_time(const struct ps_widget *widget);
double ps_widget_sfxpoint_get_value(const struct ps_widget *widget);

/* Wave.
 *****************************************************************************/

#define PS_WIDGET_WAVE_USAGE_UNSET      0
#define PS_WIDGET_WAVE_USAGE_IPCM       1 /* Presets, no envelopes. */
#define PS_WIDGET_WAVE_USAGE_INSTRUMENT 2 /* Envelopes, no presets. */

#define PS_WIDGET_WAVE_PRESET_SINE      0
#define PS_WIDGET_WAVE_PRESET_SQUARE    1
#define PS_WIDGET_WAVE_PRESET_SAW       2
#define PS_WIDGET_WAVE_PRESET_NOISE     3
#define PS_WIDGET_WAVE_PRESET_HARMONICS 4

#define PS_WIDGET_WAVE_COEF_COUNT 16 /* AKAU quietly stops at 16. */

struct ps_wave_ui_model {
  int preset;
  double coefv[PS_WIDGET_WAVE_COEF_COUNT];
  int attack_time; // ms
  uint8_t attack_trim;
  int drawback_time; // ms
  uint8_t drawback_trim;
  int decay_time; // ms
};

int ps_widget_wave_set_usage(struct ps_widget *widget,int usage);
int ps_widget_wave_get_usage(const struct ps_widget *widget);

int ps_widget_wave_set_serial(struct ps_widget *widget,const void *src,int srcc);
int ps_widget_wave_serialize(void *dst,int dsta,struct ps_widget *widget);

int ps_widget_wave_set_callback(struct ps_widget *widget,struct ps_callback cb);

/* For children of the wave editor, report current modeling of the wave.
 * To be convenient, (widget) may be a wave editor or any child of it.
 * 'dirty' works the same way, to alert the entire UI of a model change.
 */
struct ps_wave_ui_model *ps_widget_wave_get_model(struct ps_widget *widget);
int ps_widget_wave_dirty(struct ps_widget *widget);

struct akau_fpcm *ps_widget_wave_get_fpcm(struct ps_widget *widget);
struct akau_instrument *ps_widget_wave_get_instrument(struct ps_widget *widget);

/* Harmonics.
 *****************************************************************************/

int ps_widget_harmonics_model_changed(struct ps_widget *widget);

/* Waveview.
 *****************************************************************************/

int ps_widget_waveview_model_changed(struct ps_widget *widget);

#endif
