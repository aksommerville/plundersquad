/* ps_editor.h
 * Widgets specific to the data editor.
 */

#ifndef PS_EDITOR_H
#define PS_EDITOR_H

#include "util/ps_callback.h"

struct akau_ipcm;
struct akau_song;
struct ps_iwg;
struct ps_sem;
struct ps_blueprint;
struct ps_blueprint_poi;
struct ps_blueprint_solution;
struct ps_sprdef;

extern const struct ps_widget_type ps_widget_type_edithome;
extern const struct ps_widget_type ps_widget_type_editsoundeffect;
extern const struct ps_widget_type ps_widget_type_wave; /* Modal. For instruments and ipcm voices. */
extern const struct ps_widget_type ps_widget_type_editsong;
extern const struct ps_widget_type ps_widget_type_editblueprint;
extern const struct ps_widget_type ps_widget_type_editsprdef;

// TODO:
// editplrdef
// editregion

extern const struct ps_widget_type ps_widget_type_sfxchan;
extern const struct ps_widget_type ps_widget_type_sfxgraph;
extern const struct ps_widget_type ps_widget_type_sfxpoint;
extern const struct ps_widget_type ps_widget_type_harmonics;
extern const struct ps_widget_type ps_widget_type_waveview;
extern const struct ps_widget_type ps_widget_type_waveenv;
extern const struct ps_widget_type ps_widget_type_waveenvpoint;
extern const struct ps_widget_type ps_widget_type_songchart;
extern const struct ps_widget_type ps_widget_type_songvoice;
extern const struct ps_widget_type ps_widget_type_songevent; // Modal dialogue.
extern const struct ps_widget_type ps_widget_type_bpchart;
extern const struct ps_widget_type ps_widget_type_editpoi; // Modal dialogue.
extern const struct ps_widget_type ps_widget_type_editsolution; // Modal dialogue.
extern const struct ps_widget_type ps_widget_type_tileselect; // Modal dialogue.
extern const struct ps_widget_type ps_widget_type_editgrpmask; // Modal dialogue.

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

int ps_widget_wave_set_refnum1(struct ps_widget *widget,int v);
int ps_widget_wave_get_refnum1(const struct ps_widget *widget);

/* For children of the wave editor, report current modeling of the wave.
 * To be convenient, (widget) may be a wave editor or any child of it.
 * 'dirty' works the same way, to alert the entire UI of a model change.
 */
struct ps_wave_ui_model *ps_widget_wave_get_model(struct ps_widget *widget);
int ps_widget_wave_dirty(struct ps_widget *widget);

struct akau_fpcm *ps_widget_wave_get_fpcm(struct ps_widget *widget);
struct akau_instrument *ps_widget_wave_get_instrument(struct ps_widget *widget);

/* Wave components: harmonics, waveview, waveenv, waveenvpoint.
 *****************************************************************************/

#define PS_WIDGET_WAVEENVPOINT_USAGE_ATTACK   1
#define PS_WIDGET_WAVEENVPOINT_USAGE_DRAWBACK 2

int ps_widget_harmonics_model_changed(struct ps_widget *widget);

int ps_widget_waveview_model_changed(struct ps_widget *widget);

int ps_widget_waveenv_model_changed(struct ps_widget *widget);
int ps_widget_waveenv_get_timescale(const struct ps_widget *widget);

int ps_widget_waveenvpoint_set_usage(struct ps_widget *widget,int usage);
int ps_widget_waveenvpoint_get_usage(const struct ps_widget *widget);

/* Editsong.
 *****************************************************************************/

int ps_widget_editsong_set_resource(struct ps_widget *widget,int id,struct akau_song *song,const char *name);

int ps_widget_editsong_toggle_playback(struct ps_widget *widget);

/* For convenience, these accept any descendant of the editsong.
 */
struct akau_song *ps_widget_editsong_get_song(const struct ps_widget *widget);
struct ps_sem *ps_widget_editsong_get_sem(const struct ps_widget *widget);
int ps_widget_editsong_dirty(struct ps_widget *widget);
int ps_widget_editsong_get_current_beat(const struct ps_widget *widget);

/* Real song editing. Again, these accept any descendant.
 * These are mostly interactive.
 * If a (force) argument exists, pass nonzero to skip any verification dialogue.
 */
int ps_widget_editsong_edit_drum(struct ps_widget *widget,int drumid);
int ps_widget_editsong_edit_instrument(struct ps_widget *widget,int instrid);
int ps_widget_editsong_delete_drum(struct ps_widget *widget,int drumid,int force);
int ps_widget_editsong_delete_instrument(struct ps_widget *widget,int instrid,int force);
int ps_widget_editsong_play_from_beat(struct ps_widget *widget,int beatp);

/* (drums) and (instruments) each point to 32 bytes.
 * We set a bit in each buffer for the voices currently visible. (little-endianly)
 */
int ps_widget_editsong_get_voice_visibility(uint8_t *drums,uint8_t *instruments,const struct ps_widget *widget);
#define PS_EDITSONG_TEST_VOICE(buffer,voiceid) ((buffer)[(voiceid)>>3]&(1<<((voiceid)&7)))

int ps_widget_editsong_voice_visibility_changed(struct ps_widget *widget);

/* Editsong components.
 *****************************************************************************/

int ps_widget_songvoice_setup_drum(struct ps_widget *widget,int drumid);
int ps_widget_songvoice_setup_instrument(struct ps_widget *widget,int instrid);

// 0=invalid or unset, 1=drum, 2=instrument
int ps_widget_songvoice_get_type(const struct ps_widget *widget);
uint8_t ps_widget_songvoice_get_id(const struct ps_widget *widget);
int ps_widget_songvoice_get_visibility(const struct ps_widget *widget);

int ps_widget_songchart_rebuild(struct ps_widget *widget);
int ps_widget_songchart_voice_visibility_changed(struct ps_widget *widget);

int ps_widget_songevent_setup(struct ps_widget *widget,struct ps_sem *sem,int beatp,int eventp);

/* Editblueprint.
 *****************************************************************************/

int ps_widget_editblueprint_set_resource(struct ps_widget *widget,int id,struct ps_blueprint *blueprint,const char *name);
int ps_widget_editblueprint_highlight_poi_for_cell(struct ps_widget *widget,int col,int row);

struct ps_blueprint *ps_widget_editblueprint_get_blueprint(const struct ps_widget *widget);

int ps_widget_editpoi_set_poi(struct ps_widget *widget,const struct ps_blueprint_poi *poi);
const struct ps_blueprint_poi *ps_widget_editpoi_get_poi(const struct ps_widget *widget);
int ps_widget_editpoi_set_callback(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_editpoi_set_index(struct ps_widget *widget,int index); // For reference only.
int ps_widget_editpoi_get_index(const struct ps_widget *widget);
int ps_widget_editpoi_set_blueprint(struct ps_widget *widget,struct ps_blueprint *blueprint); // For validation
int ps_widget_editpoi_wants_deleted(const struct ps_widget *widget); // Always check first in your callback.

int ps_widget_editsolution_set_solution(struct ps_widget *widget,const struct ps_blueprint_solution *solution);
const struct ps_blueprint_solution *ps_widget_editsolution_get_solution(const struct ps_widget *widget);
int ps_widget_editsolution_set_callback(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_editsolution_set_index(struct ps_widget *widget,int index);
int ps_widget_editsolution_get_index(const struct ps_widget *widget);
int ps_widget_editsolution_wants_deleted(const struct ps_widget *widget);

/* Editsprdef.
 *****************************************************************************/

int ps_widget_editsprdef_set_resource(struct ps_widget *widget,int id,struct ps_sprdef *sprdef,const char *name);

/* Tileselect.
 *****************************************************************************/

int ps_widget_tileselect_set_tileid(struct ps_widget *widget,uint16_t tileid);
uint16_t ps_widget_tileselect_get_tileid(const struct ps_widget *widget);
int ps_widget_tileselect_set_callback(struct ps_widget *widget,struct ps_callback cb);

/* Editgrpmask.
 *****************************************************************************/

int ps_widget_editgrpmask_set_grpmask(struct ps_widget *widget,uint32_t grpmask);
uint32_t ps_widget_editgrpmask_get_grpmask(const struct ps_widget *widget);
int ps_widget_editgrpmask_set_callback(struct ps_widget *widget,struct ps_callback cb);

#endif
