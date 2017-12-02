/* ps_isong_binary.c
 * Encoder and decoder for the binary format of intermediate song objects.
 *
 *-----------------------------------------------------------------------------
 * FORMAT DEFINITION
 *   0000   8 Signature: "\0PS\xffSONG"
 *   0008   2 Tempo.
 *   000a   1 Drum count.
 *   000b   1 Instrument count.
 *   000c   4 Additional header length.
 *   0010 ... Additional header.
 *   .... ... Drum IPCM IDs, one byte each.
 *   .... ... Instruments:
 *     0000   1 Length.
 *     0001 ... Text.
 *     ....
 *   .... ... Commands.
 *     0000   1 Opcode, determines remainder of format.
 *     .... ... See below.
 *
 * Commands.
 *   00 NOOP
 *   01 BEAT
 *   02 NOTE
 *       1 instrid
 *       1 pitch
 *       1 trim
 *       1 duration
 *       1 pan
 *   03 DRUM
 *       1 drumid
 *       1 trim
 *       1 pan
 *   04 ASSIGN
 *       1 chanid
 *   05 GLISS
 *       1 chanid
 *       1 pitch
 *       1 duration
 *-----------------------------------------------------------------------------
 */

#include "ps.h"
#include "ps_isong.h"
#include "util/ps_buffer.h"

/* Encode header.
 */

static int ps_isong_encode_binary_header(struct ps_buffer *dst,const struct ps_isong *isong) {
  uint8_t tmp[16]={0};
  memcpy(tmp,"\0PS\xffSONG",8);
  tmp[8]=isong->tempo>>8;
  tmp[9]=isong->tempo;
  tmp[10]=isong->drumidc;
  tmp[11]=isong->instrc;
  // 12..15 are additional header length, always zero.
  if (ps_buffer_append(dst,tmp,16)<0) return -1;
  return 0;
}

/* Encode drums and instruments.
 */

static int ps_isong_encode_binary_drums(struct ps_buffer *dst,const struct ps_isong *isong) {
  int i=0; for (;i<isong->drumidc;i++) {
    uint8_t id=isong->drumidv[i];
    if (ps_buffer_append(dst,&id,1)<0) return -1;
  }
  return 0;
}

static int ps_isong_encode_binary_instruments(struct ps_buffer *dst,const struct ps_isong *isong) {
  const struct ps_isong_instrument *instr=isong->instrv;
  int i=isong->instrc; for (;i-->0;instr++) {
    uint8_t len=instr->textc;
    if (ps_buffer_append(dst,&len,1)<0) return -1;
    if (ps_buffer_append(dst,instr->text,instr->textc)<0) return -1;
  }
  return 0;
}

/* Encode commands.
 */

static int ps_isong_encode_binary_command(struct ps_buffer *dst,const struct ps_isong_command *cmd) {
  if (ps_buffer_append(dst,&cmd->op,1)<0) return -1;
  switch (cmd->op) {
    case PS_ISONG_OP_NOOP: return 0;
    case PS_ISONG_OP_BEAT: return 0;
    case PS_ISONG_OP_NOTE: {
        uint8_t tmp[5]={cmd->NOTE.instrid,cmd->NOTE.pitch,cmd->NOTE.trim,cmd->NOTE.duration,cmd->NOTE.pan};
        return ps_buffer_append(dst,tmp,5);
      }
    case PS_ISONG_OP_DRUM: {
        uint8_t tmp[3]={cmd->DRUM.drumid,cmd->DRUM.trim,cmd->DRUM.pan};
        return ps_buffer_append(dst,tmp,3);
      }
    case PS_ISONG_OP_ASSIGN: {
        return ps_buffer_append(dst,&cmd->ASSIGN.chanid,1);
      }
    case PS_ISONG_OP_GLISS: {
        uint8_t tmp[3]={cmd->GLISS.chanid,cmd->GLISS.pitch,cmd->GLISS.duration};
        return ps_buffer_append(dst,tmp,3);
      }
  }
  ps_log(RES,ERROR,"Unexpected isong opcode 0x%02x.",cmd->op);
  return -1;
}

static int ps_isong_encode_binary_commands(struct ps_buffer *dst,const struct ps_isong *isong) {
  const struct ps_isong_command *cmd=isong->cmdv;
  int i=isong->cmdc; for (;i-->0;cmd++) {
    if (ps_isong_encode_binary_command(dst,cmd)<0) return -1;
  }
  return 0;
}

/* Encode to binary, main entry point.
 */

int ps_isong_encode_binary(void *dstpp,const struct ps_isong *isong) {
  if (!dstpp||!isong) return -1;
  struct ps_buffer dst={0};
  if (
    (ps_isong_encode_binary_header(&dst,isong)<0)||
    (ps_isong_encode_binary_drums(&dst,isong)<0)||
    (ps_isong_encode_binary_instruments(&dst,isong)<0)||
    (ps_isong_encode_binary_commands(&dst,isong),0)
  ) {
    ps_buffer_cleanup(&dst);
    return -1;
  }
  *(void**)dstpp=dst.v;
  return dst.c;
}

/* Decode drums.
 */

static int ps_isong_decode_binary_drums(struct ps_isong *isong,const uint8_t *src,int srcc,int drumc) {
  if (srcc<drumc) return -1;
  int i=0; for (;i<drumc;i++) {
    if (ps_isong_add_drum(isong,src[i])<0) return -1;
  }
  return drumc;
}

/* Decode instruments.
 */

static int ps_isong_decode_binary_instruments(struct ps_isong *isong,const uint8_t *src,int srcc,int instrc) {
  int srcp=0,i=0;
  for (;i<instrc;i++) {
    if (srcp>=srcc) return -1;
    int textc=src[srcp++];
    if (srcp>srcc-textc) return -1;
    if (ps_isong_add_instrument(isong,(const char*)src+srcp,textc)<0) return -1;
    srcp+=textc;
  }
  return srcp;
}

/* Decode commands.
 */

static int ps_isong_decode_binary_command(struct ps_isong *isong,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t op=src[0];

  /* BEAT is different from the others. */
  if (op==PS_ISONG_OP_BEAT) {
    if (ps_isong_insert_beats(isong,-1,1)<0) return -1;
    return 1;
  }

  /* Don't bother preserving NOOP. */
  if (op==PS_ISONG_OP_NOOP) {
    return 1;
  }

  /* Everything else works basically the same way. */
  struct ps_isong_command *cmd=ps_isong_insert_command(isong,-1);
  if (!cmd) return -1;
  cmd->op=op;

  /* Read command arguments. */
  switch (op) {
    case PS_ISONG_OP_NOTE: {
        if (srcc<6) return -1;
        cmd->NOTE.instrid=src[1];
        cmd->NOTE.pitch=src[2];
        cmd->NOTE.trim=src[3];
        cmd->NOTE.duration=src[4];
        cmd->NOTE.pan=src[5];
      } return 6;
    case PS_ISONG_OP_DRUM: {
        if (srcc<4) return -1;
        cmd->DRUM.drumid=src[1];
        cmd->DRUM.trim=src[2];
        cmd->DRUM.pan=src[3];
      } return 4;
    case PS_ISONG_OP_ASSIGN: {
        if (srcc<2) return -1;
        cmd->ASSIGN.chanid=src[1];
      } return 2;
    case PS_ISONG_OP_GLISS: {
        if (srcc<4) return -1;
        cmd->GLISS.chanid=src[1];
        cmd->GLISS.pitch=src[2];
        cmd->GLISS.duration=src[3];
      } return 4;
  }
  return -1;
}

static int ps_isong_decode_binary_commands(struct ps_isong *isong,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    int err=ps_isong_decode_binary_command(isong,src+srcp,srcc-srcp);
    if (err<0) return -1;
    srcp+=err;
  }
  return 0;
}

/* Decode from binary, main entry point.
 */

int ps_isong_decode_binary(struct ps_isong *isong,const uint8_t *src,int srcc) {
  if (!isong||!src) return -1;

  /* Decode and validate header. */
  if (srcc<16) return -1;
  if (memcmp(src,"\0PS\xffSONG",8)) return -1;
  int tempo=(src[8]<<8)|src[9];
  int drumc=src[10];
  int instrc=src[11];
  int addlc=(src[12]<<24)|(src[13]<<16)|(src[14]<<8)|src[15];
  if (tempo<1) return -1;
  if (addlc<0) return -1;
  int srcp=16+addlc;
  if ((srcp<16)||(srcp>srcc)) return -1;
  if (ps_isong_clear(isong)<0) return -1;

  /* Decode drums, instruments, and commands. */
  int err;
  if ((err=ps_isong_decode_binary_drums(isong,src+srcp,srcc-srcp,drumc))<0) return err; srcp+=err;
  if ((err=ps_isong_decode_binary_instruments(isong,src+srcp,srcc-srcp,instrc))<0) return err; srcp+=err;
  if (ps_isong_decode_binary_commands(isong,src+srcp,srcc-srcp)<0) return -1;
  
  return 0;
}
