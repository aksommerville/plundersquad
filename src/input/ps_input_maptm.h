/* ps_input_maptm.h
 * Template for input device mapping.
 * These are part of the configuration.
 */

#ifndef PS_INPUT_MAPTM_H
#define PS_INPUT_MAPTM_H

struct ps_input_device;

/* Where a template has (srclo==0) and (srchi<0), srchi is an automatic indicator.
 * Fields initialized like this will consider the actual input range from the device.
 */
#define PS_INPUT_MAPTM_AUTO_TOPHALF       -1
#define PS_INPUT_MAPTM_AUTO_BOTTOMHALF    -2
#define PS_INPUT_MAPTM_AUTO_TOPTHIRD      -3
#define PS_INPUT_MAPTM_AUTO_MIDDLETHIRD   -4
#define PS_INPUT_MAPTM_AUTO_BOTTOMTHIRD   -5

struct ps_input_maptm_fld {
  int srcbtnid,dstbtnid;
  int srclo,srchi;
};

struct ps_input_maptm {
  char *namepattern; // Unset is effectively "*"
  int namepatternc;
  int providerid;    // Zero matches all
  uint16_t deviceid; // Zero matches all
  uint16_t vendorid; // Zero matches all
  struct ps_input_maptm_fld *fldv;
  int fldc,flda;
  int invalid_provider; // Flag set during decode if a provider was named but is not installed.
};

struct ps_input_maptm *ps_input_maptm_new();
void ps_input_maptm_del(struct ps_input_maptm *maptm);

int ps_input_maptm_set_namepattern(struct ps_input_maptm *maptm,const char *src,int srcc);

/* Zero if it does not match or anything is amiss.
 * >0 if it matches; higher values are better-quality matches.
 * Never returns negative.
 */
int ps_input_maptm_match_device(const struct ps_input_maptm *maptm,const struct ps_input_device *device);

/* Generate a new map from this template for this device.
 * We do not actually install it in the device.
 * We do not assign map->plrid.
 */
struct ps_input_map *ps_input_maptm_apply_for_device(
  const struct ps_input_maptm *maptm,struct ps_input_device *device
);

int ps_input_maptm_fld_search(const struct ps_input_maptm *maptm,int srcbtnid);
struct ps_input_maptm_fld *ps_input_maptm_fld_insert(struct ps_input_maptm *maptm,int p,int srcbtnid);

/* Decoding measures the text block and returns consumed length.
 * Encoded maptm begins with '{' and ends with '}' each on their own line.
 * '#' begins a line comment.
 * Each not-empty line in that block is either a header field or a button mapping.
 * Header fields are "KEY = VALUE":
 *   name = STRING
 *   provider = IDENTIFIER
 *   device = INTEGER
 *   vendor = INTEGER
 * Mappings are "SRCBTNID [LO HI] DSTBTNID".
 */
int ps_input_maptm_encode(char *dst,int dsta,const struct ps_input_maptm *maptm);
int ps_input_maptm_decode(struct ps_input_maptm *maptm,const char *src,int srcc);

/* Create a new template that matches this devices as closely as possible.
 * If (device->map) is set, each of those live mappings is templatized.
 */
struct ps_input_maptm *ps_input_maptm_generate_from_device(const struct ps_input_device *device);

#endif
