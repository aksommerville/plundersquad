#include "ps.h"
#include "ps_sprite.h"
#include "ps_game.h"
#include "util/ps_text.h"

/* Object lifecycle.
 */

struct ps_sprdef *ps_sprdef_new(int fldc) {

  if (fldc<0) return 0;
  if (fldc>INT_MAX/sizeof(struct ps_sprdef_fld)) return 0;
  int basesize=sizeof(struct ps_sprdef);
  int addsize=sizeof(struct ps_sprdef_fld)*fldc;
  if (basesize>INT_MAX-addsize) return 0;

  struct ps_sprdef *sprdef=calloc(1,basesize+addsize);
  if (!sprdef) return 0;

  sprdef->refc=1;
  sprdef->fldc=fldc;

  return sprdef;
}
  
void ps_sprdef_del(struct ps_sprdef *sprdef) {
  if (!sprdef) return;
  if (sprdef->refc-->1) return;

  free(sprdef);
}

int ps_sprdef_ref(struct ps_sprdef *sprdef) {
  if (!sprdef) return -1;
  if (sprdef->refc<1) return -1;
  if (sprdef->refc==INT_MAX) return -1;
  sprdef->refc++;
  return 0;
}

/* Get field.
 */

int ps_sprdef_fld_search(const struct ps_sprdef *sprdef,int k) {
  if (!sprdef) return -1;
  int lo=0,hi=sprdef->fldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (k<sprdef->fldv[ck].k) hi=ck;
    else if (k>sprdef->fldv[ck].k) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ps_sprdef_fld_get(const struct ps_sprdef *sprdef,int k,int def) {
  int p=ps_sprdef_fld_search(sprdef,k);
  if (p<0) return def;
  return sprdef->fldv[p].v;
}

/* Instantiate.
 */

static int ps_sprite_initialize(struct ps_sprite *spr,struct ps_game *game,struct ps_sprdef *sprdef,const int *argv,int argc,int x,int y) {
  if (ps_sprite_set_sprdef(spr,sprdef)<0) return -1;

  int defaultgroups=(
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)
  );
  int grpmask=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_grpmask,defaultgroups);
  int i,test=1;
  for (i=0;i<PS_SPRGRP_COUNT;i++,test<<=1) {
    if (grpmask&test) {
      if (ps_sprite_add_sprgrp(spr,game->grpv+i)<0) return -1;
    }
  }

  spr->x=x;
  spr->y=y;

  spr->tsid=sprdef->tileid>>8;
  spr->tileid=sprdef->tileid;

  spr->layer=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_layer,spr->layer);
  spr->radius=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_radius,spr->radius);
  spr->shape=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_shape,spr->shape);

  if (spr->type->configure) {
    if (spr->type->configure(spr,game,argv,argc)<0) return -1;
  }

  return 0;
}

struct ps_sprite *ps_sprdef_instantiate(struct ps_game *game,struct ps_sprdef *sprdef,const int *argv,int argc,int x,int y) {
  if (!game||!sprdef) return 0;
  if ((argc<0)||(argc&&!argv)) return 0;

  struct ps_sprite *spr=ps_sprite_new(sprdef->type);
  if (!spr) return 0;

  if (ps_sprite_add_sprgrp(spr,game->grpv+PS_SPRGRP_KEEPALIVE)<0) {
    ps_sprite_del(spr);
    return 0;
  }
  ps_sprite_del(spr);

  if (ps_sprite_initialize(spr,game,sprdef,argv,argc,x,y)<0) {
    ps_sprite_kill(spr);
    return 0;
  }

  return spr;
}

/* Evaluate field key.
 */

static int ps_sprdef_fld_k_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  if ((srcc==5)&&!memcmp(src,"layer",5)) return PS_SPRDEF_FLD_layer;
  if ((srcc==7)&&!memcmp(src,"grpmask",7)) return PS_SPRDEF_FLD_grpmask;
  if ((srcc==6)&&!memcmp(src,"radius",6)) return PS_SPRDEF_FLD_radius;
  if ((srcc==5)&&!memcmp(src,"shape",5)) return PS_SPRDEF_FLD_shape;
  
  return -1;
}

/* Evaluate value for extended field.
 * Integers are always acceptable, and some keys have special processing first.
 */

static int ps_sprite_grpmask_eval_1(int *dst,const char *src,int srcc) {
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) { *dst=1<<PS_SPRGRP_##tag; return 0; }
  //_(KEEPALIVE) // Can't explicitly request KEEPALIVE; they all get it
  //_(DEATHROW) // Can't explicitly request DEATHROW; it's not that kind of group
  _(VISIBLE)
  _(UPDATE)
  _(PHYSICS)
  _(HERO)
  _(HAZARD)
  _(HEROHAZARD)
  _(FRAGILE)
  _(TREASURE)
  _(LATCH)
  _(SOLID)
  #undef _
  return ps_int_eval(dst,src,srcc);
}

static int ps_sprite_grpmask_eval(int *dst,const char *src,int srcc) {
  *dst=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
    int mask1;
    if (ps_sprite_grpmask_eval_1(&mask1,token,tokenc)<0) {
      ps_log(RES,ERROR,"'%.*s' is not a valid sprite group mask component",tokenc,token);
      return -1;
    }
    *dst|=mask1;
  }
  return 0;
}

static int ps_sprite_shape_eval(int *dst,const char *src,int srcc) {
  if ((srcc==6)&&!ps_memcasecmp(src,"SQUARE",6)) { *dst=PS_SPRITE_SHAPE_SQUARE; return 0; }
  if ((srcc==6)&&!ps_memcasecmp(src,"CIRCLE",6)) { *dst=PS_SPRITE_SHAPE_CIRCLE; return 0; }
  return -1;
}

static int ps_sprdef_fld_v_eval(int *dst,int k,const char *src,int srcc) {
  switch (k) {
    case PS_SPRDEF_FLD_layer: break;
    case PS_SPRDEF_FLD_grpmask: if (ps_sprite_grpmask_eval(dst,src,srcc)>=0) return 0; break;
    case PS_SPRDEF_FLD_radius: break;
    case PS_SPRDEF_FLD_shape: if (ps_sprite_shape_eval(dst,src,srcc)>=0) return 0; break;
  }
  return ps_int_eval(dst,src,srcc);
}

/* Set base field (not in fldv).
 */

static int ps_sprdef_set_fld_type(struct ps_sprdef *sprdef,const char *src,int srcc) {
  const struct ps_sprtype *type=ps_sprtype_by_name(src,srcc);
  if (!type) {
    ps_log(RES,ERROR,"No sprite type '%.*s'.",srcc,src);
    return -1;
  }
  sprdef->type=type;
  return 0;
}

static int ps_sprdef_set_fld_tileid(struct ps_sprdef *sprdef,const char *src,int srcc) {
  int v; 
  if (ps_int_eval_interactive(&v,src,srcc,0,0xffff,"tileid")<0) return -1;
  sprdef->tileid=v;
  return 0;
}

static int ps_sprdef_set_base_field(struct ps_sprdef *sprdef,const char *k,int kc,const char *v,int vc) {
  if ((kc==4)&&!memcmp(k,"type",4)) return ps_sprdef_set_fld_type(sprdef,v,vc);
  if ((kc==6)&&!memcmp(k,"tileid",6)) return ps_sprdef_set_fld_tileid(sprdef,v,vc);
  ps_log(RES,ERROR,"Unexpected key '%.*s' in sprdef.",kc,k);
  return -1;
}

/* Count fields in encoded sprdef.
 */

static int ps_sprdef_decode_count_fields(const char *src,int srcc) {
  int fldc=0;
  struct ps_line_reader reader;
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (1) {
    int err=ps_line_reader_next(&reader);
    if (err<0) return -1;
    if (!err) break;
    if (!reader.linec) continue;
    int kwc=0;
    while ((kwc<reader.linec)&&((unsigned char)reader.line[kwc]>0x20)) kwc++;
    if (ps_sprdef_fld_k_eval(reader.line,kwc)>=0) fldc++;
  }
  return fldc;
}

/* Deocde sprdef, single line.
 */

static int ps_sprdef_decode_line(struct ps_sprdef *sprdef,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  const char *k=src+srcp;
  int kc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *v=src+srcp;
  int vc=srcc-srcp;
  while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;

  int fldk=ps_sprdef_fld_k_eval(k,kc);
  if (fldk>=0) {
    int p=ps_sprdef_fld_search(sprdef,fldk);
    if (p>=0) {
      ps_log(RES,ERROR,"Duplicate sprdef field '%.*s'.",kc,k);
      return -1;
    }
    p=-p-1;
    memmove(sprdef->fldv+p+1,sprdef->fldv+p,sizeof(struct ps_sprdef_fld)*(sprdef->fldc-p));
    sprdef->fldc++;
    sprdef->fldv[p].k=fldk;
    if (ps_sprdef_fld_v_eval(&sprdef->fldv[p].v,fldk,v,vc)<0) {
      ps_log(RES,ERROR,"Failed to evalute '%.*s' for sprdef field '%.*s'.",vc,v,kc,k);
      return -1;
    }
  } else {
    if (ps_sprdef_set_base_field(sprdef,k,kc,v,vc)<0) return -1;
  }
  return 0;
}

/* Decode sprdef, split lines.
 */

static int ps_sprdef_decode_inner(struct ps_sprdef *sprdef,const char *src,int srcc) {
  sprdef->fldc=0;
  struct ps_line_reader reader;
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (1) {
    int err=ps_line_reader_next(&reader);
    if (err<0) return -1;
    if (!err) break;
    if (!reader.linec) continue;
    if (ps_sprdef_decode_line(sprdef,reader.line,reader.linec)<0) return -1;
  }
  return 0;
}

/* Decode.
 */
 
struct ps_sprdef *ps_sprdef_decode(const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  int fldc=ps_sprdef_decode_count_fields(src,srcc);
  if (fldc<0) return 0;

  struct ps_sprdef *sprdef=ps_sprdef_new(fldc);
  if (!sprdef) return 0;

  if (ps_sprdef_decode_inner(sprdef,src,srcc)<0) {
    ps_sprdef_del(sprdef);
    return 0;
  }

  if (!sprdef->type) {
    ps_log(RES,ERROR,"sprdef did not declare a type.");
    ps_sprdef_del(sprdef);
    return 0;
  }
  
  return sprdef;
}
