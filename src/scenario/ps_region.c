#include "ps.h"
#include "ps_region.h"
#include "ps_blueprint.h"
#include "util/ps_text.h"
#include "util/ps_buffer.h"
#include "util/ps_enums.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "game/ps_sprite.h"

static struct ps_region *ps_region_decode_binary(const void *src,int srcc);

/* New.
 */
 
struct ps_region *ps_region_new(int shapec) {
  if (shapec<0) return 0;

  int basesize=sizeof(struct ps_region);
  if (shapec>INT_MAX/sizeof(struct ps_region_shape)) return 0;
  int addsize=shapec*sizeof(struct ps_region_shape);
  if (basesize>INT_MAX-addsize) return 0;

  struct ps_region *region=calloc(1,basesize+addsize);
  if (!region) return 0;

  region->shapec=shapec;

  return region;
}

/* Delete.
 */
 
void ps_region_del(struct ps_region *region) {
  if (!region) return;
  free(region);
}

/* Sort shapes in place by physics.
 */

static inline int ps_region_compare_shapes(const struct ps_region_shape *a,const struct ps_region_shape *b) {
  if (a->physics<b->physics) return -1;
  if (a->physics>b->physics) return 1;
  return 0;
}

int ps_region_sort_shapes(struct ps_region *region) {
  if (!region) return -1;
  int lo=0,hi=region->shapec-1,d=1;
  while (lo<hi) {
    int first,last,i,done=1;
    if (d==1) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      int cmp=ps_region_compare_shapes(region->shapev+i,region->shapev+i+d);
      if (cmp==d) {
        struct ps_region_shape tmp=region->shapev[i];
        region->shapev[i]=region->shapev[i+d];
        region->shapev[i+d]=tmp;
        done=0;
      }
    }
    if (done) break;
    if (d==1) { hi--; d=-1; }
    else { lo++; d=1; }
  }
  return 0;
}

/* Get all shapes for given physics.
 */

int ps_region_get_shapes(const struct ps_region_shape **dst,const struct ps_region *region,uint8_t physics) {
  if (!dst||!region) return 0;
  int lo=0,hi=region->shapec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (physics<region->shapev[ck].physics) hi=ck;
    else if (physics>region->shapev[ck].physics) lo=ck+1;
    else {
      int c=1;
      while ((ck>lo)&&(region->shapev[ck-1].physics==physics)) { ck--; c++; }
      while ((ck+c<hi)&&(region->shapev[ck+c].physics==physics)) c++;
      *dst=region->shapev+ck;
      return c;
    }
  }
  *dst=region->shapev+lo;
  return 0;
}

/* Shape selection helpers.
 */
 
int ps_region_shapes_calculate_weight(const struct ps_region_shape *shape,int shapec) {
  if (!shape||(shapec<1)) return 0;
  int weight=0;
  for (;shapec--;shape++) {
    weight+=shape->weight;
  }
  return weight;
}

const struct ps_region_shape *ps_region_shapes_get_by_weight(const struct ps_region_shape *shape,int shapec,int selection) {
  if (!shape||(shapec<1)||(selection<0)) return 0;
  for (;shapec-->0;shape++) {
    if (selection<shape->weight) return shape;
    selection-=shape->weight;
  }
  return 0;
}

/* Count shapes in encoded region.
 */

static int ps_region_text_count_shapes(const char *src,int srcc) {
  int srcp=0,shapec=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) { srcp++; linec++; }
    if ((linec>=5)&&!memcmp(line,"shape",5)) shapec++;
  }
  return shapec;
}

/* Count valid monster IDs.
 */
 
int ps_region_count_monsters(const struct ps_region *region) {
  if (!region) return 0;
  int i=PS_REGION_MONSTER_LIMIT,c=0;
  while (i-->0) if (region->monster_sprdefidv[i]) c++;
  return c;
}
 
int ps_region_count_monsters_at_difficulty(const struct ps_region *region,int difficulty) {
  if (!region) return 0;
  int i=PS_REGION_MONSTER_LIMIT,c=0;
  while (i-->0) if (region->monster_sprdefidv[i]) {
    const struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,region->monster_sprdefidv[i]);
    if (!sprdef) continue; // This is a seriaus error, but we can sweep it under the carpet.
    int sprdef_difficulty=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_difficulty,0);
    if (sprdef_difficulty>difficulty) continue;
    c++;
  }
  return c;
}

/* Get a monster ID by its index.
 */
 
int ps_region_get_monster(const struct ps_region *region,int p) {
  if (!region) return -1;
  if (p<0) return -1;
  int i=PS_REGION_MONSTER_LIMIT;
  while (i-->0) {
    if (region->monster_sprdefidv[i]) {
      if (!p--) return region->monster_sprdefidv[i];
    }
  }
  return -1;
}
 
int ps_region_get_monster_at_difficulty(const struct ps_region *region,int p,int difficulty) {
  if (!region) return -1;
  if (p<0) return -1;
  int i=PS_REGION_MONSTER_LIMIT;
  while (i-->0) {
    if (region->monster_sprdefidv[i]) {
      const struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,region->monster_sprdefidv[i]);
      if (!sprdef) continue; // This is a seriaus error, but we can sweep it under the carpet.
      int sprdef_difficulty=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_difficulty,0);
      if (sprdef_difficulty>difficulty) continue;
      if (!p--) return region->monster_sprdefidv[i];
    }
  }
  return -1;
}

/* Test presence of a monster ID.
 */

int ps_region_has_monster(const struct ps_region *region,int sprdefid) {
  if (!region) return 0;
  if (sprdefid<=0) return 0;
  int i=PS_REGION_MONSTER_LIMIT;
  while (i-->0) {
    if (region->monster_sprdefidv[i]==sprdefid) return 1;
  }
  return 0;
}

/* Add a monster ID.
 */
 
int ps_region_add_monster(struct ps_region *region,int sprdefid) {
  if (!region) return -1;
  if (sprdefid<=0) return -1;
  if (ps_region_has_monster(region,sprdefid)) return 0; // Redundant is OK.
  int i=PS_REGION_MONSTER_LIMIT;
  while (i-->0) {
    if (!region->monster_sprdefidv[i]) {
      region->monster_sprdefidv[i]=sprdefid;
      return 0;
    }
  }
  return -1;
}

/* Add any monster ID.
 */

int ps_region_add_any_valid_monster(struct ps_region *region) {
  if (!region) return -1;
  int p=-1;
  int highest=0;
  int i=PS_REGION_MONSTER_LIMIT; while (i-->0) {
    if (!region->monster_sprdefidv[i]) {
      p=i;
    } else if (region->monster_sprdefidv[i]>highest) {
      highest=region->monster_sprdefidv[i];
    }
  }
  if (p<0) return -1; // Can't add; already full.
  if (ps_res_get(PS_RESTYPE_SPRDEF,highest+1)) { // Easiest solution is take the first ID above the current highest.
    return region->monster_sprdefidv[p]=highest+1;
  }
  // The hard way: Scan all sprdef resources until we find one not in use.
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_SPRDEF);
  if (!restype) return -1;
  const struct ps_res *res=restype->resv;
  for (i=restype->resc;i-->0;res++) {
    if (!ps_region_has_monster(region,res->id)) {
      return region->monster_sprdefidv[p]=res->id;
    }
  }
  return -1; // We are already using every sprdef...?
}

/* Decode shape declaration.
 */

static int ps_region_decode_shape(struct ps_region_shape *shape,const char *src,int srcc) {
  int srcp=0,subc,v;
  const char *sub;

  #define NEXTTOKEN \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    sub=src+srcp; subc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; subc++; }

  NEXTTOKEN
  if ((v=ps_blueprint_cell_eval(sub,subc))<0) {
    ps_log(RES,ERROR,"Failed to evaluate '%.*s' as region shape physics.",subc,sub);
    return -1;
  }
  shape->physics=v;

  NEXTTOKEN
  if (ps_int_eval_interactive(&v,sub,subc,0,255,"weight")<0) return -1;
  shape->weight=v;

  NEXTTOKEN
  if (ps_int_eval_interactive(&v,sub,subc,0,255,"tile")<0) return -1;
  shape->tileid=v;

  NEXTTOKEN
  if ((v=ps_region_shape_style_eval(sub,subc))<0) {
    ps_log(RES,ERROR,"Failed to evaluate '%.*s' as region shape style.",subc,sub);
    return -1;
  }
  shape->style=v;

  while (srcp<srcc) {
    NEXTTOKEN
    if (!subc) break;
    if ((v=ps_region_shape_flag_eval(sub,subc))<0) {
      ps_log(RES,ERROR,"Failed to evaluate '%.*s' as region shape flag.",subc,sub);
      return -1;
    }
    shape->flags|=v;
  }

  #undef NEXTTOKEN

  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) {
    ps_log(RES,ERROR,"Unexpected text '%.*s' after region shape declaration.",srcc-srcp,src+srcp);
    return -1;
  }
  
  return 0;
}

/* Decode resource into allocated object.
 */

static int ps_region_decode_inner(struct ps_region *region,const char *src,int srcc) {

  struct ps_line_reader reader;
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;

  int shapec=0,tilesheetc=0,songc=0;
  while (1) {
    int err=ps_line_reader_next(&reader);
    if (err<0) return -1;
    if (!err) break;
    if (!reader.linec) continue;

    const char *kw=reader.line;
    int kwc=0;
    while ((kwc<reader.linec)&&((unsigned char)kw[kwc]>0x20)) kwc++;
    const char *arg=reader.line+kwc;
    int argc=reader.linec-kwc;
    while (arg&&((unsigned char)arg[0]<=0x20)) { arg++; argc--; }

    if ((kwc==5)&&!memcmp(kw,"shape",5)) {
      if (shapec>=region->shapec) { shapec++; break; } // let it fail below
      if (ps_region_decode_shape(region->shapev+shapec,arg,argc)<0) return -1;
      shapec++;
      
    } else if ((kwc==9)&&!memcmp(kw,"tilesheet",9)) {
      if (tilesheetc) {
        ps_log(RES,ERROR,"%d: Multiple tilesheet in region.",reader.lineno);
        return -1;
      }
      if (ps_int_eval_interactive(&region->tsid,arg,argc,0,255,"tilesheet")<0) return -1;
      tilesheetc++;
      
    } else if ((kwc==4)&&!memcmp(kw,"song",4)) {
      if (songc) {
        ps_log(RES,ERROR,"%d: Multiple song in region.",reader.lineno);
        return -1;
      }
      if (ps_int_eval_interactive(&region->songid,arg,argc,0,INT_MAX,"song")<0) return -1;
      songc++;

    } else if ((kwc==7)&&!memcmp(kw,"monster",7)) {
      int sprdefid;
      if (ps_int_eval_interactive(&sprdefid,arg,argc,1,INT_MAX,"monster")<0) return -1;
      if (ps_region_add_monster(region,sprdefid)<0) {
        ps_log(RES,ERROR,"%d: Failed to add monster (over limit of %d?)",reader.lineno,PS_REGION_MONSTER_LIMIT);
        return -1;
      }
      
    } else {
      ps_log(RES,ERROR,"%d: Unexpected keyword '%.*s' in region.",reader.lineno,kwc,kw);
      return -1;
    }
  }
  
  if (shapec!=region->shapec) {
    ps_log(RES,ERROR,"Expected %d shapes in region, found %d.",region->shapec,shapec);
    return -1;
  }
  if (!tilesheetc) {
    ps_log(RES,ERROR,"Region does not declare a tilesheet.");
    return -1;
  }
  if (!songc) {
    ps_log(RES,ERROR,"Region does not declare a song.");
    return -1;
  }

  if (ps_region_sort_shapes(region)<0) return -1;
  
  return 0;
}

/* Decode resource.
 */

struct ps_region *ps_region_decode(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  if ((srcc>=2)&&!memcmp(src,"\xfa\xfa",2)) return ps_region_decode_binary(src,srcc);
  
  int shapec=ps_region_text_count_shapes(src,srcc);
  if (shapec<0) return 0;
  struct ps_region *region=ps_region_new(shapec);
  if (!region) return 0;

  if (ps_region_decode_inner(region,src,srcc)<0) {
    ps_region_del(region);
    return 0;
  }
  return region;
}

/* Encode resource.
 */

static int ps_region_encode_to_buffer(struct ps_buffer *buffer,const struct ps_region *region) {

  if (ps_buffer_appendf(buffer,"tilesheet %d\n",region->tsid)<0) return -1;
  if (ps_buffer_appendf(buffer,"song %d\n",region->songid)<0) return -1;

  const int *sprdefid=region->monster_sprdefidv;
  int i=PS_REGION_MONSTER_LIMIT; for (;i-->0;sprdefid++) {
    if (*sprdefid) {
      if (ps_buffer_appendf(buffer,"monster %d\n",*sprdefid)<0) return -1;
    }
  }

  const struct ps_region_shape *shape=region->shapev;
  for (i=region->shapec;i-->0;shape++) {
    const char *physics=ps_blueprint_cell_repr(shape->physics);
    const char *style=ps_region_shape_style_repr(shape->style);
    const char *flags=ps_region_shape_flag_repr(shape->flags); //TODO must change if we add a second flag
    if (!physics||!style||!flags) {
      ps_log(RES,ERROR,"Unable to represent region shape (%d,%d,%d) = (%s,%s,%s)",shape->physics,shape->style,shape->flags,physics,style,flags);
      return -1;
    }
    if (ps_buffer_appendf(buffer,"shape %s %d %d %s %s\n",physics,shape->weight,shape->tileid,style,flags)<0) return -1;
  }
  
  return 0;
}

int ps_region_encode(void *dstpp,const struct ps_region *region) {
  if (!dstpp||!region) return -1;
  struct ps_buffer buffer={0};
  if (ps_region_encode_to_buffer(&buffer,region)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  *(void**)dstpp=buffer.v;
  return buffer.c;
}

/* Encode binary.
 */
 
int ps_region_encode_binary(void *dst,int dsta,const struct ps_region *region) {
  if (!dst||(dsta<0)) dsta=0;
  if (!region) return -1;
  int dstc=13+5*region->shapec;
  if (dstc>dsta) return dstc;
  uint8_t *DST=dst;
  
  DST[0]=0xfa;
  DST[1]=0xfa;
  DST[2]=region->tsid;
  DST[3]=region->songid;
  int i=0; for (;i<8;i++) DST[4+i]=region->monster_sprdefidv[i];
  DST[12]=region->shapec;
  if (sizeof(struct ps_region_shape)!=5) return -1;
  memcpy(DST+13,region->shapev,5*region->shapec);
  
  return dstc;
}

/* Decode binary.
 */
 

static struct ps_region *ps_region_decode_binary(const void *src,int srcc) {
  if (!src||(srcc<13)) return 0;
  if (memcmp(src,"\xfa\xfa",2)) return 0;
  const uint8_t *SRC=src;
  int shapec=SRC[12];
  if (shapec&&(sizeof(struct ps_region_shape)!=5)) return 0;
  struct ps_region *region=ps_region_new(shapec);
  if (!region) return 0;
  
  region->tsid=SRC[2];
  region->songid=SRC[3];
  int i=0; for (;i<8;i++) region->monster_sprdefidv[i]=SRC[4+i];
  memcpy(region->shapev,SRC+13,5*shapec);
  
  return region;
}
