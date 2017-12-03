#include "akau_song_internal.h"
#include "../akau_pcm.h"
#include "../akau_instrument.h"
#include "../akau_store.h"

/* Clean up minor objects.
 */

static void akau_song_drum_cleanup(struct akau_song_drum *drum) {
  akau_ipcm_del(drum->ipcm);
}

static void akau_song_instrument_cleanup(struct akau_song_instrument *instr) {
  if (instr->serial) free(instr->serial);
  akau_instrument_del(instr->instrument);
}

/* New.
 */

struct akau_song *akau_song_new() {
  struct akau_song *song=calloc(1,sizeof(struct akau_song));
  if (!song) return 0;

  song->refc=1;
  song->tempo=100;
  song->frames_per_beat=(akau_get_master_rate()*60)/song->tempo;

  return song;
}

/* Delete.
 */
 
void akau_song_del(struct akau_song *song) {
  if (!song) return;
  if (song->refc-->1) return;

  if (song->drumv) {
    while (song->drumc-->0) akau_song_drum_cleanup(song->drumv+song->drumc);
    free(song->drumv);
  }
  if (song->instrv) {
    while (song->instrc-->0) akau_song_instrument_cleanup(song->instrv+song->instrc);
    free(song->instrv);
  }
  if (song->cmdv) {
    free(song->cmdv);
  }

  free(song);
}

/* Retain.
 */
 
int akau_song_ref(struct akau_song *song) {
  if (!song) return -1;
  if (song->refc<1) return -1;
  if (song->refc==INT_MAX) return -1;
  song->refc++;
  return 0;
}

/* Clear.
 */

int akau_song_clear(struct akau_song *song) {
  if (!song) return -1;
  song->tempo=100;
  song->frames_per_beat=(akau_get_master_rate()*60)/song->tempo;
  while (song->drumc>0) {
    song->drumc--;
    akau_song_drum_cleanup(song->drumv+song->drumc);
  }
  while (song->instrc>0) {
    song->instrc--;
    akau_song_instrument_cleanup(song->instrv+song->instrc);
  }
  song->cmdc=0;
  return 0;
}

/* Lock.
 */

static int akau_song_assert_lockable(const struct akau_song *song) {
  if (!song) return -1;

  /* At least one command must exist, and the last command must be BEAT. */
  if (song->cmdc<1) return -1;
  if (song->cmdv[song->cmdc-1].op!=AKAU_SONG_OP_BEAT) return -1;

  /* All drums must be linked. */
  const struct akau_song_drum *drum=song->drumv;
  int i=song->drumc;
  for (;i-->0;drum++) {
    if (!drum->ipcm) return -1;
  }
  
  return 0;
}

int akau_song_get_lock(const struct akau_song *song) {
  if (!song) return 0;
  return song->lock?1:0;
}

int akau_song_lock(struct akau_song *song) {
  if (!song) return -1;
  if (song->lock==INT_MAX) return -1;
  if (!song->lock) {
    if (akau_song_assert_lockable(song)<0) return -1;
  }
  song->lock++;
  return 0;
}

int akau_song_unlock(struct akau_song *song) {
  if (!song) return -1;
  if (song->lock<1) return -1;
  song->lock--;
  return 0;
}

/* Tempo.
 */

int akau_song_get_tempo(const struct akau_song *song) {
  if (!song) return -1;
  return song->tempo;
}

int akau_song_set_tempo(struct akau_song *song,int tempo_bpm) {
  if (!song) return -1;
  if (song->lock) return -1;
  if (tempo_bpm<1) return -1;
  if (tempo_bpm>0xffff) return -1;
  song->tempo=tempo_bpm;
  song->frames_per_beat=(akau_get_master_rate()*60)/song->tempo;
  return 0;
}

/* Grow buffers.
 */

static int akau_song_drumv_require(struct akau_song *song) {
  if (song->drumc>=255) return -1;
  if (song->drumc<song->druma) return 0;
  int na=song->druma+8;
  void *nv=realloc(song->drumv,sizeof(struct akau_song_drum)*na);
  if (!nv) return -1;
  song->drumv=nv;
  song->druma=na;
  return 0;
}

static int akau_song_instrv_require(struct akau_song *song) {
  if (song->instrc>=255) return -1;
  if (song->instrc<song->instra) return 0;
  int na=song->instra+8;
  void *nv=realloc(song->instrv,sizeof(struct akau_song_instrument)*na);
  if (!nv) return -1;
  song->instrv=nv;
  song->instra=na;
  return 0;
}

static int akau_song_cmdv_require(struct akau_song *song) {
  if (song->cmdc<song->cmda) return 0;
  int na=song->cmda+32;
  if (na>INT_MAX/sizeof(union akau_song_command)) return -1;
  void *nv=realloc(song->cmdv,sizeof(union akau_song_command)*na);
  if (!nv) return -1;
  song->cmdv=nv;
  song->cmda=na;
  return 0;
}

/* Drum list.
 */

int akau_song_count_drums(const struct akau_song *song) {
  if (!song) return 0;
  return song->drumc;
}

struct akau_ipcm *akau_song_get_drum(const struct akau_song *song,int drumid) {
  if (!song) return 0;
  if ((drumid<0)||(drumid>=song->drumc)) return 0;
  return song->drumv[drumid].ipcm;
}

int akau_song_get_drum_ipcmid(const struct akau_song *song,int drumid) {
  if (!song) return 0;
  if ((drumid<0)||(drumid>=song->drumc)) return -1;
  return song->drumv[drumid].ipcmid;
}

int akau_song_set_drum(struct akau_song *song,int drumid,int ipcmid) {
  if (!song) return -1;
  if (song->lock) return -1;
  if ((drumid<0)||(drumid>=song->drumc)) return -1;
  if (ipcmid<0) return -1;
  struct akau_song_drum *drum=song->drumv+drumid;
  if (drum->ipcmid==ipcmid) return 0;
  drum->ipcmid=ipcmid;
  akau_ipcm_del(drum->ipcm);
  drum->ipcm=0;
  return 0;
}

int akau_song_add_drum(struct akau_song *song,int ipcmid) {
  if (!song) return -1;
  if (song->lock) return -1;
  if (ipcmid<0) return -1;
  if (akau_song_drumv_require(song)<0) return -1;
  struct akau_song_drum *drum=song->drumv+song->drumc;
  memset(drum,0,sizeof(struct akau_song_drum));
  drum->ipcmid=ipcmid;
  return song->drumc++;
}

int akau_song_remove_drum(struct akau_song *song,int drumid) {
  if (!song) return -1;
  if (song->lock) return -1;
  if ((drumid<0)||(drumid>=song->drumc)) return -1;
  struct akau_song_drum *drum=song->drumv+drumid;
  akau_song_drum_cleanup(drum);
  song->drumc--;
  memmove(drum,drum+1,sizeof(struct akau_song_drum)*(song->drumc-drumid));
  return 0;
}

/* Instrument list.
 */

int akau_song_count_instruments(const struct akau_song *song) {
  if (!song) return 0;
  return song->instrc;
}

struct akau_instrument *akau_song_get_instrument(const struct akau_song *song,int instrid) {
  if (!song) return 0;
  if ((instrid<0)||(instrid>=song->instrc)) return 0;
  return song->instrv[instrid].instrument;
}

int akau_song_get_instrument_source(void *dstpp,const struct akau_song *song,int instrid) {
  if (!dstpp||!song) return -1;
  if ((instrid<0)||(instrid>=song->instrc)) return -1;
  *(void**)dstpp=song->instrv[instrid].serial;
  return song->instrv[instrid].serialc;
}

int akau_song_set_instrument(struct akau_song *song,int instrid,const void *src,int srcc) {
  if (!song) return -1;
  if (song->lock) return -1;
  if ((instrid<0)||(instrid>=song->instrc)) return -1;
  struct akau_song_instrument *instr=song->instrv+instrid;
  struct akau_instrument *instrument=akau_instrument_decode_binary(src,srcc);
  if (!instrument) return -1;
  akau_instrument_del(instr->instrument);
  instr->instrument=instrument;
  return 0;
}

int akau_song_add_instrument(struct akau_song *song,const void *src,int srcc) {
  if (!song) return -1;
  if (!src||(srcc<1)) return -1;
  if (song->lock) return -1;
  if (akau_song_instrv_require(song)<0) return -1;
  struct akau_song_instrument *instr=song->instrv+song->instrc;
  memset(instr,0,sizeof(struct akau_song_instrument));
  if (!(instr->instrument=akau_instrument_decode_binary(src,srcc))) return -1;
  if (!(instr->serial=malloc(srcc))) {
    akau_instrument_del(instr->instrument);
    return -1;
  }
  memcpy(instr->serial,src,srcc);
  instr->serialc=srcc;
  return song->instrc++;
}

int akau_song_remove_instrument(struct akau_song *song,int instrid) {
  if (!song) return -1;
  if (song->lock) return -1;
  if ((instrid<0)||(instrid>=song->instrc)) return -1;
  struct akau_song_instrument *instr=song->instrv+instrid;
  akau_song_instrument_cleanup(instr);
  song->instrc--;
  memmove(instr,instr+1,sizeof(struct akau_song_instrument)*(song->instrc-instrid));
  return 0;
}

/* Command list.
 */

int akau_song_count_commands(const struct akau_song *song) {
  if (!song) return 0;
  return song->cmdc;
}

int akau_song_get_command(union akau_song_command *dst,const struct akau_song *song,int cmdp) {
  if (!dst||!song) return -1;
  if ((cmdp<0)||(cmdp>=song->cmdc)) return -1;
  memcpy(dst,song->cmdv+cmdp,sizeof(union akau_song_command));
  return 0;
}

int akau_song_set_command(struct akau_song *song,int cmdp,const union akau_song_command *src) {
  if (!song||!src) return -1;
  if (song->lock) return -1;
  if ((cmdp<0)||(cmdp>=song->cmdc)) return -1;
  memcpy(song->cmdv+cmdp,src,sizeof(union akau_song_command));
  return 0;
}

int akau_song_insert_command(struct akau_song *song,int cmdp,const union akau_song_command *src) {
  if (!song||!src) return -1;
  if (song->lock) return -1;
  if ((cmdp<0)||(cmdp>song->cmdc)) cmdp=song->cmdc;
  if (akau_song_cmdv_require(song)<0) return -1;
  union akau_song_command *cmd=song->cmdv+cmdp;
  memmove(cmd+1,cmd,sizeof(union akau_song_command)*(song->cmdc-cmdp));
  memcpy(cmd,src,sizeof(union akau_song_command));
  song->cmdc++;
  return 0;
}

int akau_song_remove_commands(struct akau_song *song,int cmdp,int cmdc) {
  if (!song) return -1;
  if (song->lock) return -1;
  if (cmdp<0) return -1;
  if (cmdc<0) return -1;
  if (cmdp>song->cmdc-cmdc) return -1;
  song->cmdc-=cmdc;
  memmove(song->cmdv+cmdp,song->cmdv+cmdp+cmdc,sizeof(union akau_song_command)*(song->cmdc-cmdp));
  return 0;
}

int akau_song_get_all_commands(union akau_song_command **dstpp,const struct akau_song *song) {
  if (!dstpp||!song) return -1;
  *dstpp=song->cmdv;
  return song->cmdc;
}

/* Beat counting.
 */

int akau_song_beatp_from_cmdp(const struct akau_song *song,int cmdp) {
  if (!song) return -1;
  if ((cmdp<0)||(cmdp>=song->cmdc)) return -1;
  int beatp=0;
  const union akau_song_command *cmd=song->cmdv;
  for (;cmdp-->0;cmd++) {
    if (cmd->op==AKAU_SONG_OP_BEAT) beatp++;
  }
  return beatp;
}

int akau_song_cmdp_from_beatp(const struct akau_song *song,int beatp) {
  if (!song) return -1;
  if (beatp<0) return -1;
  if (!beatp) return 0;
  const union akau_song_command *cmd=song->cmdv;
  int cmdp=0;
  for (;cmdp<song->cmdc;cmdp++,cmd++) {
    if (cmd->op==AKAU_SONG_OP_BEAT) {
      if (--beatp<0) return cmdp;
    }
  }
  return -1;
}

int akau_song_cmdp_advance(const struct akau_song *song,int cmdp) {
  if (!song) return -1;
  if (cmdp<0) return 0;
  if (cmdp>=song->cmdc) return song->cmdc;
  const union akau_song_command *cmd=song->cmdv+cmdp;
  while (cmdp<song->cmdc) {
    cmdp++;
    if (cmd->op==AKAU_SONG_OP_BEAT) return cmdp;
    cmd++;
  }
  // This should not happen, because the last command should be BEAT. But whatever.
  return song->cmdc;
}

int akau_song_cmdp_retreat(const struct akau_song *song,int cmdp) {
  if (!song) return -1;
  if (cmdp<=0) return 0;
  cmdp--;
  while ((cmdp>0)&&(song->cmdv[cmdp-1].op!=AKAU_SONG_OP_BEAT)) cmdp--;
  return cmdp;
}

/* Link.
 */
 
int akau_song_link(struct akau_song *song,struct akau_store *store) {
  if (!song||!store) return -1;
  struct akau_song_drum *drum=song->drumv;
  int i=song->drumc; for (;i-->0;drum++) {
    if (drum->ipcm) continue;
    if (!(drum->ipcm=akau_store_get_ipcm(store,drum->ipcmid))) return -1;
    if (akau_ipcm_ref(drum->ipcm)<0) {
      drum->ipcm=0;
      return -1;
    }
  }
  return 0;
}
