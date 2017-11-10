#include "ps.h"
#include "ps_region.h"
#include "ps_blueprint.h"
#include "util/ps_text.h"

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
      if (ps_int_eval_interactive(&region->songid,arg,argc,0,255,"song")<0) return -1;
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

/* Evaluate shape style.
 */
 
int ps_region_shape_style_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return PS_REGION_SHAPE_STYLE_##tag;
  _(SINGLE)
  _(ALT4)
  _(ALT8)
  _(EVEN4)
  _(SKINNY)
  _(FAT)
  _(3X3)
  _(ALT16)
  #undef _
  return -1;
}
