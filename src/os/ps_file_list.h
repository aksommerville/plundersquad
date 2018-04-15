/* ps_file_list.h
 * Ordered list of paths, so we can easily find "first existing" and such.
 * One ps_file_list contains alternates for a single final path.
 */

#ifndef PS_FILE_LIST_H
#define PS_FILE_LIST_H

struct ps_file_list;

struct ps_file_list *ps_file_list_new();
void ps_file_list_del(struct ps_file_list *list);
int ps_file_list_ref(struct ps_file_list *list);

/* Return the first path in this list meeting the given criteria.
 */
const char *ps_file_list_get_first_existing_file(const struct ps_file_list *list);
const char *ps_file_list_get_first_existing_directory(const struct ps_file_list *list);
const char *ps_file_list_get_first_existing_file_or_directory(const struct ps_file_list *list);
const char *ps_file_list_get_first_existing_parent(const struct ps_file_list *list);

/* Direct sequential access to the list (probably not useful).
 */
int ps_file_list_count(const struct ps_file_list *list);
const char *ps_file_list_get_by_index(const struct ps_file_list *list,int p);

/* Add a path.
 * (p) will typically be -1 (end) or 0 (beginning).
 * We don't check for redundancy, formatting, or anything like that.
 */
int ps_file_list_add(struct ps_file_list *list,int p,const char *src,int srcc);

#endif
