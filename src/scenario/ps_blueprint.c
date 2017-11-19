#include "ps.h"
#include "ps_blueprint.h"
#include "util/ps_text.h"
#include "util/ps_enums.h"

/* New.
 */
 
struct ps_blueprint *ps_blueprint_new() {
  struct ps_blueprint *blueprint=calloc(1,sizeof(struct ps_blueprint));
  if (!blueprint) return 0;

  blueprint->refc=1;

  return blueprint;
}

/* Delete.
 */
 
void ps_blueprint_del(struct ps_blueprint *blueprint) {
  if (!blueprint) return;
  if (blueprint->refc-->1) return;

  if (blueprint->poiv) free(blueprint->poiv);
  if (blueprint->solutionv) free(blueprint->solutionv);

  free(blueprint);
}

/* Retain.
 */
 
int ps_blueprint_ref(struct ps_blueprint *blueprint) {
  if (!blueprint) return -1;
  if (blueprint->refc<1) return -1;
  if (blueprint->refc==INT_MAX) return -1;
  blueprint->refc++;
  return 0;
}

/* Get cell.
 */
 
uint8_t ps_blueprint_get_cell(const struct ps_blueprint *blueprint,int x,int y,uint8_t xform) {
  if (!blueprint) return PS_BLUEPRINT_CELL_SOLID;
  if ((x<0)||(x>=PS_BLUEPRINT_COLC)) return PS_BLUEPRINT_CELL_SOLID;
  if ((y<0)||(y>=PS_BLUEPRINT_ROWC)) return PS_BLUEPRINT_CELL_SOLID;
  if (xform&PS_BLUEPRINT_XFORM_HORZ) x=PS_GRID_COLC-x-1;
  if (xform&PS_BLUEPRINT_XFORM_VERT) y=PS_GRID_ROWC-y-1;
  return blueprint->cellv[y*PS_GRID_COLC+x];
}

/* Get POIs.
 */
 
int ps_blueprint_get_pois_for_cell(
  struct ps_blueprint_poi **dst,int dsta,
  const struct ps_blueprint *blueprint,
  int x,int y,uint8_t xform
) {
  if (!dst||(dsta<0)) dsta=0;
  if (!blueprint) return 0;
  if ((x<0)||(y<0)||(x>=PS_BLUEPRINT_COLC)||(y>=PS_BLUEPRINT_ROWC)) return 0;

  if (xform&PS_BLUEPRINT_XFORM_HORZ) x=PS_GRID_COLC-x-1;
  if (xform&PS_BLUEPRINT_XFORM_VERT) y=PS_GRID_ROWC-y-1;
  
  int dstc=0,i=blueprint->poic;
  struct ps_blueprint_poi *poi=blueprint->poiv;
  for (;i-->0;poi++) {
    if ((poi->x==x)&&(poi->y==y)) {
      if (dstc<dsta) dst[dstc]=poi;
      dstc++;
    }
  }
  return dstc;
}

/* Count POI of type.
 */
 
int ps_blueprint_count_poi_of_type(const struct ps_blueprint *blueprint,uint8_t type) {
  if (!blueprint) return 0;
  int i=blueprint->poic,c=0;
  const struct ps_blueprint_poi *poi=blueprint->poiv;
  for (;i-->0;poi++) {
    if (poi->type==type) c++;
  }
  return c;
}

/* Test solution.
 */

int ps_blueprint_is_solvable(
  uint8_t *difficulty,
  const struct ps_blueprint *blueprint,
  int playerc,uint16_t skills
) {
  if (!blueprint) return 0;

  if (blueprint->solutionc<1) {
    if (difficulty) *difficulty=0;
    return 1;
  }
  
  if (difficulty) *difficulty=0xff;
  int result=0;
  const struct ps_blueprint_solution *solution=blueprint->solutionv;
  int i=blueprint->solutionc; for (;i-->0;solution++) {
    if (playerc<solution->plo) continue;
    if (playerc>solution->phi) continue;
    if ((skills&solution->skills)!=solution->skills) continue;
    if (difficulty) {
      result=1;
      if (solution->difficulty<*difficulty) *difficulty=solution->difficulty;
    } else {
      return 1;
    }
  }
  return result;
}

/* Get preference.
 */
 
uint8_t ps_blueprint_get_preference(
  const struct ps_blueprint *blueprint,
  int playerc,uint16_t skills
) {
  if (!blueprint) return 0;
  if (blueprint->solutionc<1) return 1; // Challengeless default
  uint8_t difficulty=0xff;
  uint8_t preference=0;
  const struct ps_blueprint_solution *solution=blueprint->solutionv;
  int i=blueprint->solutionc; for (;i-->0;solution++) {
    if (playerc<solution->plo) continue;
    if (playerc>solution->phi) continue;
    if ((skills&solution->skills)!=solution->skills) continue;
    if (solution->difficulty<=difficulty) {
      difficulty=solution->difficulty;
      preference=solution->preference;
    }
  }
  return preference;
}

/* Clear.
 */
 
int ps_blueprint_clear(struct ps_blueprint *blueprint) {
  if (!blueprint) return -1;
  blueprint->poic=0;
  blueprint->solutionc=0;
  memset(blueprint->cellv,0,sizeof(blueprint->cellv));
  return 0;
}

/* Add POI.
 */
 
struct ps_blueprint_poi *ps_blueprint_add_poi(struct ps_blueprint *blueprint) {
  if (!blueprint) return 0;
  if (blueprint->poic>=255) return 0;

  if (blueprint->poic>=blueprint->poia) {
    int na=blueprint->poia+8;
    void *nv=realloc(blueprint->poiv,sizeof(struct ps_blueprint_poi)*na);
    if (!nv) return 0;
    blueprint->poiv=nv;
    blueprint->poia=na;
  }

  struct ps_blueprint_poi *poi=blueprint->poiv+blueprint->poic;
  blueprint->poic++;
  memset(poi,0,sizeof(struct ps_blueprint_poi));

  return poi;
}

/* Add solution.
 */
 
int ps_blueprint_add_solution(
  struct ps_blueprint *blueprint,
  uint8_t plo,uint8_t phi,
  uint8_t difficulty,uint8_t preference,
  uint16_t skills
) {
  if (!blueprint) return -1;
  if (blueprint->solutionc>=255) return -1;
  
  if (plo<1) plo=1;
  if (phi>PS_PLAYER_LIMIT) phi=PS_PLAYER_LIMIT;
  if (difficulty<PS_DIFFICULTY_MIN) difficulty=PS_DIFFICULTY_MIN;
  else if (difficulty>PS_DIFFICULTY_MAX) difficulty=PS_DIFFICULTY_MAX;
  if (preference<1) preference=1;

  if (plo>phi) {
    uint8_t tmp=plo;
    plo=phi;
    phi=tmp;
  }

  if (blueprint->solutionc>=blueprint->solutiona) {
    int na=blueprint->solutiona+8;
    void *nv=realloc(blueprint->solutionv,sizeof(struct ps_blueprint_solution)*na);
    if (!nv) return -1;
    blueprint->solutionv=nv;
    blueprint->solutiona=na;
  }

  struct ps_blueprint_solution *solution=blueprint->solutionv+blueprint->solutionc++;
  solution->plo=plo;
  solution->phi=phi;
  solution->difficulty=difficulty;
  solution->preference=preference;
  solution->skills=skills;
  
  return 1;
}

/* Encode.
 */
 
int ps_blueprint_encode(void *dst,int dsta,const struct ps_blueprint *blueprint) {
  if (!dst||(dsta<0)) dsta=0;
  if (!blueprint) return -1;
  uint8_t *DST=dst;
  int i;

  /* Begins with fixed 16-byte header. */
  if (dsta>=16) {
    DST[0]=PS_BLUEPRINT_SIZE>>8;
    DST[1]=PS_BLUEPRINT_SIZE;
    DST[2]=blueprint->solutionc;
    DST[3]=blueprint->poic;
    DST[4]=blueprint->monsterc_min;
    DST[5]=blueprint->monsterc_max;
    memset(DST+6,0,10);
  }
  int dstc=16;

  /* Copy cells. */
  if (dstc<=dsta-PS_BLUEPRINT_SIZE) memcpy(DST+dstc,blueprint->cellv,PS_BLUEPRINT_SIZE);
  dstc+=PS_BLUEPRINT_SIZE;

  /* Copy solutions. */
  const struct ps_blueprint_solution *solution=blueprint->solutionv;
  for (i=blueprint->solutionc;i-->0;solution++) {
    if (dstc<=dsta-6) {
      DST[dstc+ 0]=solution->plo;
      DST[dstc+ 1]=solution->phi;
      DST[dstc+ 2]=solution->difficulty;
      DST[dstc+ 3]=solution->preference;
      DST[dstc+ 4]=solution->skills>>8;
      DST[dstc+ 5]=solution->skills;
    }
    dstc+=6;
  }

  /* Copy POIs. */
  const struct ps_blueprint_poi *poi=blueprint->poiv;
  for (i=blueprint->poic;i-->0;poi++) {
    if (dstc<=dsta-16) {
      DST[dstc+ 0]=poi->x;
      DST[dstc+ 1]=poi->y;
      DST[dstc+ 2]=poi->type;
      DST[dstc+ 3]=0;
      DST[dstc+ 4]=poi->argv[0]>>24;
      DST[dstc+ 5]=poi->argv[0]>>16;
      DST[dstc+ 6]=poi->argv[0]>>8;
      DST[dstc+ 7]=poi->argv[0];
      DST[dstc+ 8]=poi->argv[1]>>24;
      DST[dstc+ 9]=poi->argv[1]>>16;
      DST[dstc+10]=poi->argv[1]>>8;
      DST[dstc+11]=poi->argv[1];
      DST[dstc+12]=poi->argv[2]>>24;
      DST[dstc+13]=poi->argv[2]>>16;
      DST[dstc+14]=poi->argv[2]>>8;
      DST[dstc+15]=poi->argv[2];
    }
    dstc+=16;
  }

  return dstc;
}

/* Decode text primitives.
 */

static int ps_blueprint_decode_int(int *dst,const char *src,int srcc,int lineno,int lo,int hi) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=ps_int_measure(token,srcc-srcp);
  if (tokenc<1) {
    ps_log(RES,ERROR,"%d: Expected integer.",lineno);
    return -1;
  }
  if (ps_int_eval_interactive(dst,token,tokenc,lo,hi,0)<0) {
    ps_log(RES,ERROR,"%d: At this line.",lineno);
    return -1;
  }
  srcp+=tokenc;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

static int ps_blueprint_decode_solution(uint16_t *dst,const char *src,int srcc,int lineno) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *sub=src+srcp;
  int subc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; subc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  int skill=ps_skill_eval(sub,subc);
  if (skill<0) {
    ps_log(RES,ERROR,"%d: '%.*s' is not a skill. (FLY,HEAL,FROG,SWORD,ARROW,FLAME,CARRY,SPEED,MARTYR,WEIGHT,HOOKSHOT,IMMORTAL)",lineno,subc,sub);
    return -1;
  }
  *dst=skill;

  return srcp;
}

static int ps_blueprint_decode_poi_type(int *dst,const char *src,int srcc,int lineno) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *sub=src+srcp;
  int subc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; subc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  *dst=-1;
  switch (subc) {
    case 4: {
        if (!memcmp(sub,"NOOP",4)) { *dst=PS_BLUEPRINT_POI_NOOP; break; }
        if (!memcmp(sub,"HERO",4)) { *dst=PS_BLUEPRINT_POI_HERO; break; }
      } break;
    case 6: {
        if (!memcmp(sub,"SPRITE",6)) { *dst=PS_BLUEPRINT_POI_SPRITE; break; }
      } break;
    case 7: {
        if (!memcmp(sub,"BARRIER",7)) { *dst=PS_BLUEPRINT_POI_BARRIER; break; }
      } break;
    case 8: {
        if (!memcmp(sub,"TREASURE",8)) { *dst=PS_BLUEPRINT_POI_TREASURE; break; }
      } break;
    case 9: {
        if (!memcmp(sub,"DEATHGATE",9)) { *dst=PS_BLUEPRINT_POI_DEATHGATE; break; }
      } break;
  }
  
  if (*dst<0) {
    ps_log(RES,ERROR,"%d: '%.*s' is not a POI type. (NOOP,HERO,SPRITE,BARRIER,TREASURE,DEATHGATE)",lineno,subc,sub);
    return -1;
  }

  return srcp;
}

/* Decode text header line.
 */

static int ps_blueprint_decode_line_header(struct ps_blueprint *blueprint,const char *src,int srcc,int lineno) {
  const char *kw=src;
  int kwc=0,srcp=0,err;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }

  #define FINISH \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    if (srcp<srcc) { \
      ps_log(RES,ERROR,"%d: Unexpected tokens after blueprint header line.",lineno); \
      return -1; \
    } \
    return 0;
  
  if ((kwc==8)&&!memcmp(kw,"solution",8)) {
    int plo,phi,difficulty,preference;
    uint16_t skills=0;
    
    if ((err=ps_blueprint_decode_int(&plo,src+srcp,srcc-srcp,lineno,1,PS_PLAYER_LIMIT))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&phi,src+srcp,srcc-srcp,lineno,plo,PS_PLAYER_LIMIT))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&difficulty,src+srcp,srcc-srcp,lineno,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&preference,src+srcp,srcc-srcp,lineno,1,255))<0) return -1; srcp+=err;
    
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      uint16_t sub;
      if ((err=ps_blueprint_decode_solution(&sub,src+srcp,srcc-srcp,lineno))<0) return -1;
      srcp+=err;
      skills|=sub;
    }

    if (ps_blueprint_add_solution(blueprint,plo,phi,difficulty,preference,skills)<0) return -1;
    FINISH
  }
  
  if ((kwc==3)&&!memcmp(kw,"poi",3)) {
    int x,y,type,arg0,arg1,arg2;
    if ((err=ps_blueprint_decode_int(&x,src+srcp,srcc-srcp,lineno,0,PS_GRID_COLC-1))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&y,src+srcp,srcc-srcp,lineno,0,PS_GRID_ROWC-1))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_poi_type(&type,src+srcp,srcc-srcp,lineno))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&arg0,src+srcp,srcc-srcp,lineno,INT_MIN,INT_MAX))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&arg1,src+srcp,srcc-srcp,lineno,INT_MIN,INT_MAX))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&arg2,src+srcp,srcc-srcp,lineno,INT_MIN,INT_MAX))<0) return -1; srcp+=err;
    struct ps_blueprint_poi *poi=ps_blueprint_add_poi(blueprint);
    if (!poi) return -1;
    poi->x=x;
    poi->y=y;
    poi->type=type;
    poi->argv[0]=arg0;
    poi->argv[1]=arg1;
    poi->argv[2]=arg2;
    FINISH
  }

  if ((kwc==8)&&!memcmp(kw,"monsters",8)) {
    int lo,hi;
    if ((err=ps_blueprint_decode_int(&lo,src+srcp,srcc-srcp,lineno,0,255))<0) return -1; srcp+=err;
    if ((err=ps_blueprint_decode_int(&hi,src+srcp,srcc-srcp,lineno,0,255))<0) return -1; srcp+=err;
    if (lo>hi) {
      ps_log(RES,ERROR,"%d: Monster counts (%d,%d) don't make sense.",lineno,lo,hi);
      return -1;
    }
    blueprint->monsterc_min=lo;
    blueprint->monsterc_max=hi;
    FINISH
  }

  #undef FINISH
  ps_log(RES,ERROR,"%d: Unexpected keyword '%.*s'. (playerc_min,playerc_max,solution,poi)",lineno,kwc,kw);
  return -1;
}

/* Decode text body line.
 */

static int ps_blueprint_decode_line_body(struct ps_blueprint *blueprint,const char *src,int srcc,int lineno,int y) {
  if (y>=PS_BLUEPRINT_ROWC) {
    ps_log(RES,ERROR,"%d: Extra rows of blueprint body data.",lineno);
    return -1;
  }

  int colc=(srcc+1)>>1;
  if (colc!=PS_BLUEPRINT_COLC) {
    ps_log(RES,ERROR,"%d: Expected %d columns, found %d.",lineno,PS_BLUEPRINT_COLC,colc);
    return -1;
  }

  uint8_t *dst=blueprint->cellv+y*PS_BLUEPRINT_COLC;
  int i=PS_BLUEPRINT_COLC; for (;i-->0;dst++,src+=2) switch (*src) {
    case ',': *dst=PS_BLUEPRINT_CELL_VACANT; break;
    case 'X': *dst=PS_BLUEPRINT_CELL_SOLID; break;
    case 'O': *dst=PS_BLUEPRINT_CELL_HOLE; break;
    case 'L': *dst=PS_BLUEPRINT_CELL_LATCH; break;
    case '-': *dst=PS_BLUEPRINT_CELL_HEROONLY; break;
    case '!': *dst=PS_BLUEPRINT_CELL_HAZARD; break;
    case 'H': *dst=PS_BLUEPRINT_CELL_HEAL; break;
    default: {
        ps_log(RES,ERROR,"%d: Unexpected character '%c' in blueprint body. [,XOL?-!]",lineno,*src);
        return -1;
      }
  }
  
  return 0;
}

/* Decode text.
 */

static int ps_blueprint_decode_text(struct ps_blueprint *blueprint,const char *src,int srcc) {

  struct ps_line_reader reader;
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  if (ps_blueprint_clear(blueprint)<0) return -1;

  int y=-1;
  while (1) {
    int err=ps_line_reader_next(&reader);
    if (err<0) return -1;
    if (!err) break;
    if (!reader.linec) continue;
    
    if (y>=0) {
      if (ps_blueprint_decode_line_body(blueprint,reader.line,reader.linec,reader.lineno,y)<0) return -1;
      y++;
    } else if (reader.line[0]=='=') {
      y=0;
    } else {
      if (ps_blueprint_decode_line_header(blueprint,reader.line,reader.linec,reader.lineno)<0) return -1;
    }
  }
  
  if (y<0) {
    ps_log(RES,ERROR,"Blueprint has no body.");
    return -1;
  } else if (y<PS_BLUEPRINT_ROWC) {
    ps_log(RES,ERROR,"Incomplete blueprint (%d rows, expected %d).",y,PS_BLUEPRINT_ROWC);
    return -1;
  }
  return 0;
}

/* Decode.
 */
 
int ps_blueprint_decode(struct ps_blueprint *blueprint,const void *src,int srcc) {
  if (!blueprint||(srcc<16)||!src) return -1;
  const uint8_t *SRC=src;

  /* If the first byte is 0x09 or greater, assume it is text.
   * That would otherwise be the high byte of cell count, and would be way out of range.
   */
  if (SRC[0]>=0x09) return ps_blueprint_decode_text(blueprint,src,srcc);

  /* Decode 16-byte header. Ignore reserved zone. */
  uint16_t cellc=(SRC[0]<<8)|SRC[1];
  uint8_t solutionc=SRC[2];
  uint8_t poic=SRC[3];

  /* Validate header. */
  if (cellc!=PS_BLUEPRINT_SIZE) {
    ps_log(RES,ERROR,"Invalid blueprint size %d, must be %d*%d==%d.",cellc,PS_BLUEPRINT_COLC,PS_BLUEPRINT_ROWC,PS_BLUEPRINT_SIZE);
    return -1;
  }

  /* Calculate and validate expected size. OK if it's longer than expected. */
  int expected_size=16+cellc+(solutionc*6)+(poic*16);
  if (srcc<expected_size) {
    ps_log(RES,ERROR,"Blueprint too small to contain declared elements (srcc=%d,solutionc=%d,poic=%d)",srcc,solutionc,poic);
    return -1;
  }

  /* Looks good. Wipe blueprint and begin copying. */
  if (ps_blueprint_clear(blueprint)<0) return -1;
  int srcp=16;
  memcpy(blueprint->cellv,SRC+srcp,cellc);
  srcp+=cellc;

  /* Copy solutions. */
  while (solutionc-->0) {
    uint8_t plo=SRC[srcp++];
    uint8_t phi=SRC[srcp++];
    uint8_t difficulty=SRC[srcp++];
    uint8_t preference=SRC[srcp++];
    uint8_t skills=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
    if (ps_blueprint_add_solution(blueprint,plo,phi,difficulty,preference,skills)<0) return -1;
  }

  /* Copy POIs. */
  while (poic-->0) {
    struct ps_blueprint_poi *poi=ps_blueprint_add_poi(blueprint);
    if (!poi) return -1;
    poi->x=SRC[srcp+0];
    poi->y=SRC[srcp+1];
    poi->type=SRC[srcp+2];
    poi->argv[0]=(SRC[srcp+ 4]<<24)|(SRC[srcp+ 5]<<16)|(SRC[srcp+ 6]<<8)|SRC[srcp+ 7];
    poi->argv[1]=(SRC[srcp+ 8]<<24)|(SRC[srcp+ 9]<<16)|(SRC[srcp+10]<<8)|SRC[srcp+11];
    poi->argv[2]=(SRC[srcp+12]<<24)|(SRC[srcp+13]<<16)|(SRC[srcp+14]<<8)|SRC[srcp+15];
    srcp+=16;
  }

  /* Assert that we ended in the right place. */
  if (srcp!=expected_size) {
    ps_log(RES,ERROR,"INTERNAL ERROR! Expected blueprint size %d, ended up at %d.",expected_size,srcp);
    return -1;
  }

  return 0;
}

/* Evaluate cell name.
 */
 
int ps_blueprint_cell_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return PS_BLUEPRINT_CELL_##tag;
  _(VACANT)
  _(SOLID)
  _(HOLE)
  _(LATCH)
  _(HEROONLY)
  _(HAZARD)
  _(HEAL)
  #undef _
  return -1;
}
