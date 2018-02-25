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

/* Nonzero if the device described here could be used as a player input.
 */
int ps_input_premap_usable(const struct ps_input_premap *premap);

/* Convenience; Create a new premap for this device and attach it to the device.
 * If one already exists, we do nothing and report success.
 */
int ps_input_premap_build_for_device(struct ps_input_device *device);

int ps_input_btncfg_describe(const struct ps_input_btncfg *btncfg);
#define PS_BTNCFG_GARBAGE     0 /* Ignore it. */
#define PS_BTNCFG_TWOSTATE    1 /* Basic 2-state button. */
#define PS_BTNCFG_ONEWAY      2 /* Analogue axis suitable for mapping to one button. */
#define PS_BTNCFG_TWOWAY      3 /* Analogue axis suitable for mapping to two buttons (up+down or left+right). */

#endif
