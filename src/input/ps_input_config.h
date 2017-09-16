/* ps_input_config.h
 * Container for templates for input mapping, and logic for serializing them.
 */

#ifndef PS_INPUT_CONFIG_H
#define PS_INPUT_CONFIG_H

struct ps_input_config;

struct ps_input_config *ps_input_config_new();
void ps_input_config_del(struct ps_input_config *config);

int ps_input_config_set_path(struct ps_input_config *config,const char *src,int srcc);
const char *ps_input_config_get_path(const struct ps_input_config *config);

#endif
