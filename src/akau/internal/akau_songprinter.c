#include "akau_internal.h"
#include "../akau_songprinter.h"
#include "../akau_mixer.h"
#include "../akau_song.h"
#include "../akau_pcm.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define HAVE_STRUCT_TIMESPEC 1
#include <pthread.h>
#include "os/ps_log.h"

/* Object definition.
 */

struct akau_songprinter {
  int refc;
  struct akau_song *song;
  struct akau_mixer *mixer;
  struct akau_ipcm *ipcm;
  pthread_t thread;
  pthread_mutex_t mutex;
  int progress;
  int thread_in_flight;
};

/* Initialize.
 */

static int akau_songprinter_init(struct akau_songprinter *printer,struct akau_song *song) {

  ps_log(AUDIO,DEBUG,"%s %p song=%p...",__func__,printer,song);

  if (akau_song_ref(song)<0) return -1;
  printer->song=song;
  
  if (!(printer->mixer=akau_mixer_new())) return -1;
  if (akau_mixer_set_stereo(printer->mixer,0)<0) return -1;
  if (akau_mixer_set_print_songs(printer->mixer,0)<0) return -1; // Very important!

  ps_log(AUDIO,DEBUG,"%s allocated inner mixer, measuring song...",__func__);

  int beatc=akau_song_count_beats(song);
  if (beatc<1) return -1;
  int tempo=akau_song_get_tempo(song);
  if (tempo<1) return -1;
  int rate=akau_get_master_rate();
  if (rate<1) return -1;
  if (rate>INT_MAX/tempo) return -1;
  int samples_per_beat=(rate*60)/tempo;
  if (samples_per_beat<1) return -1;
  int samplec=beatc*samples_per_beat;
  if (!(printer->ipcm=akau_ipcm_new(samplec))) {
    ps_log(AUDIO,ERROR,"Failed to allocate %d-sample IPCM buffer for song.",samplec);
    return -1;
  }

  if (pthread_mutex_init(&printer->mutex,0)) return -1;

  ps_log(AUDIO,DEBUG,"%s ok",__func__);

  return 0;
}

/* Object lifecycle.
 */
 
struct akau_songprinter *akau_songprinter_new(struct akau_song *song) {
  if (!song) return 0;
  struct akau_songprinter *printer=calloc(1,sizeof(struct akau_songprinter));
  if (!printer) return 0;

  printer->refc=1;
  if (akau_songprinter_init(printer,song)<0) {
    akau_songprinter_del(printer);
    return 0;
  }

  return printer;
}

void akau_songprinter_del(struct akau_songprinter *printer) {
  if (!printer) return;
  if (printer->refc-->1) return;

  ps_log(AUDIO,DEBUG,"%s %p",__func__,printer);

  if (printer->thread_in_flight) {
    ps_log(AUDIO,DEBUG,"Kill in-flight printer thread.");
    pthread_cancel(printer->thread);
    pthread_join(printer->thread,0);
    ps_log(AUDIO,DEBUG,"...killed");
  }

  akau_song_del(printer->song);
  akau_ipcm_del(printer->ipcm);
  akau_mixer_del(printer->mixer);
  pthread_mutex_destroy(&printer->mutex);

  free(printer);
}

int akau_songprinter_ref(struct akau_songprinter *printer) {
  if (!printer) return -1;
  if (printer->refc<1) return -1;
  if (printer->refc==INT_MAX) return -1;
  printer->refc++;
  return 0;
}

/* Accessors.
 */
 
struct akau_song *akau_songprinter_get_song(const struct akau_songprinter *printer) {
  if (!printer) return 0;
  return printer->song;
}

int akau_songprinter_get_progress(const struct akau_songprinter *printer) {
  if (!printer) return AKAU_SONGPRINTER_PROGRESS_ERROR;
  return printer->progress;
}

struct akau_ipcm *akau_songprinter_get_ipcm(const struct akau_songprinter *printer) {
  if (akau_songprinter_get_progress(printer)!=AKAU_SONGPRINTER_PROGRESS_READY) return 0;
  return printer->ipcm;
}

struct akau_ipcm *akau_songprinter_get_ipcm_even_if_incomplete(const struct akau_songprinter *printer) {
  if (!printer) return 0;
  return printer->ipcm;
}

/* Asynchronous printing (background thread main).
 * We don't make just one call to akau_mixer_update() like the synchronous version.
 * Because we need to insert cancellation points at reasonable intervals.
 * The mixer doesn't really care about the buffer sizes we pass in, it's all good.
 */

static void *akau_songprinter_bgthd(void *arg) {
  struct akau_songprinter *printer=arg;
  
  int16_t *samplev=akau_ipcm_get_sample_buffer(printer->ipcm);
  int samplec=akau_ipcm_get_sample_count(printer->ipcm);
  if (!samplev||(samplec<1)) {
    printer->progress=AKAU_SONGPRINTER_PROGRESS_ERROR;
    return 0;
  }

  ps_log(AUDIO,DEBUG,"Begin asynchronous song print...");

  int samplep=0;
  while (samplep<samplec) {

    pthread_testcancel();

    int progress=(samplep*100)/samplec;
    if (progress<1) progress=1; else if (progress>99) progress=99;
    printer->progress=progress;
    
    int subsamplec=10000;
    if (samplep>samplec-subsamplec) subsamplec=samplec-samplep;
    if (akau_mixer_update(samplev+samplep,subsamplec,printer->mixer)<0) {
      ps_log(AUDIO,ERROR,"Error from akau_mixer_update() during asynchronous song print.");
      printer->progress=AKAU_SONGPRINTER_PROGRESS_ERROR;
      return 0;
    }
    samplep+=subsamplec;
    
  }
  ps_log(AUDIO,DEBUG,"...asynchronous song print complete");
  printer->progress=AKAU_SONGPRINTER_PROGRESS_READY;
  return 0;
}

/* Begin asynchronous print.
 */

int akau_songprinter_begin(struct akau_songprinter *printer) {
  if (!printer) return -1;
  if (printer->progress!=AKAU_SONGPRINTER_PROGRESS_INIT) return -1;
  if (akau_mixer_play_song(printer->mixer,printer->song,1,0)<0) return -1;
  printer->progress=1;
  printer->thread_in_flight=1;
  ps_log(AUDIO,DEBUG,"Spawning thread for asynchronous song print.");
  if (pthread_create(&printer->thread,0,akau_songprinter_bgthd,printer)) {
    printer->progress=0;
    printer->thread_in_flight=0;
    return -1;
  }
  return 0;
}

/* Cancel asynchronous print.
 */
 
int akau_songprinter_cancel(struct akau_songprinter *printer) {
  if (!printer) return -1;
  if ((printer->progress>0)&&(printer->progress<100)) {
    pthread_cancel(printer->thread);
    pthread_join(printer->thread,0);
    printer->progress=0;
    printer->thread_in_flight=0;
  }
  return 0;
}

/* Print synchronously or block until complete.
 */
 
int akau_songprinter_finish(struct akau_songprinter *printer) {
  if (!printer) return -1;

  /* Already complete? */
  if (!printer->thread_in_flight&&(printer->progress>=100)) return 0;

  /* Failed out? */
  if (printer->progress<0) return -1;

  /* If asynchronous printing is in progress, join it.
   */
  if (printer->thread_in_flight) {
    pthread_join(printer->thread,0);
    printer->thread_in_flight=0;
    if (printer->progress!=AKAU_SONGPRINTER_PROGRESS_READY) return -1;
    return 0;
  }
  
  /* Print song synchronously.
   */
  if (akau_mixer_play_song(printer->mixer,printer->song,1,0)<0) return -1;
  int16_t *samplev=akau_ipcm_get_sample_buffer(printer->ipcm);
  if (!samplev) return -1;
  int samplec=akau_ipcm_get_sample_count(printer->ipcm);
  if (samplec<1) return -1;
  if (akau_mixer_update(samplev,samplec,printer->mixer)<0) return -1;
  printer->progress=AKAU_SONGPRINTER_PROGRESS_READY;
  return 0;
}
