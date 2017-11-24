#include "akau_wavegen_internal.h"

/* New decoder.
 */
 
struct akau_wavegen_decoder *akau_wavegen_decoder_new() {
  struct akau_wavegen_decoder *decoder=calloc(1,sizeof(struct akau_wavegen_decoder));
  if (!decoder) return 0;

  return decoder;
}

/* Delete.
 */
 
void akau_wavegen_decoder_del(struct akau_wavegen_decoder *decoder) {
  if (!decoder) return;

  if (decoder->chanv) {
    while (decoder->chanc-->0) {
      struct akau_wavegen_chan *chan=decoder->chanv+decoder->chanc;
      akau_fpcm_del(chan->fpcm);
    }
    free(decoder->chanv);
  }

  if (decoder->cmdv) free(decoder->cmdv);

  free(decoder);
}

/* Add channel.
 */

int akau_wavegen_decoder_add_channel(struct akau_wavegen_decoder *decoder,struct akau_fpcm *fpcm) {
  if (!decoder||!fpcm) return -1;

  if (decoder->chanc>=decoder->chana) {
    int na=decoder->chana+8;
    if (na>INT_MAX/sizeof(struct akau_wavegen_chan)) return -1;
    void *nv=realloc(decoder->chanv,sizeof(struct akau_wavegen_chan)*na);
    if (!nv) return -1;
    decoder->chanv=nv;
    decoder->chana=na;
  }

  if (akau_fpcm_ref(fpcm)<0) return -1;

  struct akau_wavegen_chan *chan=decoder->chanv+decoder->chanc++;
  memset(chan,0,sizeof(struct akau_wavegen_chan));

  chan->fpcm=fpcm;

  return 0;
}

/* Search command insertion point by frame.
 */

static int akau_wavegen_decoder_cmdv_search(const struct akau_wavegen_decoder *decoder,int framep) {
  if (framep<0) return -1;
  if (decoder->cmdc<1) return 0;
  if (framep>=decoder->cmdv[decoder->cmdc-1].framep) return decoder->cmdc;
  int lo=0,hi=decoder->cmdc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (framep<decoder->cmdv[ck].framep) hi=ck;
    else if (framep>decoder->cmdv[ck].framep) lo=ck+1;
    else return ck;
  }
  return lo;
}

/* Add command.
 */

int akau_wavegen_decoder_add_command(struct akau_wavegen_decoder *decoder,int framep,int chanid,int k,double v) {
  if (!decoder) return -1;

  int cmdp=akau_wavegen_decoder_cmdv_search(decoder,framep);
  if (cmdp<0) return -1;

  if (decoder->cmdc>=decoder->cmda) {
    int na=decoder->cmda+32;
    if (na>INT_MAX/sizeof(struct akau_wavegen_cmd)) return -1;
    void *nv=realloc(decoder->cmdv,sizeof(struct akau_wavegen_cmd)*na);
    if (!nv) return -1;
    decoder->cmdv=nv;
    decoder->cmda=na;
  }

  struct akau_wavegen_cmd *cmd=decoder->cmdv+cmdp;
  memmove(cmd+1,cmd,sizeof(struct akau_wavegen_cmd)*(decoder->cmdc-cmdp));
  decoder->cmdc++;
  memset(cmd,0,sizeof(struct akau_wavegen_cmd));

  cmd->framep=framep;
  cmd->chanid=chanid;
  cmd->k=k;
  cmd->v=v;

  return 0;
}

/* Get length.
 */
 
int akau_wavegen_decoder_get_length(const struct akau_wavegen_decoder *decoder) {
  if (!decoder) return 0;
  if (decoder->cmdc<1) return 0;
  return decoder->cmdv[decoder->cmdc-1].framep;
}
