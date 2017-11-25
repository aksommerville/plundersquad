/* akau_store.h
 * Storage and decoding of data.
 * We have separate lists of ipcm, fpcm, instrument, and song.
 * Within each list, every resource has a unique positive ID.
 */

#ifndef AKAU_STORE_H
#define AKAU_STORE_H

struct akau_store;
struct akau_ipcm;
struct akau_fpcm;
struct akau_instrument;
struct akau_song;

struct akau_store *akau_store_new();
void akau_store_del(struct akau_store *store);
int akau_store_ref(struct akau_store *store);

/* Remove all loaded resources.
 */
int akau_store_clear(struct akau_store *store);

/* Load resources from an archive file or directory.
 * Existing content is preserved.
 * See below for formatting.
 */
int akau_store_load(struct akau_store *store,const char *path);

/* Get a WEAK reference to a loaded resource.
 */
struct akau_ipcm *akau_store_get_ipcm(const struct akau_store *store,int ipcmid);
struct akau_fpcm *akau_store_get_fpcm(const struct akau_store *store,int fpcmid);
struct akau_instrument *akau_store_get_instrument(const struct akau_store *store,int instrumentid);
struct akau_song *akau_store_get_song(const struct akau_store *store,int songid);

/* Install a resource.
 */
int akau_store_add_ipcm(struct akau_store *store,struct akau_ipcm *ipcm,int ipcmid);
int akau_store_add_fpcm(struct akau_store *store,struct akau_fpcm *fpcm,int fpcmid);
int akau_store_add_instrument(struct akau_store *store,struct akau_instrument *instrument,int instrumentid);
int akau_store_add_song(struct akau_store *store,struct akau_song *song,int songid);

int akau_store_replace_ipcm(struct akau_store *store,struct akau_ipcm *ipcm,int ipcmid);
int akau_store_replace_fpcm(struct akau_store *store,struct akau_fpcm *fpcm,int fpcmid);
int akau_store_replace_instrument(struct akau_store *store,struct akau_instrument *instrument,int instrumentid);
int akau_store_replace_song(struct akau_store *store,struct akau_song *song,int songid);

/* Get resources sequentially.
 */
 
int akau_store_count_ipcm(const struct akau_store *store);
struct akau_ipcm *akau_store_get_ipcm_by_index(const struct akau_store *store,int p);
int akau_store_get_ipcm_id_by_index(const struct akau_store *store,int p);
int akau_store_get_unused_ipcm_id(int *id,int *p,const struct akau_store *store);

int akau_store_count_fpcm(const struct akau_store *store);
struct akau_fpcm *akau_store_get_fpcm_by_index(const struct akau_store *store,int p);
int akau_store_get_fpcm_id_by_index(const struct akau_store *store,int p);
int akau_store_get_unused_fpcm_id(int *id,int *p,const struct akau_store *store);

int akau_store_count_instrument(const struct akau_store *store);
struct akau_instrument *akau_store_get_instrument_by_index(const struct akau_store *store,int p);
int akau_store_get_instrument_id_by_index(const struct akau_store *store,int p);
int akau_store_get_unused_instrument_id(int *id,int *p,const struct akau_store *store);

int akau_store_count_song(const struct akau_store *store);
struct akau_song *akau_store_get_song_by_index(const struct akau_store *store,int p);
int akau_store_get_song_id_by_index(const struct akau_store *store,int p);
int akau_store_get_unused_song_id(int *id,int *p,const struct akau_store *store);

/* Return path to the file for this resource.
 * If store was loaded from an archive or was built programmatically, we fail.
 * "get" returns only existing files.
 * "generate" composes a default name; use it if "get" fails and you want to create the file.
 */
int akau_store_get_resource_path(char *dst,int dsta,const struct akau_store *store,const char *restype,int resid);
int akau_store_generate_resource_path(char *dst,int dsta,const struct akau_store *store,const char *restype,int resid);

/******************************************************************************
 * ARCHIVE FORMAT
 *-----------------------------------------------------------------------------
 * File is compressed with zlib.
 * Uncompressed content begins with an 8-byte signature:
 *   \x00AKAUAR\xff
 * After that are loose files, each beginning with an 8-byte header (big-endian):
 *   1  Resource type:
 *       1  ipcm
 *       2  fpcm
 *       3  instrument
 *       4  song
 *   1  Resource format:
 *       1  WAV (ipcm,fpcm,instrument)
 *       2  AKAU PCM (ipcm,fpcm,instrument)
 *       3  AKAU song text (song)
 *       4  AKAU instrument text (instrument)
 *       5  Harmonic coefficients (fpcm)
 *       6  AKAU WaveGen (ipcm,fpcm)
 *   2  Resource ID.
 *   4  Uncompressed length.
 * The compressed length is not stored; we assume you will decompress the whole
 * archive in one pass.
 * Duplicate IDs are an error.
 *****************************************************************************/

/******************************************************************************
 * DIRECTORY FORMAT
 *-----------------------------------------------------------------------------
 * Top level of the directory contains a directory for each resource type:
 *   ipcm,fpcm,instrument,song
 * Directly under each of those are regular files, one per resource.
 * Each resource file must begin with the decimal resource ID.
 * Files without a leading ID are ignored.
 * Duplicate IDs are an error.
 * We will guess the file format based on path extension, resource type, and file content.
 *****************************************************************************/

#endif
