#include "akau_wavegen_internal.h"

/* Get the next command, starting at (decoder->cmdp), matching this channel and key.
 */

static const struct akau_wavegen_cmd *akau_wavegen_get_next_command(const struct akau_wavegen_decoder *decoder,const struct akau_wavegen_cmd *first) {
  const struct akau_wavegen_cmd *cmd=decoder->cmdv+decoder->cmdp;
  int i=decoder->cmdc-decoder->cmdp; for (;i-->0;cmd++) {
    if (cmd->chanid!=first->chanid) continue;
    if (cmd->k!=first->k) continue;
    return cmd;
  }
  return 0;
}

/* Apply single command.
 */

static int akau_wavegen_decoder_apply_command(struct akau_wavegen_decoder *decoder,const struct akau_wavegen_cmd *cmd) {
  if ((cmd->chanid<0)||(cmd->chanid>=decoder->chanc)) return -1;
  struct akau_wavegen_chan *chan=decoder->chanv+cmd->chanid;
  const struct akau_wavegen_cmd *next=akau_wavegen_get_next_command(decoder,cmd);
  switch (cmd->k) {
    case AKAU_WAVEGEN_K_NOOP: break;
    
    case AKAU_WAVEGEN_K_STEP: {
        if (next) {
          chan->stepa=cmd->v;
          chan->stepz=next->v;
          chan->stepap=cmd->framep;
          chan->stepzp=next->framep;
          chan->step=cmd->v;
        } else {
          chan->step=cmd->v;
          chan->stepap=chan->stepzp=0;
        }
      } break;

    case AKAU_WAVEGEN_K_TRIM: {
        if (next) {
          chan->trima=cmd->v;
          chan->trimz=next->v;
          chan->trimap=cmd->framep;
          chan->trimzp=next->framep;
          chan->trim=cmd->v;
        } else {
          chan->trim=cmd->v;
          chan->trimap=chan->trimzp=0;
        }
      } break;

    default: return -1;
  }
  return 0;
}

/* Update envelopes.
 * Read from decoder->cmdv based on decoder->framep.
 */

static int akau_wavegen_decoder_update_envelopes(struct akau_wavegen_decoder *decoder) {
  while ((decoder->cmdp<decoder->cmdc)&&(decoder->framep>=decoder->cmdv[decoder->cmdp].framep)) {
    const struct akau_wavegen_cmd *cmd=decoder->cmdv+decoder->cmdp++;
    if (akau_wavegen_decoder_apply_command(decoder,cmd)<0) return -1;
  }
  return 0;
}

/* Generate single sample from wavegen decoder.
 * Update channel positions and mix.
 */

static int akau_wavegen_decoder_print_1(int16_t *dst,struct akau_wavegen_decoder *decoder) {
  double sample=0.0;
  struct akau_wavegen_chan *chan=decoder->chanv;
  int i=decoder->chanc; for (;i-->0;chan++) {

    int samplep=(int)chan->samplep;
    if (samplep>chan->fpcm->c) {
      samplep-=chan->fpcm->c;
      chan->samplep-=chan->fpcm->c;
    }
    sample+=chan->fpcm->v[samplep]*chan->trim;
    chan->samplep+=chan->step;

    if (chan->stepap<chan->stepzp) {
      if (decoder->framep>=chan->stepzp) {
        chan->step=chan->stepz;
        chan->stepap=chan->stepzp=0;
      } else {
        double t=(double)(decoder->framep-chan->stepap)/(double)(chan->stepzp-chan->stepap);
        chan->step=((1.0-t)*chan->stepa)+(t*chan->stepz);
      }
    }

    if (chan->trimap<chan->trimzp) {
      if (decoder->framep>=chan->trimzp) {
        chan->trim=chan->trimz;
        chan->trimap=chan->trimzp=0;
      } else {
        double t=(double)(decoder->framep-chan->trimap)/(double)(chan->trimzp-chan->trimap);
        chan->trim=((1.0-t)*chan->trima)+(t*chan->trimz);
      }
    }
  
  }
  sample*=32767.0;
  if (sample>=32767.0) *dst=32767;
  else if (sample<=-32768.0) *dst=-32768;
  else *dst=(int)sample;
  return 0;
}

/* Print wavegen decoder, main entry point.
 */
 
struct akau_ipcm *akau_wavegen_decoder_print(struct akau_wavegen_decoder *decoder) {
  if (!decoder) return 0;

  int samplec=akau_wavegen_decoder_get_length(decoder);
  if (samplec<1) return 0;
  struct akau_ipcm *ipcm=akau_ipcm_new(samplec);
  if (!ipcm) return 0;

  decoder->framep=0;
  decoder->cmdp=0;
  struct akau_wavegen_chan *chan=decoder->chanv;
  int i=decoder->chanc; for (;i-->0;chan++) {
    chan->samplep=0.0;
    chan->trim=0.0;
    chan->trimap=chan->trimzp=0;
    chan->step=1.0;
    chan->stepap=chan->stepzp=0;
  }

  int16_t *samplev=akau_ipcm_get_sample_buffer(ipcm);
  if (!samplev) {
    akau_ipcm_del(ipcm);
    return 0;
  }
  while (decoder->framep<samplec) {
    if (akau_wavegen_decoder_update_envelopes(decoder)<0) {
      akau_ipcm_del(ipcm);
      return 0;
    }
    if (akau_wavegen_decoder_print_1(samplev,decoder)<0) {
      akau_ipcm_del(ipcm);
      return 0;
    }
    samplev++;
    decoder->framep++;
  }
  
  return ipcm;
}
