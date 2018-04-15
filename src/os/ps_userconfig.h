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
struct ps_file_list;

struct ps_userconfig *ps_userconfig_new();
void ps_userconfig_del(struct ps_userconfig *userconfig);
int ps_userconfig_ref(struct ps_userconfig *userconfig);

/* IoC provider should create a userconfig, then set up these defaults.
 * After that, add and remove fields to tune it to the given system.
 *
 * The default fields are:
 *   PATH resources = ""
 *   PATH input = ""
 *   BOOLEAN fullscreen = 1
 *   BOOLEAN soft-render = 0
 *   INTEGER music = 255
 *   INTEGER sound = 255
 *
 * Other global command-line options which are not actually userconfig:
 *   PATH reopen-tty = ""
 *   PATH chdir = ""
 *   PATH config = ""
 */
int ps_userconfig_declare_default_fields(struct ps_userconfig *userconfig);

/* There are three files external to the application: config, input, and data.
 * Each has a ps_file_list where you can configure possible paths in order.
 */
struct ps_file_list *ps_userconfig_get_config_file_list(const struct ps_userconfig *userconfig);
struct ps_file_list *ps_userconfig_get_input_file_list(const struct ps_userconfig *userconfig);
struct ps_file_list *ps_userconfig_get_data_file_list(const struct ps_userconfig *userconfig);

/* Set values for "input" and "resources" based on the file lists.
 * Call this after populating the userconfig.
 */
int ps_userconfig_commit_paths(struct ps_userconfig *userconfig);

/* Path for the main config file "plundersquad.cfg".
 * ps_userconfig_acquire_path() ensures that path is set by consulting our file list as needed.
 * With (reset) nonzero, any previously acquired path is discarded; I don't think we'll actually need that.
 * All of this is automatic in normal cases. Just populate the "config_file_list" before loading or saving config.
 */
int ps_userconfig_set_path(struct ps_userconfig *userconfig,const char *path);
const char *ps_userconfig_get_path(const struct ps_userconfig *userconfig);
int ps_userconfig_acquire_path(struct ps_userconfig *userconfig,int reset);

/* Read a text file where each line is a single configuration item or blank.
 * Hash begins a line comment.
 * Format is "KEY VALUE" with an optional separator ':' or '='.
 * Unknown keys are an error.
 * You must first establish the file's path, see above.
 * Reading from a file that doesn't exist is *not* an error, but will log a warning.
 */
int ps_userconfig_load_file(struct ps_userconfig *userconfig);
int ps_userconfig_decode(struct ps_userconfig *userconfig,const char *src,int srcc);

/* Set fields directly from (argc,argv).
 * We ignore the first argument (presumably executable name).
 * We ignore any NULL or empty arguments -- you can preprocess this and eliminate special arguments.
 * Returns >0 if anything was set, in which case you should save me.
 */
int ps_userconfig_load_argv(struct ps_userconfig *userconfig,int argc,char **argv);

/* Encode the entire configuration to text and rewrite the file.
 * ps_userconfig_save_file() tracks our state and will quickly do nothing if we're still clean.
 */
int ps_userconfig_save_file(struct ps_userconfig *userconfig);
int ps_userconfig_encode(struct ps_buffer *output,const struct ps_userconfig *userconfig);
int ps_userconfig_set_dirty(struct ps_userconfig *userconfig,int dirty);

/* Take an existing encoded file and rewrite with keys from this userconfig.
 * The basic rules:
 *  - Comments and whitespace in (src) are preserved.
 *  - If a key is defined with the same value but commented, we uncomment that line and comment out any others.
 *  - Otherwise, first uncommented definition of a key, we replace its value.
 *  - Otherwise, if a key is present but commented, we insert the definition below.
 *  - If a key is not found at all, we append at the bottom.
 *  - If input contains an invalid key, we comment it out.
 */
int ps_userconfig_reencode(struct ps_buffer *dst,const char *src,int srcc,const struct ps_userconfig *userconfig);

/* Helpers to read the set of declared fields, and their current values.
 */
int ps_userconfig_count_fields(const struct ps_userconfig *userconfig);
int ps_userconfig_search_field(const struct ps_userconfig *userconfig,const char *k,int kc); // => fldp
int ps_userconfig_get_field_declaration(int *type,const char **k,const struct ps_userconfig *userconfig,int fldp);
int ps_userconfig_get_field_as_int(const struct ps_userconfig *userconfig,int fldp); // BOOLEAN or INTEGER only
int ps_userconfig_get_field_as_string(char *dst,int dsta,const struct ps_userconfig *userconfig,int fldp); // any type
int ps_userconfig_peek_field_as_string(void *dstpp,const struct ps_userconfig *userconfig,int fldp); // STRING or PATH only

/* Convenient getters.
 * Only work for the correct types.
 */
int ps_userconfig_get_int(const struct ps_userconfig *userconfig,const char *k,int kc);
const char *ps_userconfig_get_str(const struct ps_userconfig *userconfig,const char *k,int kc);

/* Helpers to set fields.
 * IoC should in order:
 *  - Set defaults.
 *  - Read the config file.
 *  - Read "--KEY=VALUE" arguments from the command line, with ps_userconfig_set().
 */
int ps_userconfig_set_field_as_int(struct ps_userconfig *userconfig,int fldp,int src); // BOOLEAN, INTEGER, or STRING
int ps_userconfig_set_field_as_string(struct ps_userconfig *userconfig,int fldp,const char *src,int srcc); // any type
int ps_userconfig_set(struct ps_userconfig *userconfig,const char *k,int kc,const char *v,int vc);

/* Declare a new field.
 */
int ps_userconfig_declare_boolean(struct ps_userconfig *userconfig,const char *k,int v);
int ps_userconfig_declare_integer(struct ps_userconfig *userconfig,const char *k,int v,int lo,int hi);
int ps_userconfig_declare_string(struct ps_userconfig *userconfig,const char *k,const char *v,int vc);
int ps_userconfig_declare_path(struct ps_userconfig *userconfig,const char *k,const char *v,int vc,int must_exist);

/* Remove a declared field.
 * You might like to undeclare some defaults right after construction, if they are not appropriate to the platform.
 */
int ps_userconfig_undeclare(struct ps_userconfig *userconfig,const char *k);

#endif
