/* ps_input_map.h
 * Live mapping from driver device to player and actions.
 * We have an intrinsic limit that one source device can map to only one player.
 */

#ifndef PS_INPUT_MAP_H
#define PS_INPUT_MAP_H

struct ps_input_map_fld {
  int srcbtnid;
  int srclo,srchi;
  int dstbtnid;
  int srcv,dstv;
};

struct ps_input_map {
  int refc;
  int plrid;
  struct ps_input_map_fld *fldv;
  int fldc,flda;
};

struct ps_input_map *ps_input_map_new();
void ps_input_map_del(struct ps_input_map *map);
int ps_input_map_ref(struct ps_input_map *map);

/* Report new state of a driver-side button.
 * If a callback is provided, we trigger it for all changed fields.
 * We do not trigger callback if the field's output value remains the same.
 */
int ps_input_map_set_button(
  struct ps_input_map *map,
  int srcbtnid,int srcv,
  void *userdata,
  int (*cb)(int plrid,int btnid,int value,void *userdata)
);

/* Searching a field returns the index of any field with this srcbtnid (not necessarily first or last).
 * If absent, it returns -(insertionpoint)-1.
 * If you provide an OOB position to insert, we figure it out for you.
 * Insert returns the field, with only (srcbtnid) populated and the rest zeroed.
 */
int ps_input_map_search(const struct ps_input_map *map,int srcbtnid);
struct ps_input_map_fld *ps_input_map_insert(struct ps_input_map *map,int p,int srcbtnid);

#endif
