/* ps_resmgr.h
 * Resource Manager is responsible for the game's static assets.
 */

#ifndef PS_RESMGR_H
#define PS_RESMGR_H

struct ps_restype;

int ps_resmgr_init(const char *path,int edit);
void ps_resmgr_quit();

/* Drop all resources and reread from disk.
 */
int ps_resmgr_reload();

int ps_resmgr_clear();

#define PS_RESTYPE_TILESHEET      0
#define PS_RESTYPE_IMAGE          1
#define PS_RESTYPE_BLUEPRINT      2
#define PS_RESTYPE_SPRDEF         3
#define PS_RESTYPE_REGION         4
#define PS_RESTYPE_PLRDEF         5
#define PS_RESTYPE_TRDEF          6
#define PS_RESTYPE_IPCM           7
#define PS_RESTYPE_SONG           8
#define PS_RESTYPE_COUNT          9

struct ps_restype *ps_resmgr_get_type_by_id(int tid);
#define PS_RESTYPE(tag) ps_resmgr_get_type_by_id(PS_RESTYPE_##tag)
struct ps_restype *ps_resmgr_get_type_by_name(const char *name,int namec);

void *ps_res_get(int tid,int rid);

// Reverse lookup.
int ps_res_get_id_by_obj(int tid,const void *obj);

/* This is potentially unsafe. Only for use by the editor.
 * And even then, be careful!
 */
int ps_res_replace(int tid,int rid,void *obj_HANDOFF);

/* If we were loaded from loose files, identify the file for resource (tid,rid).
 * (only_if_existing) nonzero means we will respond with an existing file or fail.
 * If zero, we make up a path.
 */
int ps_res_get_path_for_resource(char *dst,int dsta,int tid,int rid,int only_if_existing);

/* Take all resources and bundle them into a compressed archive.
 * You can open archives just like directories (but can't edit from them).
 * See below for format.
 */
int ps_res_export_archive(const char *path);

/*------------------------------------------------------------------------
 * ARCHIVE FORMAT
 *
 * File is a single zlib stream.
 *
 * Uncompressed stream starts with this header:
 *   0000   8 Signature: "\0PLSQ\xffRA"
 *   0008   8 Reserved
 *   0010
 *
 * After the archive header are resource blocks, each is a header then body.
 * Resource header:
 *   0000   2 Identifier:
 *              f000 Type ID
 *              0fff Resource ID
 *   0002   3 Uncompressed length
 *   0005
 *
 *------------------------------------------------------------------------
 */

#endif
