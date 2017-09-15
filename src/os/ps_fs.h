/* ps_fs.h
 * Helpers for reading and writing files.
 */

#ifndef PS_FS_H
#define PS_FS_H

int ps_file_read(void *dstpp,const char *path);
int ps_file_write(const char *path,const void *src,int srcc);

#endif
