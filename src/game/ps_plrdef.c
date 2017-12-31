#include "ps.h"
#include "ps_plrdef.h"
#include "util/ps_text.h"
#include "util/ps_enums.h"
#include "util/ps_buffer.h"

/* Object lifecycle.
 */
 
struct ps_plrdef *ps_plrdef_new() {
  struct ps_plrdef *plrdef=calloc(1,sizeof(struct ps_plrdef));
  if (!plrdef) return 0;
  return plrdef;
}

void ps_plrdef_del(struct ps_plrdef *plrdef) {
  if (!plrdef) return;
  if (plrdef->palettev) free(plrdef->palettev);
  free(plrdef);
}

/* Grow palette list.
 */

static int ps_plrdef_palette_require(struct ps_plrdef *plrdef) {
  if (plrdef->palettec<plrdef->palettea) return 0;
  int na=plrdef->palettea+8;
  if (na>INT_MAX/sizeof(struct ps_plrdef_palette)) return -1;
  void *nv=realloc(plrdef->palettev,sizeof(struct ps_plrdef_palette)*na);
  if (!nv) return -1;
  plrdef->palettev=nv;
  plrdef->palettea=na;
  return 0;
}

/* Add palette.
 */
 
int ps_plrdef_add_palette(struct ps_plrdef *plrdef,uint32_t rgba_head,uint32_t rgba_body) {
  if (!plrdef) return -1;
  if (ps_plrdef_palette_require(plrdef)<0) return -1;
  struct ps_plrdef_palette *palette=plrdef->palettev+plrdef->palettec++;
  palette->rgba_head=rgba_head;
  palette->rgba_body=rgba_body;
  return 0;
}

/* Restore defaults.
 */

static void ps_plrdef_reset(struct ps_plrdef *plrdef) {
  plrdef->tileid_head=0;
  plrdef->tileid_body=0;
  plrdef->skills=0;
  plrdef->palettec=0;
}

/* Simple property decoders.
 */

static int ps_plrdef_decode_head(struct ps_plrdef *plrdef,const char *src,int srcc,int lineno) {
  int n;
  if (ps_int_eval_interactive(&n,src,srcc,0,255,"head")<0) return -1;
  plrdef->tileid_head=n;
  return 0;
}

static int ps_plrdef_decode_body(struct ps_plrdef *plrdef,const char *src,int srcc,int lineno) {
  int n;
  if (ps_int_eval_interactive(&n,src,srcc,0,255,"body")<0) return -1;
  plrdef->tileid_body=n;
  return 0;
}

static int ps_plrdef_decode_skill(struct ps_plrdef *plrdef,const char *src,int srcc,int lineno) {
  int skill=ps_skill_eval(src,srcc);
  if (skill<1) {
    ps_log(RES,ERROR,"%d: Unknown skill '%.*s'",lineno,srcc,src);
    return -1;
  }
  plrdef->skills|=skill;
  return 0;
}

static int ps_plrdef_decode_color(uint32_t *dst,const char *src,int srcc,int lineno) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return -1;

  *dst=0;
  int digitc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
    int digit=ps_hexdigit_eval(src[srcp]);
    if (digit<0) {
      ps_log(RES,ERROR,"%d: Unexpected digit '%c' in hexadecimal integer.",lineno,src[srcp]);
      return -1;
    }
    *dst<<=4;
    *dst|=digit;
    digitc++;
    srcp++;
  }

  if (digitc==3) {
    *dst=(((*dst&0xf00)<<20)|((*dst&0x0f0)<<16)|((*dst&0x00f)<<8));
    *dst|=*dst>>4;
    *dst|=0xff;
  } else if (digitc==6) {
    *dst<<=8;
    *dst|=0xff;
  } else if (digitc==8) {
  } else {
    ps_log(RES,ERROR,"%d: Color must be 3, 6, or 8 hexadecimal digits.",lineno);
    return -1;
  }
  
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

static int ps_plrdef_decode_colors(struct ps_plrdef *plrdef,const char *src,int srcc,int lineno) {

  uint32_t head,body;
  int srcp=0,err;
  if ((err=ps_plrdef_decode_color(&head,src+srcp,srcc-srcp,lineno))<1) return -1;
  srcp+=err;
  if ((err=ps_plrdef_decode_color(&body,src+srcp,srcc-srcp,lineno))<1) return -1;
  srcp+=err;
  if (srcp<srcc) {
    ps_log(RES,ERROR,"%d: Unexpected tokens after colors.",lineno);
    return -1;
  }

  if (ps_plrdef_palette_require(plrdef)<0) return -1;
  struct ps_plrdef_palette *palette=plrdef->palettev+plrdef->palettec++;
  palette->rgba_head=head;
  palette->rgba_body=body;
  
  return 0;
}

/* Decode single line.
 */

static int ps_plrdef_decode_line(struct ps_plrdef *plrdef,const char *src,int srcc,int lineno) {

  int kwc=0;
  while ((kwc<srcc)&&((unsigned char)src[kwc]>0x20)) kwc++;
  int srcp=kwc;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *arg=src+srcp;
  int argc=srcc-srcp;

  if ((kwc==4)&&!memcmp(src,"head",4)) return ps_plrdef_decode_head(plrdef,arg,argc,lineno);
  if ((kwc==4)&&!memcmp(src,"body",4)) return ps_plrdef_decode_body(plrdef,arg,argc,lineno);
  if ((kwc==5)&&!memcmp(src,"skill",5)) return ps_plrdef_decode_skill(plrdef,arg,argc,lineno);
  if ((kwc==6)&&!memcmp(src,"colors",6)) return ps_plrdef_decode_colors(plrdef,arg,argc,lineno);
  if ((kwc==18)&&!memcmp(src,"head_on_top_always",18)) { plrdef->head_on_top_always=1; return 0; }

  ps_log(RES,ERROR,"%d: Unexpected keyword '%.*s' (head,body,skill,colors)",lineno,kwc,src);
  return -1;
}

/* Decode.
 */

int ps_plrdef_decode(struct ps_plrdef *plrdef,const void *src,int srcc) {
  if (!plrdef) return -1;
  ps_plrdef_reset(plrdef);
  
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)) {
    if (!reader.linec) continue;
    if (ps_plrdef_decode_line(plrdef,reader.line,reader.linec,reader.lineno)<0) return -1;
  }

  if (plrdef->palettec<1) {
    ps_log(RES,ERROR,"plrdef must define at least one color palette");
    return -1;
  }
  
  return 0;
}

/* Encode.
 */

static int ps_plrdef_encode_to_buffer(struct ps_buffer *buffer,const struct ps_plrdef *plrdef) {
  if (ps_buffer_appendf(buffer,"head 0x%02x\n",plrdef->tileid_head)<0) return -1;
  if (ps_buffer_appendf(buffer,"body 0x%02x\n",plrdef->tileid_body)<0) return -1;
  if (plrdef->head_on_top_always) {
    if (ps_buffer_append(buffer,"head_on_top_always\n",-1)<0) return -1;
  }
  uint16_t mask=1;
  int i; for (i=0;i<16;i++,mask<<=1) {
    if (plrdef->skills&mask) {
      if (ps_buffer_appendf(buffer,"skill %s\n",ps_skill_repr(mask))<0) return -1;
    }
  }
  const struct ps_plrdef_palette *palette=plrdef->palettev;
  for (i=0;i<plrdef->palettec;i++,palette++) {
    if (ps_buffer_appendf(buffer,"colors %08x %08x\n",palette->rgba_head,palette->rgba_body)<0) return -1;
  }
  return 0;
}
 
int ps_plrdef_encode(void *dstpp,const struct ps_plrdef *plrdef) {
  if (!dstpp||!plrdef) return -1;
  struct ps_buffer buffer={0};
  if (ps_plrdef_encode_to_buffer(&buffer,plrdef)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  *(void**)dstpp=buffer.v;
  return buffer.c;
}
