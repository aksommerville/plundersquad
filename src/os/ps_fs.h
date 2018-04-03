/* ps_fs.h
 * Helpers for reading and writing files.
 */

#ifndef PS_FS_H
#define PS_FS_H

int ps_file_read(void *dstpp,const char *path);
int ps_file_write(const char *path,const void *src,int srcc);

int ps_mkdir_parents(const char *path);
int ps_mkdir(const char *path);

int ps_os_reopen_tty(const char *path);

struct ps_zlib_file;
struct ps_zlib_file *ps_zlib_open(const char *path,int output);
void ps_zlib_close(struct ps_zlib_file *file);
int ps_zlib_read(void *dst,int dsta,struct ps_zlib_file *file);
int ps_zlib_write(struct ps_zlib_file *file,const void *src,int srcc);
int ps_zlib_write_end(struct ps_zlib_file *file);

#endif
