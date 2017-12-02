/* ps_isong_assembly.c
 * Encoder and decoder for the intermediate song object ps_isong.
 * The "assembly" format is an analogue of the preferred "binary" format.
 * It's basically human-readable, at least for debugging purposes.
 * It is not feasible to write music in this format, even by my standards.
 * 
 *-----------------------------------------------------------------------------
 * FORMAT DEFINITION
 *
 * Files are plain line-oriented text. Hash begins a line comment.
 * For format detection, files should begin "#asm". That's not required by this decoder.
 * Each non-empty line is a command or header field.
 * Tokens on a line are delimited by whitespace.
 *
 * Order matters:
 *  - Drums and instruments are assigned IDs starting at zero, in the order defined.
 *  - Commands are played back in the order of the file.
 *  - Tempo must be set before the first command.
 *  - Each drum and instrument must be defined before its first use, not necessarily before the first command.
 *  - ...But it is still good practice to put all the definitions before all commands.
 *
 * Syntax:
 *   tempo BPM
 *     - BPM in 1..65535.
 *     - Defaults to 100 if unset.
 *   drum IPCMID
 *     - IPCMID in 1..255 (because the binary format stores in a single byte).
 *   instrument ( ATTACKTIME ATTACKLEVEL DRAWBACKTIME STABLELEVEL DECAYTIME ) COEFFICIENTS...
 *     - Levels in 0..255.
 *     - Times in milliseconds.
 *     - Coefficients in 0..999.
 *   b
 *     - Begin beat.
 *   n INSTRID PITCH TRIM PAN DURATION
 *     - Tuned note.
 *     - PITCH in 0..255.
 *     - TRIM in 0..255.
 *     - PAN in -128..127.
 *     - DURATION in 1..255.
 *   d DRUMID TRIM PAN
 *     - Verbatim sound effect.
 *     - TRIM in 0..255.
 *     - PAN in -128..127.
 *   a CHANID
 *     - Assign the most recent 'n' command to a live channel for future reference.
 *     - CHANID in 0..31.
 *   g CHANID PITCH DURATION
 *     - Begin glissando.
 *     - CHANID in 0..31, must be previously defined by 'a'.
 *     - PITCH in 0..255.
 *     - DURATION in 1..255.
 *
 *-----------------------------------------------------------------------------
 */

#include "ps.h"
#include "ps_isong.h"
#include "util/ps_buffer.h"
#include "util/ps_text.h"

/* Encode header.
 */

static int ps_isong_encode_header(struct ps_buffer *dst,const struct ps_isong *isong) {
  if (ps_buffer_append(dst,"#asm\n",5)<0) return -1;
  if (ps_buffer_appendf(dst,"tempo %d\n",isong->tempo)<0) return -1;
  return 0;
}

/* Encode drum and instrument definitions.
 */

static int ps_isong_encode_definitions(struct ps_buffer *dst,const struct ps_isong *isong) {
  int i;
  for (i=0;i<isong->drumidc;i++) {
    if (ps_buffer_appendf(dst,"drum %d\n",isong->drumidv[i])<0) return -1;
  }
  const struct ps_isong_instrument *instr=isong->instrv;
  for (i=isong->instrc;i-->0;instr++) {
    if (ps_buffer_appendf(dst,"instrument %.*s\n",instr->textc,instr->text)<0) return -1;
  }
  return 0;
}

/* Encode commands.
 */

static int ps_isong_encode_command(struct ps_buffer *dst,const struct ps_isong *isong,const struct ps_isong_command *cmd) {
  switch (cmd->op) {
    case PS_ISONG_OP_NOOP: return 0; // Safe to skip NOOP.
    case PS_ISONG_OP_BEAT: return ps_buffer_append(dst,"b\n",2);
    case PS_ISONG_OP_NOTE: return ps_buffer_appendf(dst,"n %d %d %d %d %d\n",cmd->NOTE.instrid,cmd->NOTE.pitch,cmd->NOTE.trim,cmd->NOTE.pan,cmd->NOTE.duration);
    case PS_ISONG_OP_DRUM: return ps_buffer_appendf(dst,"d %d %d %d\n",cmd->DRUM.drumid,cmd->DRUM.trim,cmd->DRUM.pan);
    case PS_ISONG_OP_ASSIGN: return ps_buffer_appendf(dst,"a %d\n",cmd->ASSIGN.chanid);
    case PS_ISONG_OP_GLISS: return ps_buffer_appendf(dst,"g %d %d %d\n",cmd->GLISS.chanid,cmd->GLISS.pitch,cmd->GLISS.duration);
    default: {
        ps_log(RES,ERROR,"Unexpected isong cmd op %d.",cmd->op);
        return -1;
      }
  }
}

static int ps_isong_encode_commands(struct ps_buffer *dst,const struct ps_isong *isong) {
  const struct ps_isong_command *cmd=isong->cmdv;
  int i=isong->cmdc; for (;i-->0;cmd++) {
    if (ps_isong_encode_command(dst,isong,cmd)<0) return -1;
  }
  return 0;
}

/* Encode to assembly, main entry point.
 */

int ps_isong_encode_assembly(void *dstpp,const struct ps_isong *isong) {
  if (!dstpp||!isong) return -1;
  struct ps_buffer dst={0};

  if (
    (ps_isong_encode_header(&dst,isong)<0)||
    (ps_isong_encode_definitions(&dst,isong)<0)||
    (ps_isong_encode_commands(&dst,isong)<0)
  ) {
    ps_buffer_cleanup(&dst);
    return -1;
  }
  
  *(void**)dstpp=dst.v;
  return dst.c;
}

/* Decode header lines.
 */

static int ps_isong_decode_assembly_tempo(struct ps_isong *isong,const char *src,int srcc,int lineno) {
  if (isong->cmdc) {
    // It actually wouldn't matter, but I'm calling this a requirement anyway.
    ps_log(RES,ERROR,"%d: 'tempo' not permitted after the first command.",lineno);
    return -1;
  }
  int v;
  if (ps_int_eval(&v,src,srcc)<0) {
    ps_log(RES,ERROR,"%d: Failed to evaluate '%.*s' as integer for tempo.",lineno,srcc,src);
    return -1;
  }
  if ((v<1)||(v>0xffff)) {
    ps_log(RES,ERROR,"%d: Invalid tempo %d, limit 1..65535.",lineno,v);
    return -1;
  }
  isong->tempo=v;
  return 0;
}

static int ps_isong_decode_assembly_drum(struct ps_isong *isong,const char *src,int srcc,int lineno) {
  if (isong->drumidc>=255) {
    ps_log(RES,ERROR,"%d: Too many drum definitions, limit 255.",lineno);
    return -1;
  }
  int v;
  if (ps_int_eval(&v,src,srcc)<0) {
    ps_log(RES,ERROR,"%d: Failed to evaluate '%.*s' as integer for drum ipcm id.",lineno,srcc,src);
    return -1;
  }
  if ((v<1)||(v>0xff)) {
    ps_log(RES,ERROR,"%d: Invalid drum ipcm id %d, limit 1..255.",lineno,v);
    return -1;
  }
  if (ps_isong_add_drum(isong,v)<0) return -1;
  return 0;
}

static int ps_isong_decode_assembly_instrument(struct ps_isong *isong,const char *src,int srcc,int lineno) {

  if (isong->instrc>=255) {
    ps_log(RES,ERROR,"%d: Too many instrument definitions, limit 255.",lineno);
    return -1;
  }

  // ps_isong_add_instrument() validates the text aggressively.
  // Assume that failures here are due to bad formatting.
  if (ps_isong_add_instrument(isong,src,srcc)<0) {
    ps_log(RES,ERROR,"%d: Malformed instrument line. Expected: ( MS LEVEL MS LEVEL MS ) COEF...",lineno);
    ps_log(RES,ERROR,"%d:   MS in 0..65535",lineno);
    ps_log(RES,ERROR,"%d:   LEVEL in 0..255",lineno);
    ps_log(RES,ERROR,"%d:   COEF in 0..999",lineno);
    return -1;
  }
  return 0;
}

/* Command decode helpers.
 */

static int ps_isong_decode_assembly_int_args(int *dst,int dsta,const char *src,int srcc,int lineno) {
  int srcp=0,dstc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (dstc>=dsta) {
      ps_log(RES,ERROR,"%d: Extra tokens after command. Expected %d arguments.",lineno,dsta);
      return -1;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
    if (ps_int_eval(dst+dstc,token,tokenc)<0) {
      ps_log(RES,ERROR,"%d: Failed to decode '%.*s' as integer.",lineno,tokenc,token);
      return -1;
    }
    dstc++;
  }
  if (dstc<dsta) {
    ps_log(RES,ERROR,"%d: Missing command arguments. Expected %d, have %d.",lineno,dsta,dstc);
    return -1;
  }
  return 0;
}

static int ps_isong_validate_assembly_arg(int v,int lo,int hi,const char *name,int lineno) {
  if ((v<lo)||(v>hi)) {
    ps_log(RES,ERROR,"%d: Value for '%s' out of range (%d, limit %d..%d).",lineno,name,v,lo,hi);
    return -1;
  }
  return 0;
}

/* Decode command lines.
 */

static int ps_isong_decode_assembly_b(struct ps_isong *isong,const char *src,int srcc,int lineno) {
  if (srcc) {
    ps_log(RES,ERROR,"%d: Unexpected tokens after 'b'.",lineno);
    return -1;
  }
  if (ps_isong_insert_beats(isong,-1,1)<0) return -1;
  return 0;
}

static int ps_isong_decode_assembly_n(struct ps_isong *isong,const char *src,int srcc,int lineno) {

  int argv[5];
  if (ps_isong_decode_assembly_int_args(argv,5,src,srcc,lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[0],0,isong->instrc-1,"instrument",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[1],0,255,"pitch",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[2],0,255,"trim",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[3],-128,127,"pan",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[4],1,255,"duration",lineno)<0) return -1;
  
  struct ps_isong_command *cmd=ps_isong_insert_command(isong,-1);
  if (!cmd) return -1;
  cmd->op=PS_ISONG_OP_NOTE;
  cmd->NOTE.instrid=argv[0];
  cmd->NOTE.pitch=argv[1];
  cmd->NOTE.trim=argv[2];
  cmd->NOTE.pan=argv[3];
  cmd->NOTE.duration=argv[4];
  return 0;
}

static int ps_isong_decode_assembly_d(struct ps_isong *isong,const char *src,int srcc,int lineno) {

  int argv[3];
  if (ps_isong_decode_assembly_int_args(argv,3,src,srcc,lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[0],0,isong->drumidc-1,"drum",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[1],0,255,"trim",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[2],-128,127,"pan",lineno)<0) return -1;
  
  struct ps_isong_command *cmd=ps_isong_insert_command(isong,-1);
  if (!cmd) return -1;
  cmd->op=PS_ISONG_OP_DRUM;
  cmd->DRUM.drumid=argv[0];
  cmd->DRUM.trim=argv[1];
  cmd->DRUM.pan=argv[2];
  return 0;
}

static int ps_isong_decode_assembly_a(struct ps_isong *isong,const char *src,int srcc,int lineno) {

  int argv[1];
  if (ps_isong_decode_assembly_int_args(argv,1,src,srcc,lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[0],0,31,"channel",lineno)<0) return -1;
  
  struct ps_isong_command *cmd=ps_isong_insert_command(isong,-1);
  if (!cmd) return -1;
  cmd->op=PS_ISONG_OP_ASSIGN;
  cmd->ASSIGN.chanid=argv[0];
  return 0;
}

static int ps_isong_decode_assembly_g(struct ps_isong *isong,const char *src,int srcc,int lineno) {

  int argv[3];
  if (ps_isong_decode_assembly_int_args(argv,3,src,srcc,lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[0],0,31,"channel",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[1],0,255,"pitch",lineno)<0) return -1;
  if (ps_isong_validate_assembly_arg(argv[2],1,255,"duration",lineno)<0) return -1;
  
  struct ps_isong_command *cmd=ps_isong_insert_command(isong,-1);
  if (!cmd) return -1;
  cmd->op=PS_ISONG_OP_GLISS;
  cmd->GLISS.chanid=argv[0];
  cmd->GLISS.pitch=argv[1];
  cmd->GLISS.duration=argv[2];
  return 0;
}

/* Decode single line of assembly.
 */

static int ps_isong_decode_assembly_line(struct ps_isong *isong,const char *src,int srcc,int lineno) {
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0; // Blank line, ignore it.
  const char *kw=src+srcp;
  int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  if (kwc==1) switch (kw[0]) {
    case 'b': return ps_isong_decode_assembly_b(isong,src+srcp,srcc-srcp,lineno);
    case 'n': return ps_isong_decode_assembly_n(isong,src+srcp,srcc-srcp,lineno);
    case 'd': return ps_isong_decode_assembly_d(isong,src+srcp,srcc-srcp,lineno);
    case 'a': return ps_isong_decode_assembly_a(isong,src+srcp,srcc-srcp,lineno);
    case 'g': return ps_isong_decode_assembly_g(isong,src+srcp,srcc-srcp,lineno);
  }
  if ((kwc==5)&&!memcmp(kw,"tempo",5)) return ps_isong_decode_assembly_tempo(isong,src+srcp,srcc-srcp,lineno);
  if ((kwc==4)&&!memcmp(kw,"drum",4)) return ps_isong_decode_assembly_drum(isong,src+srcp,srcc-srcp,lineno);
  if ((kwc==10)&&!memcmp(kw,"instrument",10)) return ps_isong_decode_assembly_instrument(isong,src+srcp,srcc-srcp,lineno);

  ps_log(RES,ERROR,"%d: Unexpected command '%.*s' in isong.",lineno,kwc,kw);
  return -1;
}

/* Decode from assembly, main entry point.
 */

int ps_isong_decode_assembly(struct ps_isong *isong,const char *src,int srcc) {
  if (!isong) return -1;
  if (ps_isong_clear(isong)<0) return -1;
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)>0) {
    if (ps_isong_decode_assembly_line(isong,reader.line,reader.linec,reader.lineno)<0) return -1;
  }
  return 0;
}
