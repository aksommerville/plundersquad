/* ps_input_config.h
 * Serializable singleton owned by global input.
 * Contains mapping templates and other configuration.
 */

#ifndef PS_INPUT_CONFIG_H
#define PS_INPUT_CONFIG_H

struct ps_input_maptm;
struct ps_input_device;

struct ps_input_config {
  struct ps_input_maptm **maptmv;
  int maptmc,maptma;
};

struct ps_input_config *ps_input_config_new();
void ps_input_config_del(struct ps_input_config *config);

int ps_input_config_clear(struct ps_input_config *config);

int ps_input_config_save(const char *path,const struct ps_input_config *config);
int ps_input_config_load(struct ps_input_config *config,const char *path);

int ps_input_config_encode(void *dstpp,const struct ps_input_config *config);
int ps_input_config_decode(struct ps_input_config *config,const char *src,int srcc);

struct ps_input_maptm *ps_input_config_select_maptm_for_device(
  const struct ps_input_config *config,const struct ps_input_device *device
);

#endif
