#include "ps.h"
#include "ps_sprite.h"
#include "ps_game.h"
#include "util/ps_text.h"
#include "util/ps_enums.h"
#include "util/ps_buffer.h"

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
  spr->impassable=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_impassable,spr->impassable);

  if (spr->type->configure) {
    if (spr->type->configure(spr,game,argv,argc,sprdef)<0) return -1;
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

/* Encode or decode field value as text.
 */

int ps_sprdef_fld_v_eval(int *dst,int k,const char *src,int srcc) {
  if (!dst) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (k) {
    case PS_SPRDEF_FLD_grpmask: return ps_enum_eval_multiple(dst,src,srcc,0,ps_sprgrp_eval);
    case PS_SPRDEF_FLD_shape: {
        if ((*dst=ps_sprite_shape_eval(src,srcc))<0) return -1;
        return 0;
      }
    case PS_SPRDEF_FLD_type: return -1; // Type is special, it's not an integer.
    case PS_SPRDEF_FLD_impassable: return ps_enum_eval_multiple(dst,src,srcc,0,ps_blueprint_cell_eval);
    default: return ps_int_eval(dst,src,srcc);
  }
  return -1;
}

int ps_sprdef_fld_v_repr(char *dst,int dsta,int k,int v) {
  if (!dst||(dsta<0)) dsta=0;
  switch (k) {
    case PS_SPRDEF_FLD_grpmask: return ps_enum_repr_multiple(dst,dsta,v,0,ps_sprgrp_repr);
    case PS_SPRDEF_FLD_shape: {
        const char *src=ps_sprite_shape_repr(v);
        if (!src) return -1;
        return ps_strcpy(dst,dsta,src,-1);
      }
    case PS_SPRDEF_FLD_type: return -1; // Type is special, it's not an integer.
    case PS_SPRDEF_FLD_tileid: return ps_hexuint_repr(dst,dsta,v);
    case PS_SPRDEF_FLD_impassable: return ps_enum_repr_multiple(dst,dsta,v,0,ps_blueprint_cell_repr);
    default: return ps_decsint_repr(dst,dsta,v);
  }
  return -1;
}

/* Which keys are extra?
 */

static int ps_sprdef_k_is_extra(int k) {
  switch (k) {
    case PS_SPRDEF_FLD_type:
    case PS_SPRDEF_FLD_tileid:
      return 0;
  }
  return 1;
}

/* Split line into key and value.
 */

static int ps_sprdef_split_line(int *kp,int *kc,int *vp,int *vc,const char *src,int srcc) {
  int srcp=0;
  while ((srcc>0)&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  *kp=srcp;
  *kc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; (*kc)++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  *vp=srcp;
  *vc=srcc-srcp;
  return 0;
}

/* Count extra fields in encoded sprdef.
 * We have to do this before allocating the object.
 */

static int ps_sprdef_count_extra_fields_in_text(const char *src,int srcc) {
  int fldc=0;
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)>0) {
    int kp,kc,vp,vc;
    if (ps_sprdef_split_line(&kp,&kc,&vp,&vc,reader.line,reader.linec)<0) return -1;
    if (!kc) continue;
    int k=ps_sprdef_fld_k_eval(reader.line+kp,kc);
    if (ps_sprdef_k_is_extra(k)) fldc++;
  }
  return fldc;
}

/* Sort extra fields in place.
 */

static int ps_sprdef_sort_fields(struct ps_sprdef *sprdef) {

  int lo=0,hi=sprdef->fldc-1,d=1;
  while (lo<hi) {
    int first,last,i,done=1;
    if (d>0) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      int cmp;
           if (sprdef->fldv[i].k<sprdef->fldv[i+d].k) cmp=-1;
      else if (sprdef->fldv[i].k>sprdef->fldv[i+d].k) cmp=1;
      else return -1; // Duplicates not permitted.
      if (cmp==d) {
        done=0;
        struct ps_sprdef_fld tmp=sprdef->fldv[i];
        sprdef->fldv[i]=sprdef->fldv[i+d];
        sprdef->fldv[i+d]=tmp;
      }
    }
    if (done) break;
    if (d==1) { d=-1; hi--; }
    else { d=1; lo++; }
  }
  return 0;
}

/* Decode sprdef.
 */

static int ps_sprdef_decode_to_object(struct ps_sprdef *sprdef,const char *src,int srcc) {
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  int fldc=0;
  while (ps_line_reader_next(&reader)>0) {
    int kp,kc,vp,vc;
    if (ps_sprdef_split_line(&kp,&kc,&vp,&vc,reader.line,reader.linec)<0) return -1;
    if (!kc) continue;
    int k=ps_sprdef_fld_k_eval(reader.line+kp,kc);
    if (k<0) {
      ps_log(RES,ERROR,"%d: Unknown sprdef key '%.*s'",reader.lineno,kc,reader.line+kp);
      return -1;
    }
    switch (k) {
    
      case PS_SPRDEF_FLD_type: {
          const struct ps_sprtype *type=ps_sprtype_by_name(reader.line+vp,vc);
          if (!type) {
            ps_log(RES,ERROR,"%d: Unknown sprite type '%.*s'.",reader.lineno,vc,reader.line+vp);
            return -1;
          }
          sprdef->type=type;
        } break;
        
      case PS_SPRDEF_FLD_tileid: {
          int v;
          if (ps_sprdef_fld_v_eval(&v,k,reader.line+vp,vc)<0) {
            ps_log(RES,ERROR,"%d: Failed to evaluate '%.*s' for sprdef field %d.",reader.lineno,vc,reader.line+vp,k);
            return -1;
          }
          if ((v<0)||(v>0xffff)) {
            ps_log(RES,ERROR,"%d: Tile ID must be in 0..65535.",reader.lineno);
            return -1;
          }
          sprdef->tileid=v;
        } break;
        
      default: {
          if (fldc>=sprdef->fldc) {
            ps_log(RES,ERROR,"Sprdef generic field miscount.");
            return -1;
          }
          int v;
          if (ps_sprdef_fld_v_eval(&v,k,reader.line+vp,vc)<0) {
            ps_log(RES,ERROR,"%d: Failed to evaluate '%.*s' for sprdef field %d.",reader.lineno,vc,reader.line+vp,k);
            return -1;
          }
          sprdef->fldv[fldc].k=k;
          sprdef->fldv[fldc].v=v;
          fldc++;
        }
        
    }
  }
  if (fldc<sprdef->fldc) {
    ps_log(RES,ERROR,"Sprdef generic field miscount.");
    return -1;
  }
  if (!sprdef->type) {
    ps_log(RES,ERROR,"Sprdef must declare a type.");
    return -1;
  }
  if (ps_sprdef_sort_fields(sprdef)<0) return -1;
  return 0;
}
 
struct ps_sprdef *ps_sprdef_decode(const void *src,int srcc) {
  int fldc=ps_sprdef_count_extra_fields_in_text(src,srcc);
  if (fldc<0) return 0;
  struct ps_sprdef *sprdef=ps_sprdef_new(fldc);
  if (!sprdef) return 0;
  if (ps_sprdef_decode_to_object(sprdef,src,srcc)<0) {
    ps_sprdef_del(sprdef);
    return 0;
  }
  return sprdef;
}

/* Encode sprdef.
 */

static int ps_sprdef_encode_to_buffer(struct ps_buffer *buffer,const struct ps_sprdef *sprdef) {

  /* Two built-in fields. */
  if (sprdef->type) {
    if (ps_buffer_appendf(buffer,"type %s\n",sprdef->type->name)<0) return -1;
  }
  if (ps_buffer_appendf(buffer,"tileid 0x%04x\n",sprdef->tileid)<0) return -1;

  /* Everything else is generic. */
  const struct ps_sprdef_fld *fld=sprdef->fldv;
  int i=sprdef->fldc; for (;i-->0;fld++) {
    const char *k=ps_sprdef_fld_k_repr(fld->k);
    if (!k) return -1;
    if (ps_buffer_append(buffer,k,-1)<0) return -1;
    if (ps_buffer_append(buffer," ",1)<0) return -1;
    while (1) {
      int addc=ps_sprdef_fld_v_repr(buffer->v+buffer->c,buffer->a-buffer->c,fld->k,fld->v);
      if (addc<0) return -1;
      if (buffer->c<=buffer->a-addc) {
        buffer->c+=addc;
        break;
      }
      if (ps_buffer_require(buffer,addc)<0) return -1;
    }
    if (ps_buffer_append(buffer,"\n",1)<0) return -1;
  }

  return 0;
}
 
int ps_sprdef_encode(void *dstpp,const struct ps_sprdef *sprdef) {
  if (!dstpp||!sprdef) return -1;
  struct ps_buffer buffer={0};
  if (ps_sprdef_encode_to_buffer(&buffer,sprdef)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  *(void**)dstpp=buffer.v;
  return buffer.c;
}
