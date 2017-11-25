#include "ps.h"
#include "ps_iwg.h"
#include "os/ps_fs.h"
#include "util/ps_buffer.h"
#include "util/ps_text.h"
#include "akau/akau_wavegen.h"
#include "akau/akau_pcm.h"

/* Object lifecycle.
 */
 
void ps_iwg_channel_cleanup(struct ps_iwg_channel *chan) {
  if (!chan) return;
  if (chan->arg) free(chan->arg);
  memset(chan,0,sizeof(struct ps_iwg_channel));
}
 
struct ps_iwg *ps_iwg_new() {
  struct ps_iwg *iwg=calloc(1,sizeof(struct ps_iwg));
  if (!iwg) return 0;

  iwg->refc=1;

  return iwg;
}

void ps_iwg_del(struct ps_iwg *iwg) {
  if (!iwg) return;
  if (iwg->refc-->1) return;

  if (iwg->chanv) {
    while (iwg->chanc-->0) ps_iwg_channel_cleanup(iwg->chanv+iwg->chanc);
    free(iwg->chanv);
  }

  if (iwg->cmdv) {
    free(iwg->cmdv);
  }

  free(iwg);
}

int ps_iwg_ref(struct ps_iwg *iwg) {
  if (!iwg) return -1;
  if (iwg->refc<1) return -1;
  if (iwg->refc==INT_MAX) return -1;
  iwg->refc++;
  return 0;
}

/* Clear.
 */

int ps_iwg_clear(struct ps_iwg *iwg) {
  if (!iwg) return -1;
  while (iwg->chanc>0) {
    iwg->chanc--;
    ps_iwg_channel_cleanup(iwg->chanv+iwg->chanc);
  }
  iwg->cmdc=0;
  return 0;
}

/* Validate harmonics text.
 * We call it valid if it contains only space and digits.
 * Empty is valid.
 * We should check that no number exceeds 999, but we don't.
 */

static int ps_iwg_valid_harmonics(const char *src,int srcc) {
  int srcp=0;
  for (;srcp<srcc;srcp++) {
    if ((unsigned char)src[srcp]<=0x20) continue;
    if ((src[srcp]>='0')&&(src[srcp]<='9')) continue;
    return 0;
  }
  return 1;
}

/* Add channel.
 */
 
int ps_iwg_add_channel(struct ps_iwg *iwg,const char *arg,int argc) {
  if (!iwg) return -1;
  if (!arg) argc=0; else if (argc<0) { argc=0; while (arg[argc]) argc++; }
  while (argc&&((unsigned char)arg[argc-1]<=0x20)) argc--;
  while (argc&&((unsigned char)arg[0]<=0x20)) { arg++; argc--; }

       if ((argc==4)&&!memcmp(arg,"sine",4)) ;
  else if ((argc==6)&&!memcmp(arg,"square",6)) ;
  else if ((argc==3)&&!memcmp(arg,"saw",3)) ;
  else if ((argc==5)&&!memcmp(arg,"noise",5)) ;
  else if (ps_iwg_valid_harmonics(arg,argc)) ;
  else return -1;

  if (iwg->chanc>=iwg->chana) {
    int na=iwg->chana+8;
    if (na>INT_MAX/sizeof(struct ps_iwg_channel)) return -1;
    void *nv=realloc(iwg->chanv,sizeof(struct ps_iwg_channel)*na);
    if (!nv) return -1;
    iwg->chanv=nv;
    iwg->chana=na;
  }

  struct ps_iwg_channel *chan=iwg->chanv+iwg->chanc++;
  memset(chan,0,sizeof(struct ps_iwg_channel));
  if (argc) {
    if (!(chan->arg=malloc(argc+1))) {
      iwg->chanc--;
      return -1;
    }
    memcpy(chan->arg,arg,argc);
    chan->arg[argc]=0;
  }
  chan->argc=argc;

  return 0;
}

/* Add command.
 */
 
int ps_iwg_add_command(struct ps_iwg *iwg,int time_ms,int chanid,int k,double v) {

  if (!iwg) return -1;
  if (time_ms<0) return -1;
  if ((chanid<0)||(chanid>=iwg->chanc)) return -1;
  switch (k) {
    case AKAU_WAVEGEN_K_STEP: break;
    case AKAU_WAVEGEN_K_TRIM: break;
    case AKAU_WAVEGEN_K_NOOP: break;
    default: return -1;
  }
  if (v<0.0) return -1; // No field can accept a negative value.

  int cmdp,cmdc;
  cmdc=ps_iwg_search_command_time(&cmdp,iwg,time_ms);
  cmdp+=cmdc;

  if (iwg->cmdc>=iwg->cmda) {
    int na=iwg->cmda+32;
    if (na>INT_MAX/sizeof(struct ps_iwg_command)) return -1;
    void *nv=realloc(iwg->cmdv,sizeof(struct ps_iwg_command)*na);
    if (!nv) return -1;
    iwg->cmdv=nv;
    iwg->cmda=na;
  }

  struct ps_iwg_command *cmd=iwg->cmdv+cmdp;
  memmove(cmd+1,cmd,sizeof(struct ps_iwg_command)*(iwg->cmdc-cmdp));
  iwg->cmdc++;
  cmd->time=time_ms;
  cmd->chanid=chanid;
  cmd->k=k;
  cmd->v=v;

  return 0;
}

/* Remove channel, cascading.
 */
 
int ps_iwg_remove_channel(struct ps_iwg *iwg,int chanid) {
  if (!iwg) return -1;
  if ((chanid<0)||(chanid>=iwg->chanc)) return -1;

  iwg->chanc--;
  struct ps_iwg_channel *chan=iwg->chanv+chanid;
  ps_iwg_channel_cleanup(chan);
  memmove(chan,chan+1,sizeof(struct ps_iwg_channel)*(iwg->chanc-chanid));

  int i=0; while (i<iwg->cmdc) {
    int rmc=0;
    while ((i+rmc<iwg->cmdc)&&(iwg->cmdv[i+rmc].chanid==chanid)) rmc++;
    if (rmc) {
      if (ps_iwg_remove_commands(iwg,i,rmc)<0) return -1;
    } else if (iwg->cmdv[i].chanid>chanid) {
      iwg->cmdv[i].chanid--;
      i++;
    } else {
      i++;
    }
  }

  return 0;
}

/* Remove a range of comamnds.
 */

int ps_iwg_remove_commands(struct ps_iwg *iwg,int p,int c) {
  if (!iwg) return -1;
  if ((p<0)||(c<0)||(p>iwg->cmdc-c)) return -1;
  iwg->cmdc-=c;
  memmove(iwg->cmdv+p,iwg->cmdv+p+c,sizeof(struct ps_iwg_command)*(iwg->cmdc-p));
  return 0;
}

/* Adjust time of a command.
 */
 
int ps_iwg_adjust_command_time(struct ps_iwg *iwg,int p,int new_time_ms) {
  if (!iwg) return -1;
  if ((p<0)||(p>=iwg->cmdc)) return -1;
  if (new_time_ms<0) return -1;

  iwg->cmdv[p].time=new_time_ms;
  
  while ((p>0)&&(new_time_ms<iwg->cmdv[p-1].time)) {
    struct ps_iwg_command tmp=iwg->cmdv[p];
    iwg->cmdv[p]=iwg->cmdv[p-1];
    iwg->cmdv[p-1]=tmp;
    p--;
  }

  while ((p<iwg->cmdc-1)&&(new_time_ms>iwg->cmdv[p+1].time)) {
    struct ps_iwg_command tmp=iwg->cmdv[p];
    iwg->cmdv[p]=iwg->cmdv[p+1];
    iwg->cmdv[p+1]=tmp;
    p++;
  }

  return 0;
}

/* Search commands by time.
 */
 
int ps_iwg_search_command_time(int *cmdp,const struct ps_iwg *iwg,int time_ms) {
  if (!iwg) return 0;
  int lo=0,hi=iwg->cmdc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (time_ms<iwg->cmdv[ck].time) hi=ck;
    else if (time_ms>iwg->cmdv[ck].time) lo=ck+1;
    else {
      int c=1;
      while ((ck>lo)&&(iwg->cmdv[ck-1].time==time_ms)) { ck--; c++; }
      while ((ck+c<hi)&&(iwg->cmdv[ck+c].time==time_ms)) c++;
      if (cmdp) *cmdp=ck;
      return c;
    }
  }
  if (cmdp) *cmdp=lo;
  return 0;
}

/* Search for the previous or next command for a given field.
 */

int ps_iwg_search_command_field(const struct ps_iwg *iwg,int p,int chanid,int k) {
  if (!iwg) return -1;
  if (p<0) p=0;
  for (;p<iwg->cmdc;p++) {
    if (iwg->cmdv[p].chanid!=chanid) continue;
    if (iwg->cmdv[p].k!=k) continue;
    return p;
  }
  return -1;
}

int ps_iwg_search_command_field_reverse(const struct ps_iwg *iwg,int p,int chanid,int k) {
  if (!iwg) return -1;
  if (p>=iwg->cmdc) p=iwg->cmdc-1;
  for (;p>=0;p--) {
    if (iwg->cmdv[p].chanid!=chanid) continue;
    if (iwg->cmdv[p].k!=k) continue;
    return p;
  }
  return -1;
}

/* Represent key.
 */

const char *ps_iwg_k_repr(int k) {
  switch (k) {
    case AKAU_WAVEGEN_K_STEP: return "step";
    case AKAU_WAVEGEN_K_TRIM: return "trim";
    case AKAU_WAVEGEN_K_NOOP: return "noop";
  }
  return 0;
}

/* Encode channels.
 */

static int ps_iwg_encode_channels(struct ps_buffer *dst,const struct ps_iwg *iwg) {
  const struct ps_iwg_channel *chan=iwg->chanv;
  int i=iwg->chanc; for (;i-->0;chan++) {
    if (ps_buffer_append(dst,"channel ",8)<0) return -1;
    if (ps_buffer_append(dst,chan->arg,chan->argc)<0) return -1;
    if (ps_buffer_append(dst,"\n",1)<0) return -1;
  }
  return 0;
}

/* Encode commands.
 */

static int ps_iwg_encode_commands(struct ps_buffer *dst,const struct ps_iwg *iwg) {
  const struct ps_iwg_command *cmd=iwg->cmdv;
  int i=iwg->cmdc; for (;i-->0;cmd++) {
    if (ps_buffer_appendf(dst,"%d %d %s %f\n",cmd->time,cmd->chanid,ps_iwg_k_repr(cmd->k),cmd->v)<0) return -1;
  }
  return 0;
}

/* Encode, main entry point.
 */
 
int ps_iwg_encode(void *dstpp,const struct ps_iwg *iwg) {
  if (!dstpp||!iwg) return -1;
  struct ps_buffer dst={0};

  if (ps_iwg_encode_channels(&dst,iwg)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }
  if (ps_iwg_encode_commands(&dst,iwg)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }
  
  *(void**)dstpp=dst.v;
  return dst.c;
}

/* Decode one line.
 */

int ps_iwg_decode_line(struct ps_iwg *iwg,const char *src,int srcc) {
  if (!iwg) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0; // Blank line, done.

  const char *token;
  int tokenc;
  #define NEXTTOKEN \
    token=src+srcp; tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; } \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  NEXTTOKEN
  if ((tokenc==7)&&!memcmp(token,"channel",7)) {
    if (ps_iwg_add_channel(iwg,src+srcp,srcc-srcp)<0) {
      ps_log(RES,ERROR,"Invalid channel initializer: %.*s",srcc-srcp,src+srcp);
      return -1;
    }
    return 0;
  }
  int time;
  if (ps_int_eval_interactive(&time,token,tokenc,0,INT_MAX,"time")<0) return -1;

  NEXTTOKEN
  int chid;
  if (ps_int_eval_interactive(&chid,token,tokenc,0,iwg->chanc-1,"channel id")<0) return -1;

  NEXTTOKEN
  int k;
       if ((tokenc==4)&&!memcmp(token,"step",4)) k=AKAU_WAVEGEN_K_STEP;
  else if ((tokenc==4)&&!memcmp(token,"trim",4)) k=AKAU_WAVEGEN_K_TRIM;
  else if ((tokenc==4)&&!memcmp(token,"noop",4)) k=AKAU_WAVEGEN_K_NOOP;
  else {
    ps_log(RES,ERROR,"Unexpected wavegen key '%.*s' (step,trim,noop)",tokenc,token);
    return -1;
  }

  NEXTTOKEN
  double v;
  if (ps_double_eval(&v,token,tokenc)<0) {
    ps_log(RES,ERROR,"Failed to evaluate '%.*s' as a float.",tokenc,token);
    return -1;
  }

  if (ps_iwg_add_command(iwg,time,chid,k,v)<0) {
    ps_log(RES,ERROR,"Failed to add command: %d %d %s %f",time,chid,ps_iwg_k_repr(k),v);
    return -1;
  }

  return 0;
}

/* Decode, main entry point.
 */
 
int ps_iwg_decode(struct ps_iwg *iwg,const char *src,int srcc) {
  if (!iwg||(srcc<0)||(srcc&&!src)) return -1;
  if (ps_iwg_clear(iwg)<0) return -1;
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)>0) {
    if (ps_iwg_decode_line(iwg,reader.line,reader.linec)<0) {
      ps_log(RES,ERROR,"%d: <-- at this line",reader.lineno);
      return -1;
    }
  }
  return 0;
}

/* Encode and decode conveniences.
 */
 
struct akau_wavegen_decoder *ps_iwg_to_wavegen_decoder(const struct ps_iwg *iwg) {
  void *serial=0;
  int serialc=ps_iwg_encode(&serial,iwg);
  if ((serialc<0)||!serial) return 0;

  struct akau_wavegen_decoder *decoder=akau_wavegen_decoder_new();
  if (!decoder) {
    free(serial);
    return 0;
  }

  if (akau_wavegen_decoder_decode(decoder,serial,serialc)<0) {
    free(serial);
    akau_wavegen_decoder_del(decoder);
    return 0;
  }

  free(serial);
  return decoder;
}

struct akau_ipcm *ps_iwg_to_ipcm(const struct ps_iwg *iwg) {
  void *serial=0;
  int serialc=ps_iwg_encode(&serial,iwg);
  if ((serialc<0)||!serial) return 0;
  struct akau_ipcm *ipcm=akau_wavegen_decode(serial,serialc);
  free(serial);
  return ipcm;
}

int ps_iwg_write_file(const char *path,const struct ps_iwg *iwg) {
  void *serial=0;
  int serialc=ps_iwg_encode(&serial,iwg);
  if ((serialc<0)||!serial) return -1;

  if (ps_file_write(path,serial,serialc)<0) {
    free(serial);
    return -1;
  }

  free(serial);
  return 0;
}

int ps_iwg_read_file(struct ps_iwg *iwg,const char *path) {
  void *serial=0;
  int serialc=ps_file_read(&serial,path);
  if ((serialc<0)||!serial) {
    ps_log(RES,ERROR,"%s: Failed to read file",path);
    return -1;
  }

  if (ps_iwg_decode(iwg,serial,serialc)<0) {
    ps_log(RES,ERROR,"%s: <-- in this file",path);
    free(serial);
    return -1;
  }

  free(serial);
  return 0;
}
