/* akau_songprinter.h
 * Coordinator for generating a flat IPCM from a song.
 */

#ifndef AKAU_SONGPRINTER_H
#define AKAU_SONGPRINTER_H

struct akau_songprinter;
struct akau_ipcm;
struct akau_song;

struct akau_songprinter *akau_songprinter_new(struct akau_song *song);
void akau_songprinter_del(struct akau_songprinter *printer);
int akau_songprinter_ref(struct akau_songprinter *printer);

struct akau_song *akau_songprinter_get_song(const struct akau_songprinter *printer);

int akau_songprinter_get_progress(const struct akau_songprinter *printer);
#define AKAU_SONGPRINTER_PROGRESS_ERROR      -1 /* Invalid request or printing failed. */
#define AKAU_SONGPRINTER_PROGRESS_INIT        0 /* Valid object, printing not started. */
/* 1..99 mean asynchronous printing is in progress. */
#define AKAU_SONGPRINTER_PROGRESS_READY     100 /* Printing complete. */

/* Use "begin" to spin off a background thread for asynchronous printing.
 * That will return immediately.
 * Once started, use "cancel" to discard all work or "finish" to block until complete.
 * "finish" without "begin" is legal; that requests to print the song synchronously.
 * In that case, a background thread is never started.
 */
int akau_songprinter_begin(struct akau_songprinter *printer);
int akau_songprinter_cancel(struct akau_songprinter *printer);
int akau_songprinter_finish(struct akau_songprinter *printer);

/* If progress is READY, return a weak reference to the output IPCM.
 * The IPCM object can exist before that time, but we won't return it yet.
 */
struct akau_ipcm *akau_songprinter_get_ipcm(const struct akau_songprinter *printer);

#endif
