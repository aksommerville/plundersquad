#include "akau_song_internal.h"

/* Private buffer type.
 */

struct akau_song_output {
  uint8_t *v;
  int c,a;
};

static void akau_song_output_cleanup(struct akau_song_output *output) {
  if (output->v) free(output->v);
}

static int akau_song_output_require(struct akau_song_output *output,int addc) {
  if (addc<1) return 0;
  if (output->c>INT_MAX-addc) return -1;
  int na=output->c+addc;
  if (na<=output->a) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(output->v,na);
  if (!nv) return -1;
  output->v=nv;
  output->a=na;
  return 0;
}

static int akau_song_output_append(struct akau_song_output *output,const void *src,int srcc) {
  if (srcc<1) return 0;
  if (akau_song_output_require(output,srcc)<0) return -1;
  memcpy(output->v+output->c,src,srcc);
  output->c+=srcc;
  return 0;
}

/* Encode header.
 */

static int akau_song_encode_header(struct akau_song_output *output,const struct akau_song *song) {
  uint8_t tmp[16]={0};
  memcpy(tmp,"\0AK\xffSONG",8);
  tmp[8]=song->tempo>>8;
  tmp[9]=song->tempo;
  tmp[10]=song->drumc;
  tmp[11]=song->instrc;
  if (akau_song_output_append(output,tmp,16)<0) return -1;
  return 0;
}

/* Encode drums.
 */

static int akau_song_encode_drums(struct akau_song_output *output,const struct akau_song *song) {
  const struct akau_song_drum *drum=song->drumv;
  int i=song->drumc; for (;i-->0;drum++) {
    uint8_t tmp[2]={drum->ipcmid>>8,drum->ipcmid};
    if (akau_song_output_append(output,tmp,2)<0) return -1;
  }
  return 0;
}

/* Encode instruments.
 */

static int akau_song_encode_instruments(struct akau_song_output *output,const struct akau_song *song) {
  const struct akau_song_instrument *instr=song->instrv;
  int i=song->instrc; for (;i-->0;instr++) {
    uint8_t tmp=instr->serialc;
    if (akau_song_output_append(output,&tmp,1)<0) return -1;
    if (akau_song_output_append(output,instr->serial,tmp)<0) return -1;
  }
  return 0;
}

/* Encode commands.
 */

static int akau_song_encode_command(uint8_t *tmp,const union akau_song_command *cmd) {
  switch (cmd->op) {
    case AKAU_SONG_OP_NOOP: return 0;
    case AKAU_SONG_OP_BEAT: {
        tmp[0]=AKAU_SONG_OP_BEAT;
      } return 1;
    case AKAU_SONG_OP_DRUM: {
        tmp[0]=AKAU_SONG_OP_DRUM;
        tmp[1]=cmd->DRUM.ref;
        tmp[2]=cmd->DRUM.drumid;
        tmp[3]=cmd->DRUM.trim;
        tmp[4]=cmd->DRUM.pan;
      } return 5;
    case AKAU_SONG_OP_NOTE: {
        tmp[0]=AKAU_SONG_OP_NOTE;
        tmp[1]=cmd->NOTE.ref;
        tmp[2]=cmd->NOTE.instrid;
        tmp[3]=cmd->NOTE.trim;
        tmp[4]=cmd->NOTE.pan;
        tmp[5]=cmd->NOTE.pitch;
        tmp[6]=cmd->NOTE.duration;
      } return 7;
    case AKAU_SONG_OP_ADJPITCH: 
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN: {
        tmp[0]=cmd->op;
        tmp[1]=cmd->ADJ.ref;
        tmp[2]=cmd->ADJ.v;
        tmp[3]=cmd->ADJ.duration;
      } return 4;
  }
  return -1;
}

static int akau_song_encode_commands(struct akau_song_output *output,const struct akau_song *song) {
  const union akau_song_command *cmd=song->cmdv;
  int i=song->cmdc; for (;i-->0;cmd++) {
    if (akau_song_output_require(output,16)<0) return -1; // Longest is actually 7 bytes.
    int err=akau_song_encode_command(output->v+output->c,cmd);
    if (err<0) return -1;
    output->c+=err;
  }
  return 0;
}

/* Encode.
 */
 
int akau_song_encode(void *dstpp,const struct akau_song *song) {
  if (!dstpp||!song) return -1;
  struct akau_song_output output={0};
  if (
    (akau_song_encode_header(&output,song)<0)||
    (akau_song_encode_drums(&output,song)<0)||
    (akau_song_encode_instruments(&output,song)<0)||
    (akau_song_encode_commands(&output,song)<0)
  ) {
    akau_song_output_cleanup(&output);
    return -1;
  }
  *(void**)dstpp=output.v;
  return output.c;
}

/* Decode drums.
 */

static int akau_song_decode_drums(struct akau_song *song,int drumc,const uint8_t *src,int srcc) {
  int srcp=0;
  while (drumc-->0) {
    if (srcp>srcc-2) return -1;
    int ipcmid=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (akau_song_add_drum(song,ipcmid)<0) return -1;
  }
  return srcp;
}

/* Decode instruments.
 */

static int akau_song_decode_instruments(struct akau_song *song,int instrc,const uint8_t *src,int srcc) {
  int srcp=0;
  while (instrc-->0) {
    if (srcp>=srcc) return -1;
    int serialc=src[srcp++];
    if (srcp>srcc-serialc) return -1;
    if (akau_song_add_instrument(song,src+srcp,serialc)<0) return -1;
    srcp+=serialc;
  }
  return srcp;
}

/* Decode commands.
 */

static int akau_song_decode_command(struct akau_song *song,const uint8_t *src,int srcc) {
  union akau_song_command cmd={0};
  int srcp=1;
  cmd.op=src[0];
  switch (cmd.op) {
    case AKAU_SONG_OP_NOOP: return 1; // Don't store it.
    case AKAU_SONG_OP_BEAT: break;
    case AKAU_SONG_OP_DRUM: {
        if (srcc<5) return -1;
        cmd.DRUM.ref=src[1];
        cmd.DRUM.drumid=src[2];
        cmd.DRUM.trim=src[3];
        cmd.DRUM.pan=src[4];
        srcp=5;
      } break;
    case AKAU_SONG_OP_NOTE: {
        if (srcc<7) return -1;
        cmd.NOTE.ref=src[1];
        cmd.NOTE.instrid=src[2];
        cmd.NOTE.trim=src[3];
        cmd.NOTE.pan=src[4];
        cmd.NOTE.pitch=src[5];
        cmd.NOTE.duration=src[6];
        srcp=7;
      } break;
    case AKAU_SONG_OP_ADJPITCH:
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN: {
        if (srcc<4) return -1;
        cmd.ADJ.ref=src[1];
        cmd.ADJ.v=src[2];
        cmd.ADJ.duration=src[3];
        srcp=4;
      } break;
    default: return -1;
  }
  if (akau_song_insert_command(song,-1,&cmd)<0) return -1;
  return srcp;
}

static int akau_song_decode_commands(struct akau_song *song,const uint8_t *src,int srcc) {
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=akau_song_decode_command(song,src+srcp,srcc-srcp))<0) return -1;
    srcp+=err;
  }
  return srcp;
}

/* Decode.
 */
 
int akau_song_decode(struct akau_song *song,const void *src,int srcc) {
  if (!song) return -1;
  if (!src||(srcc<1)) return -1;
  if (akau_song_clear(song)<0) return -1;
  const uint8_t *SRC=src;
  int err;

  if (srcc<16) return -1;
  if (memcmp(SRC,"\0AK\xffSONG",8)) return -1;
  int tempo=(SRC[8]<<8)|SRC[9];
  int drumc=SRC[10];
  int instrc=SRC[11];
  int extra=(SRC[12]<<24)|(SRC[13]<<16)|(SRC[14]<<8)|SRC[15];
  if (akau_song_set_tempo(song,tempo)<0) return -1;
  if (extra<0) return -1;
  if (extra>0x01000000) return -1; // Arbitrary sanity limit.
  int srcp=16+extra;
  
  if ((err=akau_song_decode_drums(song,drumc,SRC+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_song_decode_instruments(song,instrc,SRC+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_song_decode_commands(song,SRC+srcp,srcc-srcp))<0) return -1; srcp+=err;
  
  if (srcp<srcc) return -1;
  return 0;
}
