#include "ps_alsa.h"
#include "os/ps_emergency_abort.h"
#include <stdio.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

// Size of hardware buffer in frames (ie 1/44100 of second).
// Lower values increase likelihood of underrun.
// Higher values uniformly increase latency.
// Must be a power of two.
// I find 2048 works, but haven't tested it under heavy load yet.
#define PS_ALSA_BUFFER_SIZE 2048

/* Globals.
 */

static struct {
  void (*cb)(int16_t *dst,int dstc);

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int rate;
  int chanc;
  int hwbuffersize;
  int bufc; // frames
  int bufc_samples;
  int16_t *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort; // Extra cancellation flag for tidy shutdown (not technically necessary).
  
} ps_alsa={0};

/* I/O thread.
 */

static void *ps_alsa_iothd(void *dummy) {
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
  while (1) {
    pthread_testcancel();
    ps_emergency_abort_set_message("Refilling audio buffer.");
    
    // fill buffer
    if (pthread_mutex_lock(&ps_alsa.iomtx)) return 0;
    ps_alsa.cb(ps_alsa.buf,ps_alsa.bufc_samples);
    pthread_mutex_unlock(&ps_alsa.iomtx);

    // dump buffer to alsa
    int16_t *samplev=ps_alsa.buf;
    int samplep=0,samplec=ps_alsa.bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      ps_emergency_abort_set_message("Sending audio to ALSA.");
      int err=snd_pcm_writei(ps_alsa.alsa,samplev+samplep,samplec-samplep);
      if (ps_alsa.ioabort) return 0;
      if (err<=0) {
        if ((err=snd_pcm_recover(ps_alsa.alsa,err,0))<0) {
          fprintf(stderr,"ps: snd_pcm_writei: %d (%s)\n",err,snd_strerror(err));
          return 0;
        }
        ps_emergency_abort_set_message("Recovered from pcm write error.");
        break;
      }
      samplep+=err;
    }
  }
  return 0;
}

/* Init.
 */
 
#ifndef PS_ALSA_DEVICE
  #define PS_ALSA_DEVICE "default"
#endif

int ps_alsa_init(int rate,int chanc,void (*cb)(int16_t *dst,int dstac)) {
  if (!cb) return -1;
  memset(&ps_alsa,0,sizeof(ps_alsa));
  ps_alsa.cb=cb;

  ps_alsa.rate=rate;
  ps_alsa.chanc=chanc;
  const char *device=PS_ALSA_DEVICE;

  if (snd_pcm_open(&ps_alsa.alsa,device,SND_PCM_STREAM_PLAYBACK,0)<0) return -1;
  if (snd_pcm_hw_params_malloc(&ps_alsa.hwparams)<0) return -1;
  if (snd_pcm_hw_params_any(ps_alsa.alsa,ps_alsa.hwparams)<0) return -1;
  if (snd_pcm_hw_params_set_access(ps_alsa.alsa,ps_alsa.hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0) return -1;
  if (snd_pcm_hw_params_set_format(ps_alsa.alsa,ps_alsa.hwparams,SND_PCM_FORMAT_S16)<0) return -1;
  if (snd_pcm_hw_params_set_rate_near(ps_alsa.alsa,ps_alsa.hwparams,&rate,0)<0) return -1;
  if (snd_pcm_hw_params_set_channels_near(ps_alsa.alsa,ps_alsa.hwparams,&chanc)<0) return -1;
  if (snd_pcm_hw_params_set_buffer_size(ps_alsa.alsa,ps_alsa.hwparams,PS_ALSA_BUFFER_SIZE)<0) return -1;
  if (snd_pcm_hw_params(ps_alsa.alsa,ps_alsa.hwparams)<0) return -1;

  if (snd_pcm_nonblock(ps_alsa.alsa,0)<0) return -1;
  if (snd_pcm_prepare(ps_alsa.alsa)<0) return -1;
  ps_alsa.rate=rate;
  ps_alsa.chanc=chanc;
  ps_alsa.bufc=PS_ALSA_BUFFER_SIZE;
  ps_alsa.bufc_samples=PS_ALSA_BUFFER_SIZE*chanc;
  ps_alsa.cb=cb;
  if (!(ps_alsa.buf=malloc(chanc*2*ps_alsa.bufc))) return -1;

  { pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&ps_alsa.iomtx,&mattr)) return -1;
    pthread_mutexattr_destroy(&mattr);
    if (pthread_create(&ps_alsa.iothd,0,ps_alsa_iothd,0)) return -1;
  }
  
  return 0;
}

/* Quit.
 */

void ps_alsa_quit() {

  ps_alsa.ioabort=1;
  if (ps_alsa.iothd) {
    ps_emergency_abort_set_message("Cancelling audio thread.");
    pthread_cancel(ps_alsa.iothd);
    ps_emergency_abort_set_message("Joining audio thread.");
    pthread_join(ps_alsa.iothd,0);
  }
  pthread_mutex_destroy(&ps_alsa.iomtx);
  
  if (ps_alsa.hwparams) snd_pcm_hw_params_free(ps_alsa.hwparams);
  if (ps_alsa.alsa) {
    ps_emergency_abort_set_message("Closing ALSA connection. a");
    snd_pcm_close(ps_alsa.alsa);
  }
  if (ps_alsa.buf) free(ps_alsa.buf);

  memset(&ps_alsa,0,sizeof(ps_alsa));
  ps_emergency_abort_set_message("ALSA cleanup complete.");
}

/* Lock.
 */

int ps_alsa_lock() {
  if (pthread_mutex_lock(&ps_alsa.iomtx)) return -1;
  return 0;
}

int ps_alsa_unlock() {
  if (pthread_mutex_unlock(&ps_alsa.iomtx)) return -1;
  return 0;
}

/* AKAU glue.
 */

#include "akau/akau_driver.h"

const struct akau_driver akau_driver_alsa={
  .init=ps_alsa_init,
  .quit=ps_alsa_quit,
  .lock=ps_alsa_lock,
  .unlock=ps_alsa_unlock,
};
