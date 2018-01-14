/* ps_input_premap.h
 * Container for button configurations from a device.
 */

#ifndef PS_INPUT_PREMAP_H
#define PS_INPUT_PREMAP_H

struct ps_input_btncfg; // ps_input_device.h
struct ps_input_device;

struct ps_input_premap {
  int refc;

  struct ps_input_btncfg *btncfgv;
  int btncfgc,btncfga;

  int btnc;
  int axis1c;
  int axis2c;
  int garbagec;
  
};

struct ps_input_premap *ps_input_premap_new();
void ps_input_premap_del(struct ps_input_premap *premap);
int ps_input_premap_ref(struct ps_input_premap *premap);

int ps_input_premap_rebuild(struct ps_input_premap *premap,struct ps_input_device *device);

int ps_input_premap_search(const struct ps_input_premap *premap,int srcbtnid);
struct ps_input_btncfg *ps_input_premap_get(const struct ps_input_premap *premap,int srcbtnid);
struct ps_input_btncfg *ps_input_premap_insert(struct ps_input_premap *premap,int p,int srcbtnid);

#endif
