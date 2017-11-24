#include "akau_song_internal.h"
#include "../akau_store.h"

int akau_queue_sync_token(uint16_t token);

/* Object lifecycle.
 */

struct akau_song *akau_song_new() {
  struct akau_song *song=calloc(1,sizeof(struct akau_song));
  if (!song) return 0;

  song->refc=1;

  return song;
}

void akau_song_del(struct akau_song *song) {
  if (!song) return;
  if (song->refc-->1) return;

  if (song->instrumentv) {
    while (song->instrumentc-->0) akau_instrument_del(song->instrumentv[song->instrumentc]);
    free(song->instrumentv);
  }
  if (song->ipcmv) {
    while (song->ipcmc-->0) akau_ipcm_del(song->ipcmv[song->ipcmc]);
    free(song->ipcmv);
  }
  if (song->cmdv) free(song->cmdv);
  if (song->referencev) free(song->referencev);

  free(song);
}

int akau_song_ref(struct akau_song *song) {
  if (!song) return -1;
  if (song->refc<1) return -1;
  if (song->refc==INT_MAX) return -1;
  song->refc++;
  return 0;
}

/* Lock.
 */

int akau_song_lock(struct akau_song *song) {
  if (!song) return -1;
  if (song->lock<0) return -1;
  if (song->lock==INT_MAX) return -1;
  if (song->referencec) return -1;
  song->lock++;
  return 0;
}

int akau_song_unlock(struct akau_song *song) {
  if (!song) return -1;
  if (song->lock<1) return -1;
  song->lock--;
  return 0;
}

/* Reallocate buffers.
 */
 
static int akau_song_instrumentv_require(struct akau_song *song) {
  if (!song) return -1;
  if (song->instrumentc<song->instrumenta) return 0;
  if (song->instrumentc>=256) return -1;
  int na=song->instrumenta+8;
  void *nv=realloc(song->instrumentv,sizeof(void*)*na);
  if (!nv) return -1;
  song->instrumentv=nv;
  song->instrumenta=na;
  return 0;
}

static int akau_song_ipcmv_require(struct akau_song *song) {
  if (!song) return -1;
  if (song->ipcmc<song->ipcma) return 0;
  if (song->ipcmc>=256) return -1;
  int na=song->ipcma+8;
  void *nv=realloc(song->ipcmv,sizeof(void*)*na);
  if (!nv) return -1;
  song->ipcmv=nv;
  song->ipcma=na;
  return 0;
}

static int akau_song_cmdv_require(struct akau_song *song) {
  if (!song) return -1;
  if (song->cmdc<song->cmda) return 0;
  int na=song->cmda+32;
  if (na>INT_MAX/sizeof(struct akau_song_cmd)) return -1;
  void *nv=realloc(song->cmdv,sizeof(struct akau_song_cmd)*na);
  if (!nv) return -1;
  song->cmdv=nv;
  song->cmda=na;
  return 0;
}

static int akau_song_referencev_require(struct akau_song *song) {
  if (!song) return -1;
  if (song->referencec<song->referencea) return 0;
  int na=song->referencea+8;
  if (na>INT_MAX/sizeof(struct akau_song_reference)) return -1;
  void *nv=realloc(song->referencev,sizeof(struct akau_song_reference)*na);
  if (!nv) return -1;
  song->referencev=nv;
  song->referencea=na;
  return 0;
}

/* Add objects.
 */
 
int akau_song_add_instrument(struct akau_song *song,struct akau_instrument *instrument) {
  if (!song||!instrument) return -1;
  if (song->lock) return -1;
  if (akau_song_instrumentv_require(song)<0) return -1;
  if (akau_instrument_ref(instrument)<0) return -1;
  song->instrumentv[song->instrumentc]=instrument;
  return song->instrumentc++;
}

int akau_song_add_ipcm(struct akau_song *song,struct akau_ipcm *ipcm) {
  if (!song||!ipcm) return -1;
  if (song->lock) return -1;
  if (akau_song_ipcmv_require(song)<0) return -1;
  if (akau_ipcm_ref(ipcm)<0) return -1;
  if (akau_ipcm_lock(ipcm)<0) {
    akau_ipcm_del(ipcm);
    return -1;
  }
  song->ipcmv[song->ipcmc]=ipcm;
  return song->ipcmc++;
}

int akau_song_add_external_instrument(struct akau_song *song,int instrumentid) {
  if (!song||(instrumentid<1)) return -1;
  if (song->lock) return -1;
  if (akau_song_instrumentv_require(song)<0) return -1;
  if (akau_song_referencev_require(song)<0) return -1;
  struct akau_song_reference *reference=song->referencev+song->referencec++;
  reference->type=3;
  reference->internal_id=song->instrumentc;
  reference->external_id=instrumentid;
  song->instrumentv[song->instrumentc]=0;
  return song->instrumentc++;
}

int akau_song_add_external_ipcm(struct akau_song *song,int ipcmid) {
  if (!song||(ipcmid<1)) return -1;
  if (song->lock) return -1;
  if (akau_song_ipcmv_require(song)<0) return -1;
  if (akau_song_referencev_require(song)<0) return -1;
  struct akau_song_reference *reference=song->referencev+song->referencec++;
  reference->type=1;
  reference->internal_id=song->ipcmc;
  reference->external_id=ipcmid;
  song->ipcmv[song->ipcmc]=0;
  return song->ipcmc++;
}

/* Add commands.
 */
 
struct akau_song_cmd *akau_song_addcmd(struct akau_song *song,uint8_t op) {
  if (!song) return 0;
  if (song->lock) return 0;
  if (akau_song_cmdv_require(song)<0) return 0;
  struct akau_song_cmd *cmd=song->cmdv+song->cmdc++;
  cmd->op=op;
  return cmd;
}

int akau_song_addcmd_delay(struct akau_song *song,int framec) {
  if (!song) return -1;
  if (framec<1) return 0;

  /* If the last command was also DELAY, add what we can to it. */
  if (song->cmdc>0) {
    struct akau_song_cmd *lastcmd=song->cmdv+song->cmdc-1;
    if (lastcmd->op==AKAU_SONG_OP_DELAY) {
      int addc=0xffff-lastcmd->DELAY.framec;
      if (addc>framec) addc=framec;
      lastcmd->DELAY.framec+=addc;
      framec-=addc;
      if (framec<1) return 0;
    }
  }

  /* Add DELAY in a loop until (framec) exhausted. */
  while (framec>0) {
    struct akau_song_cmd *cmd=akau_song_addcmd(song,AKAU_SONG_OP_DELAY);
    if (!cmd) return -1;
    int addc=framec;
    if (addc>0xffff) addc=0xffff;
    cmd->DELAY.framec=addc;
    framec-=addc;
  }

  return 0;
}

int akau_song_addcmd_sync(struct akau_song *song,uint16_t token) {
  struct akau_song_cmd *cmd=akau_song_addcmd(song,AKAU_SONG_OP_SYNC);
  if (!cmd) return -1;
  cmd->SYNC.token=token;
  return 0;
}

int akau_song_addcmd_note(struct akau_song *song,uint8_t instrid,uint8_t pitch,uint8_t trim,int8_t pan,int32_t framec) {
  struct akau_song_cmd *cmd=akau_song_addcmd(song,AKAU_SONG_OP_NOTE);
  if (!cmd) return -1;
  cmd->NOTE.instrid=instrid;
  cmd->NOTE.pitch=pitch;
  cmd->NOTE.trim=trim;
  cmd->NOTE.pan=pan;
  cmd->NOTE.framec=framec;
  return 0;
}

int akau_song_addcmd_drum(struct akau_song *song,uint8_t ipcmid,uint8_t trim,int8_t pan) {
  struct akau_song_cmd *cmd=akau_song_addcmd(song,AKAU_SONG_OP_DRUM);
  if (!cmd) return -1;
  cmd->DRUM.ipcmid=ipcmid;
  cmd->DRUM.trim=trim;
  cmd->DRUM.pan=pan;
  return 0;
}

/* Link to external resources.
 */

int akau_song_link(struct akau_song *song,struct akau_store *store) {
  if (!song) return -1;
  if (song->lock) return -1;

  /* Link instruments, if they are already present. */
  int i; for (i=0;i<song->instrumentc;i++) {
    struct akau_instrument *instrument=song->instrumentv[i];
    if (!instrument) continue;
    if (akau_instrument_link(instrument,store)<0) return -1;
  }
  
  if (song->referencec<1) return 0; // Don't even null-check store in this case.
  if (!store) return -1;

  while (song->referencec>0) {
    struct akau_song_reference *reference=song->referencev+song->referencec-1;
    if (reference->internal_id<0) return -1;
    if (reference->external_id<1) return -1;
    switch (reference->type) {
    
      case 1: { // ipcm
          if (reference->internal_id>=song->ipcmc) return -1;
          if (song->ipcmv[reference->internal_id]) return -1;
          struct akau_ipcm *ipcm=akau_store_get_ipcm(store,reference->external_id);
          if (!ipcm) {
            akau_error("IPCM %d not found.",reference->external_id);
            return -1;
          }
          if (akau_ipcm_ref(ipcm)<0) return -1;
          song->ipcmv[reference->internal_id]=ipcm;
        } break;
        
      case 3: { // instrument
          if (reference->internal_id>=song->instrumentc) return -1;
          if (song->instrumentv[reference->internal_id]) return -1;
          struct akau_instrument *instrument=akau_store_get_instrument(store,reference->external_id);
          if (!instrument) {
            akau_error("Instrument %d not found.",reference->external_id);
            return -1;
          }
          if (akau_instrument_ref(instrument)<0) return -1;
          song->instrumentv[reference->internal_id]=instrument;
        } break;
        
      default: return -1;
    }
    song->referencec--;
  }
  return 0;
}

/* Update single command.
 * Return >0 to finish update, <0 on error, or 0 to proceed.
 */

static int akau_song_execute_command(struct akau_song *song,const struct akau_song_cmd *cmd,struct akau_mixer *mixer) {
  switch (cmd->op) {

    case AKAU_SONG_OP_NOOP: return 0;

    case AKAU_SONG_OP_DELAY: {
        if (akau_mixer_set_song_delay(mixer,cmd->DELAY.framec)<0) return -1;
      } return 1;

    case AKAU_SONG_OP_SYNC: {
        /* We break separation of concerns here -- song should not assume it can access the global sync list.
         * I think it is OK, weighing efficiency against strict correctness.
         * It's unlikely that a song will be playing in any context other than master output, right?
         */
        if (akau_queue_sync_token(cmd->SYNC.token)<0) return -1;
      } return 0;

    case AKAU_SONG_OP_NOTE: {
        if (cmd->NOTE.instrid>=song->instrumentc) return -1;
        struct akau_instrument *instrument=song->instrumentv[cmd->NOTE.instrid];
        if (akau_mixer_play_note(mixer,instrument,cmd->NOTE.pitch,cmd->NOTE.trim,cmd->NOTE.pan,cmd->NOTE.framec)<0) return -1;
      } return 0;

    case AKAU_SONG_OP_DRUM: {
        if (cmd->DRUM.ipcmid>=song->ipcmc) return -1;
        struct akau_ipcm *ipcm=song->ipcmv[cmd->DRUM.ipcmid];
        if (akau_mixer_play_ipcm(mixer,ipcm,cmd->DRUM.trim,cmd->DRUM.pan,0)<0) return -1;
      } return 0;
      
  }
  return -1;
}

/* Update, main entry point.
 */

int akau_song_update(struct akau_song *song,struct akau_mixer *mixer,int cmdp) {
  if (!song||!mixer) return -1;
  if (song->cmdc<1) return -1;
  if ((cmdp<0)||(cmdp>=song->cmdc)) cmdp=0;

  while (cmdp<song->cmdc) {
    const struct akau_song_cmd *cmd=song->cmdv+cmdp;
    int err=akau_song_execute_command(song,cmd,mixer);
    if (err<0) return -1;
    cmdp++;
    if (err>0) break;
  }

  if (akau_mixer_set_song_position(mixer,cmdp)<0) return -1;
  return 0;
}

/* Decode (convenience)
 */
 
struct akau_song *akau_song_decode(
  const char *src,int srcc,
  void (*cb_error)(const char *msg,int msgc,const char *src,int srcc,int srcp)
) {
  struct akau_song_decoder *decoder=akau_song_decoder_new();
  if (!decoder) return 0;
  struct akau_song *song=akau_song_decoder_decode(decoder,src,srcc,cb_error);
  akau_song_decoder_del(decoder);
  return song;
}
