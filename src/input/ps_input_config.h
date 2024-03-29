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
  char *path;
};

struct ps_input_config *ps_input_config_new();
void ps_input_config_del(struct ps_input_config *config);

int ps_input_config_clear(struct ps_input_config *config);

/* (path) may be null when saving, and we will use the last path we saw.
 */
int ps_input_config_save(const char *path,struct ps_input_config *config);
int ps_input_config_load(struct ps_input_config *config,const char *path);

int ps_input_config_encode(void *dstpp,const struct ps_input_config *config);
int ps_input_config_decode(struct ps_input_config *config,const char *src,int srcc);

struct ps_input_maptm *ps_input_config_select_maptm_for_device(
  const struct ps_input_config *config,const struct ps_input_device *device
);

int ps_input_config_install_maptm(struct ps_input_config *config,struct ps_input_maptm *maptm_HANDOFF);

/* Delete any installed maptm which matches the same devices as this new one.
 * New maptm MUST NOT already be installed, or it will surely be deleted.
 */
int ps_input_config_remove_similar_maptm(struct ps_input_config *config,struct ps_input_maptm *maptm);

#endif
