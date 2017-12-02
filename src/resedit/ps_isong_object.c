#include "ps.h"
#include "ps_isong.h"

/* New.
 */

struct ps_isong *ps_isong_new() {
  struct ps_isong *isong=calloc(1,sizeof(struct ps_isong));
  if (!isong) return 0;

  isong->refc=1;
  isong->tempo=100;

  return isong;
}

/* Delete.
 */

static void ps_isong_instrument_cleanup(struct ps_isong_instrument *instr) {
  if (!instr) return;
  if (instr->text) free(instr->text);
  memset(instr,0,sizeof(struct ps_isong_instrument));
}
 
void ps_isong_del(struct ps_isong *isong) {
  if (!isong) return;
  if (isong->refc-->1) return;

  if (isong->drumidv) free(isong->drumidv);
  if (isong->cmdv) free(isong->cmdv);

  if (isong->instrv) {
    while (isong->instrc-->0) ps_isong_instrument_cleanup(isong->instrv+isong->instrc);
    free(isong->instrv);
  }

  free(isong);
}

/* Retain.
 */
 
int ps_isong_ref(struct ps_isong *isong) {
  if (!isong) return -1;
  if (isong->refc<1) return -1;
  if (isong->refc==INT_MAX) return -1;
  isong->refc++;
  return 0;
}

/* Clear.
 */

int ps_isong_clear(struct ps_isong *isong) {
  if (!isong) return -1;
  isong->tempo=100;
  isong->drumidc=0;
  isong->cmdc=0;
  while (isong->instrc>0) {
    isong->instrc--;
    ps_isong_instrument_cleanup(isong->instrv+isong->instrc);
  }
  return 0;
}

/* Add drum.
 */

int ps_isong_add_drum(struct ps_isong *isong,int ipcmid) {
  if (!isong) return -1;
  if (ipcmid<1) return -1;

  if (isong->drumidc>=isong->drumida) {
    int na=isong->drumida+8;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(isong->drumidv,sizeof(int)*na);
    if (!nv) return -1;
    isong->drumidv=nv;
    isong->drumida=na;
  }

  isong->drumidv[isong->drumida++]=ipcmid;

  return 0;
}

/* Remove drum.
 */
 
int ps_isong_remove_drum(struct ps_isong *isong,int drumid) {
  if (!isong) return -1;
  if ((drumid<0)||(drumid>=isong->drumidc)) return -1;
  
  isong->drumidc--;
  memmove(isong->drumidv+drumid,isong->drumidv+drumid+1,sizeof(int)*(isong->drumidc-drumid));

  /* Cascade. */
  struct ps_isong_command *cmd=isong->cmdv;
  int i=0; while (i<isong->cmdc) {
    if (cmd->op==PS_ISONG_OP_DRUM) {
      if (cmd->DRUM.drumid==drumid) {
        isong->cmdc--;
        memmove(cmd,cmd+1,sizeof(struct ps_isong_command)*(isong->cmdc-i));
        continue;
      } else if (cmd->DRUM.drumid>drumid) {
        cmd->DRUM.drumid--;
      }
    }
    i++;
    cmd++;
  }
  
  return 0;
}

/* Add instrument.
 */

int ps_isong_add_instrument(struct ps_isong *isong,const char *text,int textc) {
  if (!isong) return -1;
  if (!text) textc=0; else if (textc<0) { textc=0; while (text[textc]) textc++; }
  if (ps_isong_validate_instrument(text,textc)<0) return -1;

  if (isong->instrc>=isong->instra) {
    int na=isong->instra+8;
    if (na>INT_MAX/sizeof(struct ps_isong_instrument)) return -1;
    void *nv=realloc(isong->instrv,sizeof(struct ps_isong_instrument)*na);
    if (!nv) return -1;
    isong->instrv=nv;
    isong->instra=na;
  }

  struct ps_isong_instrument *instr=isong->instrv+isong->instrc++;
  memset(instr,0,sizeof(struct ps_isong_instrument));
  if (!(instr->text=malloc(textc+1))) {
    isong->instrc--;
    return -1;
  }
  memcpy(instr->text,text,textc);
  instr->text[textc]=0;
  instr->textc=textc;

  return 0;
}

/* Remove instrument.
 */
 
int ps_isong_remove_instrument(struct ps_isong *isong,int instrid) {
  if (!isong) return -1;
  if ((instrid<0)||(instrid>=isong->instrc)) return -1;

  struct ps_isong_instrument *instr=isong->instrv+instrid;
  ps_isong_instrument_cleanup(instr);
  isong->instrc--;
  memmove(instr,instr+1,sizeof(struct ps_isong_instrument)*(isong->instrc-instrid));

  /* Cascade. */
  struct ps_isong_command *cmd=isong->cmdv;
  int i=0; while (i<isong->cmdc) {
    if (cmd->op==PS_ISONG_OP_NOTE) {
      if (cmd->NOTE.instrid==instrid) {
        isong->cmdc--;
        memmove(cmd,cmd+1,sizeof(struct ps_isong_command)*(isong->cmdc-i));
        continue;
      } else if (cmd->NOTE.instrid>instrid) {
        cmd->NOTE.instrid--;
      }
    }
    i++;
    cmd++;
  }

  return 0;
}

/* Validate instrument.
 * ( TIME LEVEL TIME LEVEL TIME ) COEF...
 */

static int ps_isong_expect_int(const char *src,int srcc,int limit) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return -1;
  int n=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    if (n>INT_MAX/10) return -1;
    n*=INT_MAX;
    if (n>INT_MAX-digit) return -1;
    n+=digit;
  }
  if (n>limit) return -1;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) return -1;
  return srcp;
}

int ps_isong_validate_instrument(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>255) return -1;
  int srcp=0,err;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp++]!='(')) return -1;
  if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,0xffff))<0) return -1; srcp+=err;
  if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,0xff))<0) return -1; srcp+=err;
  if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,0xffff))<0) return -1; srcp+=err;
  if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,0xff))<0) return -1; srcp+=err;
  if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,0xffff))<0) return -1; srcp+=err;
  if ((srcp>=srcc)||(src[srcp++]!=')')) return -1;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if ((err=ps_isong_expect_int(src+srcp,srcc-srcp,999))<0) return -1; srcp+=err;
  }
  return 0;
}

/* Count beats.
 */

int ps_isong_count_beats(const struct ps_isong *isong) {
  if (!isong) return -1;
  const struct ps_isong_command *cmd=isong->cmdv;
  int c=0,i=isong->cmdc; for (;i-->0;cmd++) {
    if (cmd->op==PS_ISONG_OP_BEAT) c++;
  }
  return c;
}

/* Find beat by index.
 */
 
int ps_isong_find_beat(const struct ps_isong *isong,int beatp) {
  if (!isong) return -1;
  if (beatp<0) return -1;
  int lo=0,hi=isong->cmdc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int ck0=ck;
    while ((ck>lo)&&(isong->cmdv[ck].op!=PS_ISONG_OP_BEAT)) ck--;
    if ((ck<lo)||(isong->cmdv[ck].op!=PS_ISONG_OP_BEAT)) return -1;
         if (beatp<isong->cmdv[ck].BEAT.index) hi=ck;
    else if (beatp>isong->cmdv[ck].BEAT.index) {
      ck=ck0+1;
      while ((ck<hi)&&(isong->cmdv[ck].op!=PS_ISONG_OP_BEAT)) ck++;
      if ((ck>=hi)||(isong->cmdv[ck].op!=PS_ISONG_OP_BEAT)) return -1;
      lo=ck;
    } else return ck;
  }
  return -1;
}

/* Find beat for a given command position.
 */
 
int ps_isong_beat_from_cmd(const struct ps_isong *isong,int cmdp) {
  if (!isong) return -1;
  if ((cmdp<0)||(cmdp>=isong->cmdc)) return -1;
  while ((cmdp>0)&&(isong->cmdv[cmdp].op!=PS_ISONG_OP_BEAT)) cmdp--;
  if ((cmdp<0)||(isong->cmdv[cmdp].op!=PS_ISONG_OP_BEAT)) return -1;
  return isong->cmdv[cmdp].BEAT.index;
}

/* Find command position of beat for a given command position.
 */
 
int ps_isong_beat_cmdp_from_cmd(const struct ps_isong *isong,int cmdp) {
  if (!isong) return -1;
  if ((cmdp<0)||(cmdp>=isong->cmdc)) return -1;
  while ((cmdp>0)&&(isong->cmdv[cmdp].op!=PS_ISONG_OP_BEAT)) cmdp--;
  if ((cmdp<0)||(isong->cmdv[cmdp].op!=PS_ISONG_OP_BEAT)) return -1;
  return cmdp;
}

/* Read up to next BEAT command.
 */

int ps_isong_measure_beat(const struct ps_isong *isong,int cmdp) {
  if (!isong) return -1;
  if ((cmdp<0)||(cmdp>=isong->cmdc)) return -1;
  while ((cmdp<isong->cmdc)&&(isong->cmdv[cmdp].op!=PS_ISONG_OP_BEAT)) cmdp++;
  return cmdp;
}

/* Remove commands.
 */

int ps_isong_remove_commands(struct ps_isong *isong,int cmdp,int cmdc) {
  if (!isong) return -1;
  if ((cmdp<0)||(cmdc<0)||(cmdp>isong->cmdc-cmdc)) return -1;
  int rmbeatc=0,i=cmdp+cmdc;
  while (i-->cmdp) {
    if (isong->cmdv[i].op==PS_ISONG_OP_BEAT) rmbeatc++;
  }
  isong->cmdc-=cmdc;
  memmove(isong->cmdv+cmdp,isong->cmdv+cmdp+cmdc,sizeof(struct ps_isong_command)*(isong->cmdc-cmdp));
  if (rmbeatc) {
    for (i=cmdp;i<isong->cmdc;i++) {
      if (isong->cmdv[i].op==PS_ISONG_OP_BEAT) {
        isong->cmdv[i].BEAT.index-=rmbeatc;
      }
    }
  }
  return 0;
}

/* Insert command.
 */

struct ps_isong_command *ps_isong_insert_command(struct ps_isong *isong,int cmdp) {
  if (!isong) return 0;
  if ((cmdp<0)||(cmdp>isong->cmdc)) cmdp=isong->cmdc;
  if (!cmdp) return 0; // First command must be BEAT, and don't insert beats with this function.

  if (isong->cmdc>=isong->cmda) {
    int na=isong->cmda+32;
    if (na>INT_MAX/sizeof(struct ps_isong_command)) return 0;
    void *nv=realloc(isong->cmdv,sizeof(struct ps_isong_command)*na);
    if (!nv) return 0;
    isong->cmdv=nv;
    isong->cmda=na;
  }

  struct ps_isong_command *cmd=isong->cmdv+cmdp;
  memmove(cmd+1,cmd,sizeof(struct ps_isong_command)*(isong->cmdc-cmdp));
  isong->cmdc++;
  memset(cmd,0,sizeof(struct ps_isong_command));

  return cmd;
}

/* Insert beats.
 */

int ps_isong_insert_beats(struct ps_isong *isong,int cmdp,int beatc) {
  if (!isong) return -1;
  if ((cmdp<0)||(cmdp>=isong->cmdc)) cmdp=isong->cmdc;
  if (beatc<1) return 0;

  if (isong->cmdc>isong->cmda-beatc) {
    int na=(isong->cmdc+beatc+32)&~32;
    if (na>INT_MAX/sizeof(struct ps_isong_command)) return -1;
    void *nv=realloc(isong->cmdv,sizeof(struct ps_isong_command)*na);
    if (!nv) return -1;
    isong->cmdv=nv;
    isong->cmda=na;
  }

  int beatid=ps_isong_beat_from_cmd(isong,cmdp);
  if (beatid<0) return -1;
  beatid++;

  struct ps_isong_command *cmdv=isong->cmdv+cmdp;
  memmove(cmdv+beatc,cmdv,sizeof(struct ps_isong_command)*(isong->cmdc-cmdp));
  isong->cmdc+=beatc;
  int i=beatc; for (;i-->0;cmdv++) {
    cmdv->op=PS_ISONG_OP_BEAT;
    cmdv->BEAT.index=beatid++;
  }

  for (i=isong->cmdc-beatc-cmdp;i-->0;cmdv++) {
    if (cmdv->op==PS_ISONG_OP_BEAT) {
      cmdv->BEAT.index+=beatc;
    }
  }

  return 0;
}

/* Split beats.
 */
 
int ps_isong_split_beats(struct ps_isong *isong,int multiplier) {
  if (!isong) return -1;
  if (multiplier<1) return -1;
  if (multiplier==1) return 0;

  /* Survey NOTE and GLISS commands and abort if any duration would overflow. */
  struct ps_isong_command *cmd=isong->cmdv;
  int i=isong->cmdc;
  for (;i-->0;cmd++) {
    switch (cmd->op) {
      case PS_ISONG_OP_NOTE: if (cmd->NOTE.duration>255/multiplier) return -1; break;
      case PS_ISONG_OP_GLISS: if (cmd->GLISS.duration>255/multiplier) return -1; break;
    }
  }

  /* Calculate new size and reallocate cmdv. */
  int beatc=ps_isong_count_beats(isong);
  if (beatc<1) return beatc;
  if (beatc>INT_MAX/multiplier) return -1;
  int ncmdc=isong->cmdc+(beatc*(multiplier-1));
  if (ncmdc>isong->cmda) {
    int na=ncmdc;
    if (na<INT_MAX-32) na=(na+32)&~31;
    void *nv=realloc(isong->cmdv,sizeof(struct ps_isong_command)*na);
    if (!nv) return -1;
    isong->cmdv=nv;
    isong->cmda=na;
  }

  /* Add BEAT commands at the end of each existing beat. */
  int addc=multiplier-1;
  for (i=1;i<isong->cmdc;i++) {
    if (isong->cmdv[i].op==PS_ISONG_OP_BEAT) {
      if (ps_isong_insert_beats(isong,i,addc)<0) return -1;
      i+=addc;
    }
  }
  if (ps_isong_insert_beats(isong,-1,addc)<0) return -1;

  /* Multiply duration of all NOTE and GLISS commands. */
  for (cmd=isong->cmdv,i=isong->cmdc;i-->0;cmd++) {
    switch (cmd->op) {
      case PS_ISONG_OP_NOTE: cmd->NOTE.duration*=multiplier; break;
      case PS_ISONG_OP_GLISS: cmd->GLISS.duration*=multiplier; break;
    }
  }

  return 0;
}

/* Merge beats.
 */
 
int ps_isong_merge_beats(struct ps_isong *isong,int factor,int force) {
  if (!isong) return -1;
  if (factor<1) return -1;
  if (factor==1) return 0;

  /* Consider whether the action will be destructive. If we're using the force, don't bother. */
  if (!force) {
    int index=0;
    const struct ps_isong_command *cmd=isong->cmdv;
    int i=isong->cmdc; for (;i-->0;cmd++) {
      switch (cmd->op) {
        case PS_ISONG_OP_BEAT: {
            index=cmd->BEAT.index;
          } break;
        case PS_ISONG_OP_NOTE: {
            if (cmd->NOTE.duration%factor) return -1;
          } goto _require_alignment_;
        case PS_ISONG_OP_GLISS: {
            if (cmd->GLISS.duration%factor) return -1;
          } goto _require_alignment_;
        case PS_ISONG_OP_DRUM: 
        _require_alignment_: {
            if (index%factor) return -1;
          } break;
      }
    }
  }

  /* Remove BEAT where (index%factor), and divide NOTE and GLISS durations by factor. */
  struct ps_isong_command *cmd=isong->cmdv;
  int i=0; while (i<isong->cmdc) {
    switch (cmd->op) {
      case PS_ISONG_OP_BEAT: {
          if (cmd->BEAT.index%factor) {
            isong->cmdc--;
            memmove(cmd,cmd+1,sizeof(struct ps_isong_command)*(isong->cmdc-i));
            continue;
          } else {
            cmd->BEAT.index/=factor;
          }
        } break;
      case PS_ISONG_OP_NOTE: {
          if (!(cmd->NOTE.duration/=factor)) cmd->NOTE.duration=1;
        } break;
      case PS_ISONG_OP_GLISS: {
          if (!(cmd->GLISS.duration/=factor)) cmd->GLISS.duration=1;
        } break;
    }
    i++;
    cmd++;
  }

  return 0;
}

/* Decode: detect format and dispatch.
 */

int ps_isong_decode_binary(struct ps_isong *isong,const uint8_t *src,int srcc);
int ps_isong_decode_assembly(struct ps_isong *isong,const char *src,int srcc);
int ps_isong_decode_text(struct ps_isong *isong,const char *src,int srcc);

static int ps_isong_is_text(const unsigned char *src,int srcc) {
  for (;srcc-->0;src++) {
    if (*src==0x09) continue;
    if (*src==0x0a) continue;
    if (*src==0x0d) continue;
    if (*src<0x20) return 0;
    if (*src>0x7f) return 0;
  }
  return 1;
}

int ps_isong_decode(struct ps_isong *isong,const void *src,int srcc) {
  if (!isong) return -1;
  if (!src) return -1;
  if (srcc<0) return -1;
  if (ps_isong_clear(isong)<0) return -1;

  if ((srcc>=8)&&!memcmp(src,"\0PS\xffSONG",8)) {
    return ps_isong_decode_binary(isong,src,srcc);
  }

  if (ps_isong_is_text(src,srcc)) {
    if ((srcc>=4)&&!memcmp(src,"#asm",4)) {
      return ps_isong_decode_assembly(isong,src,srcc);
    }
    return ps_isong_decode_text(isong,src,srcc);
  }

  ps_log(RES,ERROR,"Unable to detect song format.");
  return -1;
}
