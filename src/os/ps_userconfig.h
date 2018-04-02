/* ps_userconfig.h
 * User's configuration, stored to a file and updated from the command line or UI.
 */

#ifndef PS_USERCONFIG_H
#define PS_USERCONFIG_H

#define PS_USERCONFIG_TYPE_BOOLEAN    1
#define PS_USERCONFIG_TYPE_INTEGER    2
#define PS_USERCONFIG_TYPE_PATH       3
#define PS_USERCONFIG_TYPE_STRING     4

struct ps_userconfig;
struct ps_buffer;

struct ps_userconfig *ps_userconfig_new();
void ps_userconfig_del(struct ps_userconfig *userconfig);
int ps_userconfig_ref(struct ps_userconfig *userconfig);

int ps_userconfig_set_first_existing_path(struct ps_userconfig *userconfig,const char *path,...);
int ps_userconfig_set_path(struct ps_userconfig *userconfig,const char *path);

int ps_userconfig_load_file(struct ps_userconfig *userconfig);

int ps_userconfig_save_file(struct ps_userconfig *userconfig);
int ps_userconfig_encode(struct ps_buffer *output,const struct ps_userconfig *userconfig);

int ps_userconfig_count_fields(const struct ps_userconfig *userconfig);
int ps_userconfig_search_field(const struct ps_userconfig *userconfig,const char *k,int kc); // => fldp
int ps_userconfig_get_field_declaration(int *type,const char **k,const struct ps_userconfig *userconfig,int fldp);
int ps_userconfig_get_field_as_int(const struct ps_userconfig *userconfig,int fldp); // BOOLEAN or INTEGER only
int ps_userconfig_get_field_as_string(char *dst,int dsta,const struct ps_userconfig *userconfig,int fldp); // any type
int ps_userconfig_peek_field_as_string(void *dstpp,const struct ps_userconfig *userconfig,int fldp); // STRING or PATH only

int ps_userconfig_set_field_as_int(struct ps_userconfig *userconfig,int fldp,int src); // BOOLEAN, INTEGER, or STRING
int ps_userconfig_set_field_as_string(struct ps_userconfig *userconfig,int fldp,const char *src,int srcc); // any type
int ps_userconfig_set(struct ps_userconfig *userconfig,const char *k,int kc,const char *v,int vc);

int ps_userconfig_declare_boolean(struct ps_userconfig *userconfig,const char *k,int v);
int ps_userconfig_declare_integer(struct ps_userconfig *userconfig,const char *k,int v,int lo,int hi);
int ps_userconfig_declare_string(struct ps_userconfig *userconfig,const char *k,const char *v,int vc);
int ps_userconfig_declare_path(struct ps_userconfig *userconfig,const char *k,const char *v,int vc,int must_exist);

#endif
