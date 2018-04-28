#include "akau_mixer_internal.h"
#include "../akau.h"
#include "../akau_songprinter.h"
#include "os/ps_log.h"
#include "os/ps_clockassist.h"

/* Clean up channel.
 */
 
void akau_mixer_chan_cleanup(struct akau_mixer_chan *chan) {
  if (!chan) return;
  switch (chan->mode) {
    case AKAU_MIXER_CHAN_MODE_TUNED: {
        akau_instrument_del(chan->tuned.instrument);
      } break;
    case AKAU_MIXER_CHAN_MODE_VERBATIM: {
        akau_ipcm_del(chan->verbatim.ipcm);
      } break;
  }
  memset(chan,0,sizeof(struct akau_mixer_chan));
}

/* Reallocate channel list.
 */
 
int akau_mixer_chanv_realloc(struct akau_mixer *mixer) {
  if (!mixer) return -1;
  int na=mixer->chana+16;
  if (na>INT_MAX/sizeof(struct akau_mixer_chan)) return -1;
  void *nv=realloc(mixer->chanv,sizeof(struct akau_mixer_chan)*na);
  if (!nv) return -1;
  mixer->chanv=nv;
  mixer->chana=na;
  return 0;
}

/* Search channel by id.
 * Because we reuse memory but don't reuse IDs, it's impractical to keep the list sorted.
 * So this is a straight linear search.
 */
 
int akau_mixer_chanv_search(const struct akau_mixer *mixer,int chanid) {
  if (!mixer) return -1;
  if (chanid<1) return -1;
  const struct akau_mixer_chan *chan=mixer->chanv;
  int p=0; for (;p<mixer->chanc;p++,chan++) {
    if (chan->chanid==chanid) return p;
  }
  return -1;
}

/* Mixer lifecycle.
 */

struct akau_mixer *akau_mixer_new() {
  struct akau_mixer *mixer=calloc(1,sizeof(struct akau_mixer));
  if (!mixer) return 0;

  mixer->refc=1;
  mixer->stereo=1;
  mixer->chanid_next=1;
  memset(mixer->trim_by_intent,0xff,sizeof(mixer->trim_by_intent));
  mixer->print_songs=1;

  return mixer;
}

void akau_mixer_del(struct akau_mixer *mixer) {
  if (!mixer) return;
  if (mixer->refc-->1) return;

  if (mixer->chanv) {
    while (mixer->chanc-->0) akau_mixer_chan_cleanup(mixer->chanv+mixer->chanc);
    free(mixer->chanv);
  }

  if (mixer->song) {
    akau_song_unlock(mixer->song);
    akau_song_del(mixer->song);
  }

  akau_songprinter_del(mixer->printer);

  free(mixer);
}

int akau_mixer_ref(struct akau_mixer *mixer) {
  if (!mixer) return -1;
  if (mixer->refc<1) return -1;
  if (mixer->refc==INT_MAX) return -1;
  mixer->refc++;
  return 0;
}

/* Accessors.
 */
 
int akau_mixer_count_channels(const struct akau_mixer *mixer) {
  if (!mixer) return 0;
  int c=0,i=mixer->chanc;
  while (i-->0) if (mixer->chanv[i].mode) c++;
  return c;
}

int akau_mixer_set_stereo(struct akau_mixer *mixer,int stereo) {
  if (!mixer) return -1;
  mixer->stereo=stereo?1:0;
  return 0;
}

int akau_mixer_set_print_songs(struct akau_mixer *mixer,int print) {
  if (!mixer) return -1;
  mixer->print_songs=print?1:0;
  return 0;
}

int akau_mixer_get_print_songs(const struct akau_mixer *mixer) {
  if (!mixer) return 0;
  return mixer->print_songs;
}

/* Update trim and pan sliders.
 */

static int akau_mixer_chan_update_shared_sliders(struct akau_mixer_chan *chan) {

  if (chan->trimc) {
    if (++(chan->trimp)>=chan->trimc) {
      chan->trim=chan->trimz;
      chan->trimc=0;
      if (!chan->trim&&chan->stop_at_silence) {
        akau_mixer_chan_cleanup(chan);
        return 0;
      }
    } else {
      chan->trim=chan->trima+(chan->trimp*(chan->trimz-chan->trima))/chan->trimc;
    }
  }

  if (chan->panc) {
    if (++(chan->panp)>=chan->panc) {
      chan->pan=chan->panz;
      chan->panc=0;
    } else {
      chan->pan=chan->pana+(chan->panp*(chan->panz-chan->pana))/chan->panc;
    }
  }

  return 0;
}

/* Update tuned channel.
 */

static inline uint8_t akau_mixer_tuned_calculate_amplitude(int p,int c,uint8_t a,uint8_t z) {
  return a+((p*(z-a))/c);
}

static int akau_mixer_chan_tuned_update(struct akau_mixer_chan *chan) {

  /* Update PCM position. */
  chan->tuned.p+=chan->tuned.dp;
  int ip=(int)chan->tuned.p;
  while (ip>=chan->tuned.instrument->ipcm->c) {
    chan->tuned.p-=chan->tuned.instrument->ipcm->c;
    ip=(int)chan->tuned.p;
  }
  int normsample=chan->tuned.instrument->ipcm->v[ip];

  /* Update rate slider. */
  if (chan->tuned.dpc) {
    if (++(chan->tuned.dpp)>=chan->tuned.dpc) {
      chan->tuned.dp=chan->tuned.dpz;
      chan->tuned.dpc=0;
    } else {
      chan->tuned.dp=chan->tuned.dpa+(chan->tuned.dpp*(chan->tuned.dpz-chan->tuned.dpa))/chan->tuned.dpc;
    }
  }

  /* Examine lifecycle and amplify accordingly. */
  uint8_t amp;
  chan->tuned.lcp++;
  if (chan->tuned.lcp>=chan->tuned.lcp_end) {
    akau_mixer_chan_cleanup(chan);
    return 0;
  } else if (chan->tuned.lcp>=chan->tuned.lcp_decay) { // fading out
    int phasep=chan->tuned.lcp-chan->tuned.lcp_decay;
    int phasec=chan->tuned.lcp_end-chan->tuned.lcp_decay;
    uint8_t ampa=chan->tuned.amp_drawback;
    uint8_t ampz=0;
    amp=akau_mixer_tuned_calculate_amplitude(phasep,phasec,ampa,ampz);
  } else if (chan->tuned.lcp>=chan->tuned.lcp_drawback) { // holding steady
    amp=chan->tuned.amp_drawback;
  } else if (chan->tuned.lcp>=chan->tuned.lcp_attack) { // pulling back
    int phasep=chan->tuned.lcp-chan->tuned.lcp_attack;
    int phasec=chan->tuned.lcp_drawback-chan->tuned.lcp_attack;
    uint8_t ampa=chan->tuned.amp_attack;
    uint8_t ampz=chan->tuned.amp_drawback;
    amp=akau_mixer_tuned_calculate_amplitude(phasep,phasec,ampa,ampz);
  } else { // ramping up
    int phasep=chan->tuned.lcp;
    int phasec=chan->tuned.lcp_attack;
    uint8_t ampa=0;
    uint8_t ampz=chan->tuned.amp_attack;
    amp=akau_mixer_tuned_calculate_amplitude(phasep,phasec,ampa,ampz);
  }
  normsample=(normsample*amp)>>8;
  
  return normsample;
}

/* Update verbatim channel.
 */

static int akau_mixer_chan_verbatim_update(struct akau_mixer_chan *chan) {

  if (chan->verbatim.p>=chan->verbatim.ipcm->c) {
    akau_mixer_chan_cleanup(chan);
    return 0;
  }
  int sample=chan->verbatim.ipcm->v[chan->verbatim.p];
  
  chan->verbatim.p++;

  if (chan->verbatim.loop) {
    if (chan->verbatim.ipcm->loopap>=0) {
      if (chan->verbatim.p>=chan->verbatim.ipcm->loopzp) {
        chan->verbatim.p=chan->verbatim.ipcm->loopap;
      }
    } else {
      if (chan->verbatim.p>=chan->verbatim.ipcm->c) {
        chan->verbatim.p=0;
      }
    }
  } else {
    if (chan->verbatim.p>=chan->verbatim.ipcm->c) {
      akau_mixer_chan_cleanup(chan);
    }
  }

  return sample;
}

/* Check progress of songprinter, during main update.
 */

static int akau_mixer_check_printer_progress(struct akau_mixer *mixer) {
  int progress=akau_songprinter_get_progress(mixer->printer);
  if (progress<0) {
    akau_songprinter_del(mixer->printer);
    mixer->printer=0;
    return 0;
  }
  if (progress==AKAU_SONGPRINTER_PROGRESS_READY) {
    int64_t now=ps_time_now();
    int64_t elapsed=now-mixer->print_start_time;
    ps_log(AUDIO,DEBUG,"%lld song print complete, elapsed %d.%06d",now,(int)(elapsed/1000000),(int)(elapsed%1000000));
    struct akau_ipcm *ipcm=akau_songprinter_get_ipcm(mixer->printer);
    if (!ipcm) return -1;
    if (akau_mixer_play_ipcm(mixer,ipcm,0xff,0,1,AKAU_INTENT_BGM)<0) return -1;
    mixer->printed_song_running=1;

  // Experiment: If we're more than 50% complete and at least 4 seconds have elapsed, start playing it.
  // This means we could potentially read IPCM samples not written yet (they would be zero).
  // I think the odds of that are vanishingly unlikely. 
  } else if (progress>=50) {
    int64_t now=ps_time_now();
    if (now>=mixer->print_start_time+4000000) {
      ps_log(AUDIO,DEBUG,"Starting song playbback at %d%% printed.",progress);
      struct akau_ipcm *ipcm=akau_songprinter_get_ipcm_even_if_incomplete(mixer->printer);
      if (!ipcm) return -1;
      if (akau_mixer_play_ipcm(mixer,ipcm,0xff,0,1,AKAU_INTENT_BGM)<0) return -1;
      mixer->printed_song_running=1;
    }
  }
  
  return 0;
}

/* Update.
 */

int akau_mixer_update(int16_t *dst,int dstc,struct akau_mixer *mixer) {
  if (!dst||(dstc<2)||!mixer) return -1;
  if (mixer->stereo) dstc&=~1;

  /* If we are printing a song, check its progress.
   * Only do this at the start of the update cycle, no need to poll it every frame.
   */
  if (mixer->printer&&!mixer->printed_song_running) {
    if (akau_mixer_check_printer_progress(mixer)<0) return -1;
  }
  
  while (dstc>0) {
    int l=0,r=0;

    /* Update unprinted song if present. */
    if (mixer->song) {
      if (mixer->song_delay>0) {
        mixer->song_delay--;
      } else {
        if (akau_song_update(mixer->song,mixer,mixer->song_cmdp,mixer->song_intent)<0) return -1;
      }
    }

    /* Update channels. */
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      int sample=0;

      if (akau_mixer_chan_update_shared_sliders(chan)<0) return -1;

      switch (chan->mode) {
        case AKAU_MIXER_CHAN_MODE_TUNED: sample=akau_mixer_chan_tuned_update(chan); break;
        case AKAU_MIXER_CHAN_MODE_VERBATIM: sample=akau_mixer_chan_verbatim_update(chan); break;
      }
      if (!sample) continue;

      uint8_t itrim=mixer->trim_by_intent[chan->intent];
      if (!itrim) continue;
      if (itrim!=0xff) {
        sample=(sample*itrim)>>8;
        if (!sample) continue;
      }

      sample=(sample*chan->trim)>>8;
      if (!sample) continue;

      if (!mixer->stereo) {
        l+=sample;
      } else if (!chan->pan) {
        l+=sample;
        r+=sample;
      } else if (chan->pan==-128) {
        l+=sample;
      } else if (chan->pan==127) {
        r+=sample;
      } else if (chan->pan<0) {
        l+=sample;
        r+=(sample*(-128-chan->pan))>>7;
      } else {
        l+=(sample*(127-chan->pan))>>7;
        r+=sample;
      }

    }

    /* Clip and output. */
    if (l<-32768) {
      *dst=-32768;
      mixer->cliplc++;
    } else if (l>32767) {
      *dst=32767; 
      mixer->cliplc++;
    } else *dst=l;
    dst++;
    dstc--;
    if (mixer->stereo) {
      if (r<-32768) {
        *dst=-32768;
        mixer->cliprc++;
      } else if (r>32767) {
        *dst=32767; 
        mixer->cliprc++;
      } else *dst=r;
      dst++;
      dstc--;
    }
    
  }
  return 0;
}

/* Allocate channel.
 */

static struct akau_mixer_chan *akau_mixer_chan_new(struct akau_mixer *mixer) {
  struct akau_mixer_chan *chan=0;

  if (mixer->chanc<mixer->chana) {
    chan=mixer->chanv+mixer->chanc++;
    memset(chan,0,sizeof(struct akau_mixer_chan));
    goto _assign_id_;
  }

  { chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      if (chan->mode==AKAU_MIXER_CHAN_MODE_SILENT) {
        goto _assign_id_;
      }
    }
  }

  if (akau_mixer_chanv_realloc(mixer)<0) return 0;
  chan=mixer->chanv+mixer->chanc++;
  memset(chan,0,sizeof(struct akau_mixer_chan));

 _assign_id_:
  chan->chanid=mixer->chanid_next;
  if (mixer->chanid_next==INT_MAX) {
    mixer->chanid_next=1;
  } else {
    mixer->chanid_next++;
  }
  
  return chan;
}

/* Set up tuned channel.
 */

static inline uint8_t akau_mixer_8bit_trim(double src) {
  src*=255.0;
  if (src<=0.0) return 0;
  if (src>=255.0) return 255;
  return (uint8_t)src;
}

int akau_mixer_play_note(
  struct akau_mixer *mixer,
  struct akau_instrument *instrument,
  uint8_t pitch,
  uint8_t trim,
  int8_t pan,
  int duration,
  uint8_t intent
) {
  if (!instrument) return -1;
  if (duration<1) return 0;
  struct akau_mixer_chan *chan=akau_mixer_chan_new(mixer);
  if (!chan) return -1;

  if (akau_instrument_ref(instrument)<0) {
    chan->chanid=0;
    return -1;
  }

  chan->mode=AKAU_MIXER_CHAN_MODE_TUNED;
  chan->stop_at_silence=0;
  chan->trim=trim;
  chan->trimc=0;
  chan->pan=pan;
  chan->panc=0;
  chan->intent=intent;

  chan->tuned.instrument=instrument;
  chan->tuned.p=0.0;
  chan->tuned.dp=akau_rate_from_pitch(pitch);
  chan->tuned.dpc=0;
  chan->tuned.pitch=pitch;

  chan->tuned.lcp=0;
  chan->tuned.lcp_attack=instrument->attack_time;
  chan->tuned.lcp_drawback=instrument->attack_time+instrument->drawback_time;
  chan->tuned.lcp_decay=duration;
  chan->tuned.lcp_end=duration+instrument->decay_time;
  if (chan->tuned.lcp_attack<1) chan->tuned.lcp_attack=1;
  if (chan->tuned.lcp_drawback<=chan->tuned.lcp_attack) chan->tuned.lcp_drawback=chan->tuned.lcp_attack+1;
  if (chan->tuned.lcp_decay<=chan->tuned.lcp_drawback) chan->tuned.lcp_decay=chan->tuned.lcp_drawback+1;
  if (chan->tuned.lcp_end<=chan->tuned.lcp_decay) chan->tuned.lcp_end=chan->tuned.lcp_decay+1;
  
  chan->tuned.amp_attack=akau_mixer_8bit_trim(instrument->attack_trim);
  chan->tuned.amp_drawback=akau_mixer_8bit_trim(instrument->drawback_trim);

  return chan->chanid;
}

/* Set up verbatim channel.
 */

int akau_mixer_play_ipcm(
  struct akau_mixer *mixer,
  struct akau_ipcm *ipcm,
  uint8_t trim,
  int8_t pan,
  int loop,
  uint8_t intent
) {
  if (!ipcm) return -1;
  struct akau_mixer_chan *chan=akau_mixer_chan_new(mixer);
  if (!chan) return -1;

  if (akau_ipcm_ref(ipcm)<0) {
    chan->chanid=0;
    return -1;
  }

  chan->mode=AKAU_MIXER_CHAN_MODE_VERBATIM;
  chan->stop_at_silence=0;
  chan->trim=trim;
  chan->trimc=0;
  chan->pan=pan;
  chan->panc=0;
  chan->intent=intent;

  chan->verbatim.ipcm=ipcm;
  chan->verbatim.p=0;
  chan->verbatim.loop=loop;

  return chan->chanid;
}

/* Get properties for a given channel.
 */

int akau_mixer_get_channel(
  uint8_t *pitch,uint8_t *trim,int8_t *pan,
  const struct akau_mixer *mixer,
  int chanid
) {
  int chanp=akau_mixer_chanv_search(mixer,chanid);
  if (chanp<0) return -1;
  struct akau_mixer_chan *chan=mixer->chanv+chanp;
  if (pitch) {
    if (chan->mode==AKAU_MIXER_CHAN_MODE_TUNED) {
      *pitch=chan->tuned.pitch;
    } else {
      *pitch=0;
    }
  }
  if (trim) *trim=chan->trim;
  if (pan) *pan=chan->pan;
  return 0;
}

/* Set sliders with positive duration.
 */

static int akau_mixer_chan_set_sliders(struct akau_mixer_chan *chan,uint8_t pitch,uint8_t trim,int8_t pan,int duration) {

  if (chan->trim==trim) {
    chan->trimc=0;
  } else {
    chan->trima=chan->trim;
    chan->trimz=trim;
    chan->trimp=0;
    chan->trimc=duration;
  }

  if (chan->pan==pan) {
    chan->panc=0;
  } else {
    chan->pana=chan->pan;
    chan->panz=pan;
    chan->panp=0;
    chan->panc=duration;
  }

  if (chan->mode==AKAU_MIXER_CHAN_MODE_TUNED) {
    if (chan->tuned.pitch==pitch) {
      if (chan->tuned.dpc>0) {
        chan->tuned.dp=chan->tuned.dpz;
        chan->tuned.dpc=0;
      }
    } else {
      chan->tuned.dpa=chan->tuned.dp;
      chan->tuned.dpz=akau_rate_from_pitch(pitch);
      chan->tuned.dpp=0;
      chan->tuned.dpc=duration;
    }
  }

  return 0;
}

/* Set sliders instantly.
 */

static int akau_mixer_chan_set_properties(struct akau_mixer_chan *chan,uint8_t pitch,uint8_t trim,int8_t pan) {
  chan->pan=pan;
  chan->panc=0;
  chan->trim=trim;
  chan->trimc=0;
  if (chan->mode==AKAU_MIXER_CHAN_MODE_TUNED) {
    chan->tuned.dp=akau_rate_from_pitch(pitch);
    chan->tuned.dpc=0;
  }
  return 0;
}

/* Set sliders, public interface.
 */

int akau_mixer_adjust_channel(
  struct akau_mixer *mixer,
  int chanid,
  uint8_t pitch,uint8_t trim,int8_t pan,
  int duration
) {
  int chanp=akau_mixer_chanv_search(mixer,chanid);
  if (chanp<0) return -1;
  struct akau_mixer_chan *chan=mixer->chanv+chanp;
  if (duration>0) {
    return akau_mixer_chan_set_sliders(chan,pitch,trim,pan,duration);
  } else {
    return akau_mixer_chan_set_properties(chan,pitch,trim,pan);
  }
}

/* Stop channel gracefully.
 */
 
int akau_mixer_stop_channel(struct akau_mixer *mixer,int chanid) {
  int chanp=akau_mixer_chanv_search(mixer,chanid);
  if (chanp<0) return -1;
  struct akau_mixer_chan *chan=mixer->chanv+chanp;
  switch (chan->mode) {

    case AKAU_MIXER_CHAN_MODE_TUNED: {
        if (chan->tuned.lcp<chan->tuned.lcp_decay) chan->tuned.lcp=chan->tuned.lcp_decay;
      } break;

    case AKAU_MIXER_CHAN_MODE_VERBATIM: {
        chan->verbatim.loop=0;
      } break;

  }
  return 0;
}

/* Silence everything.
 */
 
int akau_mixer_stop_all(struct akau_mixer *mixer,int duration) {
  if (!mixer) return -1;
  if (duration>0) {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      chan->stop_at_silence=1;
      chan->trima=chan->trim;
      chan->trimz=0;
      chan->trimp=0;
      chan->trimc=duration;
    }
  } else {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      akau_mixer_chan_cleanup(chan);
    }
  }
  return 0;
}
 
int akau_mixer_stop_all_instruments(struct akau_mixer *mixer,int duration) {
  if (!mixer) return -1;
  if (duration>0) {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      if (chan->mode!=AKAU_MIXER_CHAN_MODE_TUNED) continue;
      chan->stop_at_silence=1;
      chan->trima=chan->trim;
      chan->trimz=0;
      chan->trimp=0;
      chan->trimc=duration;
    }
  } else {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      if (chan->mode!=AKAU_MIXER_CHAN_MODE_TUNED) continue;
      akau_mixer_chan_cleanup(chan);
    }
  }
  return 0;
}

int akau_mixer_stop_by_intent(struct akau_mixer *mixer,uint8_t intent,int duration) {
  if (!mixer) return -1;
  if (duration>0) {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      if (chan->intent!=intent) continue;
      chan->stop_at_silence=1;
      chan->trima=chan->trim;
      chan->trimz=0;
      chan->trimp=0;
      chan->trimc=duration;
    }
  } else {
    struct akau_mixer_chan *chan=mixer->chanv;
    int i=mixer->chanc; for (;i-->0;chan++) {
      if (chan->mode!=AKAU_MIXER_CHAN_MODE_TUNED) continue;
      akau_mixer_chan_cleanup(chan);
    }
  }
  return 0;
}

/* Restart playback of IPCM channel from the songprinter.
 */

static int akau_mixer_restart_songprinter(struct akau_mixer *mixer) {
  const struct akau_ipcm *ipcm=akau_songprinter_get_ipcm(mixer->printer);
  if (!ipcm) return 0;
  struct akau_mixer_chan *chan=mixer->chanv;
  int i=mixer->chanc; for (;i-->0;chan++) {
    if (chan->intent!=AKAU_INTENT_BGM) continue;
    if (chan->mode!=AKAU_MIXER_CHAN_MODE_VERBATIM) continue;
    if (chan->verbatim.ipcm!=ipcm) continue;
    chan->verbatim.p=0;
  }
  return 0;
}

/* Begin new song with printing enabled.
 * TODO: If we toggle fast between two songs, we might switch to one that's still fading out. Can we keep that IPCM instead of reprinting?
 */

static int akau_mixer_register_song_for_printing(struct akau_mixer *mixer,struct akau_song *song,int restart,uint8_t intent) {

  if (mixer->printer) {
  
    /* Already playing this song. Restart IPCM or do nothing. */
    if (akau_songprinter_get_song(mixer->printer)==song) {
      if (restart&&(akau_songprinter_get_progress(mixer->printer)==AKAU_SONGPRINTER_PROGRESS_READY)) {
        if (akau_mixer_restart_songprinter(mixer)<0) return -1;
      }
      return 0;
    }

    /* Cancel existing printer. */
    if (akau_songprinter_cancel(mixer->printer)<0) return -1;
    akau_songprinter_del(mixer->printer);
    mixer->printer=0;
  }

  /* Slowly fade out any current song. */
  if (akau_mixer_stop_by_intent(mixer,intent,44100)<0) return -1;
  mixer->printed_song_running=0;

  /* Switching to silence? We're done. */
  if (!song) return 0;

  /* Create a songprinter and begin printing. */
  mixer->print_start_time=ps_time_now();
  ps_log(AUDIO,DEBUG,"%lld begin printing song",mixer->print_start_time);
  if (!(mixer->printer=akau_songprinter_new(song))) return -1;
  if (akau_songprinter_begin(mixer->printer)<0) return -1;

  return 0;
}

/* Begin new song, printing disabled.
 */

static int akau_mixer_replace_unprinted_song(struct akau_mixer *mixer,struct akau_song *song,int restart,uint8_t intent) {

  /* Abort quick if we won't be doing anything. */
  if ((song==mixer->song)&&!restart) return 0;

  /* Begin winding down all current tuned channels. */
  if (akau_mixer_stop_by_intent(mixer,intent,250)<0) return -1;

  /* Only stop current song? */
  if (!song) {
    if (mixer->song) {
      akau_song_unlock(mixer->song);
      akau_song_del(mixer->song);
      mixer->song=0;
    }
    return 0;
  }

  /* Song already playing? */
  if (song==mixer->song) {
    if (restart) {
      mixer->song_delay=0;
      mixer->song_cmdp=0;
    }
    return 0;
  }

  /* Stop current song. */
  if (mixer->song) {
    akau_song_unlock(mixer->song);
    akau_song_del(mixer->song);
    mixer->song=0;
  }
  mixer->song_delay=0;
  mixer->song_cmdp=0;

  /* Retain, lock, and install the new song. */
  if (akau_song_ref(song)<0) return -1;
  if (akau_song_restart(song)<0) {
    akau_song_del(song);
    return -1;
  }
  if (akau_song_lock(song)<0) {
    akau_song_del(song);
    return -1;
  }
  mixer->song=song;
  mixer->song_intent=intent;

  return 0;
}

/* Set song.
 */
 
int akau_mixer_play_song(struct akau_mixer *mixer,struct akau_song *song,int restart,uint8_t intent) {
  if (!mixer) return -1;
  if (mixer->print_songs) {
    return akau_mixer_register_song_for_printing(mixer,song,restart,intent);
  } else {
    return akau_mixer_replace_unprinted_song(mixer,song,restart,intent);
  }
}

/* Play song from beat.
 */

int akau_mixer_play_song_from_beat(struct akau_mixer *mixer,struct akau_song *song,int beatp,uint8_t intent) {
  if (!mixer||!song) return -1;

  /* Get the target command position. */
  int cmdp=akau_song_cmdp_from_beatp(song,beatp);
  if (cmdp<0) return -1;

  /* Stop current song. */
  if (mixer->song) {
    akau_song_unlock(mixer->song);
    akau_song_del(mixer->song);
    mixer->song=0;
  }

  /* Retain, lock, and install the new song. */
  if (akau_song_restart_at_beat(song,beatp)<0) return -1;
  if (akau_song_ref(song)<0) return -1;
  if (akau_song_lock(song)<0) {
    akau_song_del(song);
    return -1;
  }
  mixer->song_delay=0;
  mixer->song_cmdp=cmdp;
  mixer->song=song;
  mixer->song_intent=intent;

  return 0;
}

/* Song support.
 */
 
int akau_mixer_set_song_delay(struct akau_mixer *mixer,int framec) {
  if (!mixer) return -1;
  if (framec<0) return -1;
  mixer->song_delay=framec;
  return 0;
}

int akau_mixer_set_song_position(struct akau_mixer *mixer,int cmdp) {
  if (!mixer) return -1;
  if (cmdp<0) return -1;
  mixer->song_cmdp=cmdp;
  return 0;
}

/* Get clip.
 */
 
int akau_mixer_get_clip(int *l,int *r,struct akau_mixer *mixer) {
  if (!mixer) return -1;
  if (l) *l=mixer->cliplc;
  if (r) *r=mixer->cliprc;
  mixer->cliplc=0;
  mixer->cliprc=0;
  return 0;
}

/* Intent.
 */
 
int akau_mixer_set_trim_for_intent(struct akau_mixer *mixer,uint8_t intent,uint8_t trim) {
  if (!mixer) return -1;
  mixer->trim_by_intent[intent]=trim;
  return 0;
}

uint8_t akau_mixer_get_trim_for_intent(const struct akau_mixer *mixer,uint8_t intent) {
  if (!mixer) return 0;
  return mixer->trim_by_intent[intent];
}
