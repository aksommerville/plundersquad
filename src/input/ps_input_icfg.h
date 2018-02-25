/* ps_input_icfg.h
 * Interactive device configurator.
 * Use this if you have a device with no map, and you are able to prompt the user to press buttons.
 * We require that each button be pressed twice for validation. You don't need to worry about that.
 *
 * TYPICAL LIFECYCLE:
 *  - ps_input_icfg_new(device)
 *  - ps_input_icfg_event() with unmapped events from that device.
 *  - - if (ps_input_icfg_is_ready()), ps_input_icfg_finish().
 *  - ps_input_icfg_del()
 */

#ifndef PS_INPUT_ICFG_H
#define PS_INPUT_ICFG_H

struct ps_input_device;
struct ps_input_map;
struct ps_input_maptm;

#define PS_INPUT_ICFG_MAP_COUNT 7

struct ps_input_icfg_map {
  int dstbtnid;
  int srcbtnid;
  int srclo,srchi;
  int nvalue;
};

struct ps_input_icfg {
  int refc;
  struct ps_input_device *device;
  struct ps_input_icfg_map mapv[PS_INPUT_ICFG_MAP_COUNT];
  int mapc;
  int64_t delay;
  int srcbtnid;
  int collection_phase;
  int value;

  /* Minimum time in microseconds between events.
   * After we apply an event, for so long, any further events are discarded.
   * This starts with a sensible default and you can change it directly.
   */
  int interlude_us;
  
};

struct ps_input_icfg *ps_input_icfg_new(struct ps_input_device *device);
void ps_input_icfg_del(struct ps_input_icfg *icfg);
int ps_input_icfg_ref(struct ps_input_icfg *icfg);

int ps_input_icfg_set_device(struct ps_input_icfg *icfg,struct ps_input_device *device);

/* Nonzero if we're done collecting events and can finalize the map.
 * No errors.
 */
int ps_input_icfg_is_ready(const struct ps_input_icfg *icfg);

/* Return the button we are looking for right now (PS_PLRBTN_*).
 * Zero if complete or any other problem.
 */
int ps_input_icfg_get_current_button(const struct ps_input_icfg *icfg);

/* Discard any work in progress and begin mapping again from the first button.
 */
int ps_input_icfg_restart(struct ps_input_icfg *icfg);

/* Call this with unmapped events from our device.
 * Returns >0 if the collection state was advanced (ie you should update your UI, or finalize).
 */
int ps_input_icfg_event(struct ps_input_icfg *icfg,int btnid,int value);

/* Convenience: Generate the map, install it in the device, generate template, and save template to global config.
 * This is typically everything you want, once the icfg is ready.
 */
int ps_input_icfg_finish(struct ps_input_icfg *icfg,int playerid);

/* Components for finalization, which you probably shouldn't use.
 */
struct ps_input_map *ps_input_icfg_generate_map(const struct ps_input_icfg *icfg,int playerid);
int ps_input_icfg_install_map(const struct ps_input_icfg *icfg,struct ps_input_map *map);
struct ps_input_maptm *ps_input_icfg_generate_maptm(const struct ps_input_icfg *icfg);
int ps_input_icfg_install_maptm(const struct ps_input_icfg *icfg,struct ps_input_maptm *maptm);

#endif
