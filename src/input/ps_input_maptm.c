#include "ps.h"
#include "ps_input_maptm.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"
#include "ps_input_map.h"
#include "ps_input.h"
#include "util/ps_text.h"

/* Object lifecycle.
 */

struct ps_input_maptm *ps_input_maptm_new() {
  struct ps_input_maptm *maptm=calloc(1,sizeof(struct ps_input_maptm));
  if (!maptm) return 0;

  return maptm;
}

void ps_input_maptm_del(struct ps_input_maptm *maptm) {
  if (!maptm) return;

  if (maptm->namepattern) free(maptm->namepattern);
  if (maptm->fldv) free(maptm->fldv);

  free(maptm);
}

/* Set name pattern.
 */

int ps_input_maptm_set_namepattern(struct ps_input_maptm *maptm,const char *src,int srcc) {
  if (!maptm) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>1024) return -1;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (maptm->namepattern) free(maptm->namepattern);
  maptm->namepattern=nv;
  maptm->namepatternc=srcc;
  return 0;
}

/* Match device.
 */
 
int ps_input_maptm_match_device(const struct ps_input_maptm *maptm,const struct ps_input_device *device) {
  if (!maptm||!device) return 0;
  
  int score;
  if (maptm->namepatternc) {
    if ((score=ps_pattern_match(maptm->namepattern,maptm->namepatternc,device->name,device->namec))<1) return 0;
  } else {
    score=1;
  }
  
  if (maptm->providerid) {
    if (maptm->providerid!=device->providerid) return 0;
    if (score<INT_MAX) score++;
  }

  if (maptm->vendorid) {
    if (maptm->vendorid!=device->vendorid) return 0;
    if (score<INT_MAX) score++;
  }

  if (maptm->deviceid) {
    if (maptm->deviceid!=device->deviceid) return 0;
    if (score<INT_MAX) score++;
  }

  //TODO Should we consider the available button maps here too?

  return score;
}

/* Set (srclo,srchi) for live map field.
 */

static int ps_input_maptm_set_range(struct ps_input_map_fld *dstfld,const struct ps_input_maptm_fld *srcfld,int lo,int hi) {
  if ((srcfld->srclo==0)&&(srcfld->srchi<0)) {
    if (!lo&&!hi) switch (srcfld->srchi) { // Default assignments for auto range.
      case PS_INPUT_MAPTM_AUTO_TOPHALF: dstfld->srclo=1; dstfld->srchi=INT_MAX; return 0;
      case PS_INPUT_MAPTM_AUTO_BOTTOMHALF: dstfld->srclo=INT_MIN; dstfld->srchi=0; return 0;
      case PS_INPUT_MAPTM_AUTO_TOPTHIRD: dstfld->srclo=1; dstfld->srchi=INT_MAX; return 0;
      case PS_INPUT_MAPTM_AUTO_MIDDLETHIRD: dstfld->srclo=0; dstfld->srchi=0; return 0;
      case PS_INPUT_MAPTM_AUTO_BOTTOMTHIRD: dstfld->srclo=INT_MIN; dstfld->srchi=-1; return 0;
    }
    if (hi==lo+1) switch (srcfld->srchi) { // Auto range for 2-state source (avoid rounding errors).
      case PS_INPUT_MAPTM_AUTO_TOPHALF: dstfld->srclo=hi; dstfld->srchi=hi; return 0;
      case PS_INPUT_MAPTM_AUTO_BOTTOMHALF: dstfld->srclo=lo; dstfld->srchi=lo; return 0;
      case PS_INPUT_MAPTM_AUTO_TOPTHIRD: dstfld->srclo=hi; dstfld->srchi=hi; return 0;
      case PS_INPUT_MAPTM_AUTO_MIDDLETHIRD: dstfld->srclo=lo; dstfld->srchi=hi; return 0; // No sensible behavior for this.
      case PS_INPUT_MAPTM_AUTO_BOTTOMTHIRD: dstfld->srclo=lo; dstfld->srchi=lo; return 0;
    }
    switch (srcfld->srchi) { // Auto range with valid source range.
      case PS_INPUT_MAPTM_AUTO_TOPHALF: dstfld->srclo=lo+((hi-lo)>>1); dstfld->srchi=hi; return 0;
      case PS_INPUT_MAPTM_AUTO_BOTTOMHALF: dstfld->srclo=lo; dstfld->srchi=lo+((hi-lo)>>1); return 0;
      case PS_INPUT_MAPTM_AUTO_TOPTHIRD: if ((lo<0)&&(hi>0)) {
          dstfld->srclo=hi>>1; dstfld->srchi=hi;
        } else {
          dstfld->srclo=lo+(((hi-lo)*2)/3); dstfld->srchi=hi; 
        } return 0;
      case PS_INPUT_MAPTM_AUTO_MIDDLETHIRD: if ((lo<0)&&(hi>0)) {
          dstfld->srclo=lo>>1; dstfld->srchi=hi>>1;
        } else {
          dstfld->srclo=lo+((hi-lo)/3); dstfld->srchi=lo+(((hi-lo)*2)/3);
        } return 0;
      case PS_INPUT_MAPTM_AUTO_BOTTOMTHIRD: if ((lo<0)&&(hi>0)) {
          dstfld->srclo=lo; dstfld->srchi=lo>>1;
        } else {
          dstfld->srclo=lo; dstfld->srchi=lo+((hi-lo)/3);
        } return 0;
    }
  }
  dstfld->srclo=srcfld->srclo;
  dstfld->srchi=srcfld->srchi;
  return 0;
}

/* Apply maps based on default_usage.
 */

static int ps_input_maptm_apply_default(struct ps_input_map *map,const struct ps_input_btncfg *btncfg) {
  if (btncfg->lo>=btncfg->hi) return 0;
  switch (btncfg->default_usage) {

    /* HORZ and VERT ensure that source range at least 3, then create 2 maps. */
    case PS_PLRBTN_HORZ:
    case PS_PLRBTN_VERT: {
        if (btncfg->lo>btncfg->hi-2) return 0;
        struct ps_input_map_fld *fldlo=ps_input_map_insert(map,-1,btncfg->srcbtnid);
        struct ps_input_map_fld *fldhi=ps_input_map_insert(map,-1,btncfg->srcbtnid);
        if (!fldlo||!fldhi) return -1;
        fldlo->srcv=btncfg->value;
        fldhi->srcv=btncfg->value;
        fldlo->srclo=btncfg->lo;
        fldhi->srchi=btncfg->hi;
        if (btncfg->lo==btncfg->hi-2) {
          fldlo->srchi=fldlo->srclo;
          fldhi->srclo=fldhi->srchi;
        } else {
          fldlo->srchi=(btncfg->lo+btncfg->hi)>>1;
          fldhi->srclo=((btncfg->lo+btncfg->hi)*2)>>1;
        }
      } return 0;

    /* THUMB creates A for even-numbered buttons and B for odd. */
    case PS_PLRBTN_THUMB: {
        struct ps_input_map_fld *fld=ps_input_map_insert(map,-1,btncfg->srcbtnid);
        if (!fld) return -1;
        fld->dstbtnid=((btncfg->srcbtnid&1)?PS_PLRBTN_B:PS_PLRBTN_A);
        fld->srclo=(btncfg->lo<btncfg->hi-1)?((btncfg->lo+btncfg->hi)>>1):btncfg->hi;
        fld->srchi=btncfg->hi;
        fld->srcv=btncfg->value;
      } return 0;

    /* AUX is the same as START. */
    case PS_PLRBTN_AUX: {
        struct ps_input_map_fld *fld=ps_input_map_insert(map,-1,btncfg->srcbtnid);
        if (!fld) return -1;
        fld->dstbtnid=PS_PLRBTN_START;
        fld->srclo=(btncfg->lo<btncfg->hi-1)?((btncfg->lo+btncfg->hi)>>1):btncfg->hi;
        fld->srchi=btncfg->hi;
        fld->srcv=btncfg->value;
      } return 0;
    
  }
  if (btncfg->default_usage) {
    struct ps_input_map_fld *fld=ps_input_map_insert(map,-1,btncfg->srcbtnid);
    if (!fld) return -1;
    fld->dstbtnid=btncfg->default_usage;
    fld->srclo=(btncfg->lo<btncfg->hi-1)?((btncfg->lo+btncfg->hi)>>1):btncfg->hi;
    fld->srchi=btncfg->hi;
    fld->srcv=btncfg->value;
    return 0;
  }
  return 0;
}

/* Populate live map from buttons the device reports as present.
 */

struct ps_input_maptm_apply_from_reported_buttons {
  struct ps_input_map *map;
  const struct ps_input_maptm *maptm;
};

static int ps_input_maptm_apply_from_reported_buttons_cb(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata) {
  struct ps_input_maptm_apply_from_reported_buttons *ctx=userdata;
  ps_log(INPUT,TRACE,"Device '%.*s' reports button 0x%08x (%d..%d) =>%d",device->namec,device->name,btncfg->srcbtnid,btncfg->lo,btncfg->hi,btncfg->default_usage);
  int srcp=ps_input_maptm_fld_search(ctx->maptm,btncfg->srcbtnid);
  
  if (srcp<0) {
    if (!btncfg->default_usage) return 0;
    if (ps_input_maptm_apply_default(ctx->map,btncfg)<0) return -1;
    return 0;
  }

  while ((srcp>0)&&(ctx->maptm->fldv[srcp-1].srcbtnid==btncfg->srcbtnid)) srcp--;
  const struct ps_input_maptm_fld *srcfld=ctx->maptm->fldv+srcp;
  while ((srcp<ctx->maptm->fldc)&&(srcfld->srcbtnid==btncfg->srcbtnid)) {
    struct ps_input_map_fld *dstfld=ps_input_map_insert(ctx->map,-1,srcfld->srcbtnid);
    if (!dstfld) return -1;
    dstfld->dstbtnid=srcfld->dstbtnid;
    ps_input_maptm_set_range(dstfld,srcfld,btncfg->lo,btncfg->hi);
    dstfld->srcv=btncfg->value; // We don't care if it's inside the range, dstv is always zero initially.
    ps_log(INPUT,TRACE,"Mapping button 0x%08x to 0x%08x",dstfld->srcbtnid,dstfld->dstbtnid);
    srcp++;
    srcfld++;
  }
  
  return 0;
}

static int ps_input_maptm_apply_from_reported_buttons(struct ps_input_map *map,const struct ps_input_maptm *maptm,struct ps_input_device *device) {
  struct ps_input_maptm_apply_from_reported_buttons userdata={
    .map=map,
    .maptm=maptm,
  };
  int err=device->report_buttons(device,&userdata,ps_input_maptm_apply_from_reported_buttons_cb);
  if (err<0) return err;
  return 0;
}

/* Populate live map from device that doesn't report capabilities.
 * Copy every mapping.
 */

static int ps_input_maptm_apply_all(struct ps_input_map *map,const struct ps_input_maptm *maptm,struct ps_input_device *device) {
  const struct ps_input_maptm_fld *srcfld=maptm->fldv;
  int i=maptm->fldc; for (;i-->0;srcfld++) {
    struct ps_input_map_fld *dstfld=ps_input_map_insert(map,-1,srcfld->srcbtnid);
    if (!dstfld) return -1;
    dstfld->dstbtnid=srcfld->dstbtnid;
    ps_input_maptm_set_range(dstfld,srcfld,0,0);
  }
  return 0;
}

/* Generate map.
 */
 
struct ps_input_map *ps_input_maptm_apply_for_device(
  const struct ps_input_maptm *maptm,struct ps_input_device *device
) {
  if (!maptm||!device) return 0;
  struct ps_input_map *map=ps_input_map_new();

  if (device->report_buttons) {
    if (ps_input_maptm_apply_from_reported_buttons(map,maptm,device)<0) {
      ps_input_map_del(map);
      return 0;
    }
  } else {
    if (ps_input_maptm_apply_all(map,maptm,device)<0) {
      ps_input_map_del(map);
      return 0;
    }
  }

  return map;
}

/* Search field.
 */

int ps_input_maptm_fld_search(const struct ps_input_maptm *maptm,int srcbtnid) {
  if (!maptm) return -1;
  int lo=0,hi=maptm->fldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<maptm->fldv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>maptm->fldv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert field.
 */
 
struct ps_input_maptm_fld *ps_input_maptm_fld_insert(struct ps_input_maptm *maptm,int p,int srcbtnid) {
  if (!maptm) return 0;

  if ((p<0)||(p>maptm->fldc)) {
    p=ps_input_maptm_fld_search(maptm,srcbtnid);
    if (p<0) p=-p-1;
  } else {
    if (p&&(srcbtnid>maptm->fldv[p-1].srcbtnid)) return 0;
    if ((p<maptm->fldc)&&(srcbtnid<maptm->fldv[p].srcbtnid)) return 0;
  }

  if (maptm->fldc>=maptm->flda) {
    int na=maptm->flda+16;
    if (na>INT_MAX/sizeof(struct ps_input_maptm_fld)) return 0;
    void *nv=realloc(maptm->fldv,sizeof(struct ps_input_maptm_fld)*na);
    if (!nv) return 0;
    maptm->fldv=nv;
    maptm->flda=na;
  }

  struct ps_input_maptm_fld *fld=maptm->fldv+p;
  memmove(fld+1,fld,sizeof(struct ps_input_maptm_fld)*(maptm->fldc-p));
  maptm->fldc++;
  memset(fld,0,sizeof(struct ps_input_maptm_fld));
  fld->srcbtnid=srcbtnid;
  return fld;
}

/* Encode one field.
 */

static int ps_input_maptm_fld_repr(char *dst,int dsta,const struct ps_input_maptm_fld *fld,const struct ps_input_provider *provider) {

  /* srcbtnid: Try provider first, then fall back to plain integer. */
  int dstc=0;
  if (provider&&provider->btnid_repr) {
    dstc=provider->btnid_repr(dst,dsta,fld->srcbtnid);
  }
  if (dstc<1) dstc=ps_decsint_repr(dst,dsta,fld->srcbtnid);

  /* lo,hi: Plain integers. */
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=ps_decsint_repr(dst+dstc,dsta-dstc,fld->srclo);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=ps_decsint_repr(dst+dstc,dsta-dstc,fld->srchi);

  /* dstbtnid: Use universal representer. */
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=ps_btnid_repr(dst+dstc,dsta-dstc,fld->dstbtnid);

  return dstc;
}

/* Encode.
 */

int ps_input_maptm_encode(char *dst,int dsta,const struct ps_input_maptm *maptm) {
  if (!maptm) return -1;
  if (!dst||(dsta<0)) dsta=0;
  int dstc=0;
  dstc+=ps_strcpy(dst+dstc,dsta-dstc,"{\n",2);

  /* Emit any metadata which does not have the default value. */

  if (maptm->namepatternc) {
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"name=",5);
    dstc+=ps_str_repr(dst+dstc,dsta-dstc,maptm->namepattern,maptm->namepatternc);
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"\n",1);
  }

  if (maptm->providerid) {
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"provider=",9);
    dstc+=ps_input_provider_repr(dst+dstc,dsta-dstc,maptm->providerid);
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"\n",1);
  }

  if (maptm->vendorid) {
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"vendor=",7);
    dstc+=ps_hexuint_repr(dst+dstc,dsta-dstc,maptm->vendorid);
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"\n",1);
  }

  if (maptm->deviceid) {
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"device=",7);
    dstc+=ps_hexuint_repr(dst+dstc,dsta-dstc,maptm->deviceid);
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"\n",1);
  }

  /* To encode fields, it is nice if we can ask the provider for button names.
   * If this provider is not installed, we can emit them as integers, which is fine.
   */
  const struct ps_input_provider *provider=ps_input_get_provider_by_id(maptm->providerid);

  /* Emit fields. */
  const struct ps_input_maptm_fld *fld=maptm->fldv;
  int i=maptm->fldc; for (;i-->0;fld++) {
    int err=ps_input_maptm_fld_repr(dst+dstc,dsta-dstc,fld,provider);
    if (err<0) return -1;
    if (dstc>(INT_MAX>>2)-err) return -1;
    dstc+=err;
    dstc+=ps_strcpy(dst+dstc,dsta-dstc,"\n",1);
  }

  dstc+=ps_strcpy(dst+dstc,dsta-dstc,"}\n",2);
  return dstc;
}

/* Decode metadata.
 */

static int ps_input_maptm_decode_name(struct ps_input_maptm *maptm,const char *src,int srcc) {
  char dst[256];
  int dstc=ps_str_eval(dst,sizeof(dst),src,srcc);
  if (dstc<0) {
    ps_log(INPUT,ERROR,"Failed to decode string: %.*s",srcc,src);
    return -1;
  }
  if (dstc>sizeof(dst)) {
    ps_log(INPUT,ERROR,"Device name too long, please limit to 256 bytes.");
    return -1;
  }
  if (ps_input_maptm_set_namepattern(maptm,dst,dstc)<0) return -1;
  return 0;
}

static int ps_input_maptm_decode_provider(struct ps_input_maptm *maptm,const char *src,int srcc) {
  int providerid=ps_input_provider_eval(src,srcc);
  if (providerid<0) {
    ps_log(INPUT,ERROR,"Unknown input provider '%.*s'",srcc,src);
    return -1;
  }
  maptm->providerid=providerid;

  if (!ps_input_get_provider_by_id(maptm->providerid)) {
    ps_log(INPUT,ERROR,"Discarding map template due to uninstalled provider '%.*s'.",srcc,src);
    maptm->invalid_provider=1;
  }
  
  return 0;
}

static int ps_input_maptm_decode_uint16(struct ps_input_maptm *maptm,uint16_t *dst,const char *src,int srcc) {
  int tmp;
  if (ps_int_eval(&tmp,src,srcc)<0) {
    ps_log(INPUT,ERROR,"Failed to evaluate integer: '%.*s'",srcc,src);
    return -1;
  }
  if ((tmp<0)||(tmp>0xffff)) {
    ps_log(INPUT,ERROR,"Integer out of range (%.*s, must be in 0..65535)",srcc,src);
    return -1;
  }
  *dst=tmp;
  return 0;
}

static int ps_input_maptm_decode_metadata(struct ps_input_maptm *maptm,const char *k,int kc,const char *v,int vc) {
  if ((kc==4)&&!memcmp(k,"name",4)) return ps_input_maptm_decode_name(maptm,v,vc);
  if ((kc==8)&&!memcmp(k,"provider",8)) return ps_input_maptm_decode_provider(maptm,v,vc);
  if ((kc==6)&&!memcmp(k,"vendor",6)) return ps_input_maptm_decode_uint16(maptm,&maptm->vendorid,v,vc);
  if ((kc==6)&&!memcmp(k,"device",6)) return ps_input_maptm_decode_uint16(maptm,&maptm->deviceid,v,vc);
  ps_log(INPUT,ERROR,"Unexpected input config key '%.*s'. Valid: name, provider, vendor, device",kc,k);
  return -1;
}

/* Decode mapping.
 */

static int ps_input_maptm_decode_map(
  struct ps_input_maptm *maptm,
  const char *srcbtnname,int srcbtnnamec,
  const char *srclo,int srcloc,
  const char *srchi,int srchic,
  const char *dstbtnname,int dstbtnnamec
) {

  /* If a provider is defined, give it the first crack at (srcbtnname).
   * Note that it is possible to switch providers in the middle of a block.
   * That's crazy but I don't think it would do any real harm.
   */
  int srcbtnid;
  const struct ps_input_provider *provider=0;
  if (maptm->providerid) {
    if (provider=ps_input_get_provider_by_id(maptm->providerid)) {
      if (provider->btnid_eval) {
        if (provider->btnid_eval(&srcbtnid,srcbtnname,srcbtnnamec)>=0) {
          goto _got_srcbtnid_from_provider_;
        }
      }
    }
  }

  /* Default srcbtnid. */
  if (ps_int_eval(&srcbtnid,srcbtnname,srcbtnnamec)<0) {
    ps_log(INPUT,ERROR,"Failed to evaluate input button name '%.*s' from provider %d.",srcbtnnamec,srcbtnname,maptm->providerid);
    return -1;
  }
 _got_srcbtnid_from_provider_:;

  /* Low and high are plain integers, even for auto mapping. */
  int srclon,srchin;
  if (ps_int_eval(&srclon,srclo,srcloc)<0) {
    ps_log(INPUT,ERROR,"Failed to evaluate integer for input map: %.*s",srcloc,srclo);
    return -1;
  }
  if (ps_int_eval(&srchin,srchi,srchic)<0) {
    ps_log(INPUT,ERROR,"Failed to evaluate integer for input map: %.*s",srchic,srchi);
    return -1;
  }

  /* dstbtnid is universal. */
  int dstbtnid=ps_btnid_eval(dstbtnname,dstbtnnamec);
  if (dstbtnid<0) {
    ps_log(INPUT,ERROR,"Failed to evaluate target button name '%.*s' for input map.",dstbtnnamec,dstbtnname);
    return -1;
  }

  /* Insert it. */
  struct ps_input_maptm_fld *fld=ps_input_maptm_fld_insert(maptm,-1,srcbtnid);
  if (!fld) return -1;
  fld->srclo=srclon;
  fld->srchi=srchin;
  fld->dstbtnid=dstbtnid;

  return 0;
}

/* Decode single line.
 */

static int ps_input_maptm_decode_line(struct ps_input_maptm *maptm,const char *src,int srcc) {

  /* Split into tokens. There can't be more than 4. */
  struct ps_maptm_token {
    const char *v;
    int c;
  } tokenv[4];
  int tokenc=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (tokenc>=4) {
      ps_log(INPUT,ERROR,"Too many tokens on input config line: %.*s",srcc,src);
      return -1;
    }
    tokenv[tokenc].v=src+srcp;
    tokenv[tokenc].c=0;
    if (src[srcp]=='"') {
      tokenv[tokenc].c=ps_str_measure(src+srcp,srcc-srcp);
      if (tokenv[tokenc].c<1) {
        ps_log(INPUT,ERROR,"Malformed string token in input config.");
        return -1;
      }
      srcp+=tokenv[tokenc].c;
    } else {
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenv[tokenc].c++; }
    }
    tokenc++;
  }

  /* Require at least two tokens. But zero is OK. */
  if (!tokenc) return 0;
  if (tokenc<2) {
    ps_log(INPUT,ERROR,"Malformed input config line: %.*s",srcc,src);
    return -1;
  }

  /* If the second token is '=', it's metadata. */
  if ((tokenv[1].c==1)&&(tokenv[1].v[0]=='=')) {
    if (tokenc!=3) {
      ps_log(INPUT,ERROR,"Malformed input config line: %.*s",srcc,src);
      return -1;
    }
    if (ps_input_maptm_decode_metadata(maptm,tokenv[0].v,tokenv[0].c,tokenv[2].v,tokenv[2].c)<0) {
      ps_log(INPUT,ERROR,"Failed to process input config: %.*s",srcc,src);
      return -1;
    }

  /* If it's not metadata, it's a button mapping. */
  } else {
    if (tokenc==2) { // Use PS_INPUT_MAPTM_AUTO_TOPHALF if range not provided.
      if (ps_input_maptm_decode_map(maptm,tokenv[0].v,tokenv[0].c,"0",1,"-1",2,tokenv[1].v,tokenv[1].c)<0) {
        ps_log(INPUT,ERROR,"Failed to process input config: %.*s",srcc,src);
        return -1;
      }
    } else if (tokenc==4) {
      if (ps_input_maptm_decode_map(maptm,tokenv[0].v,tokenv[0].c,tokenv[1].v,tokenv[1].c,tokenv[2].v,tokenv[2].c,tokenv[3].v,tokenv[3].c)<0) {
        ps_log(INPUT,ERROR,"Failed to process input config: %.*s",srcc,src);
        return -1;
      }
    } else {
      ps_log(INPUT,ERROR,"Malformed input config line: %.*s",srcc,src);
      return -1;
    }
  }
  
  return 0;
}

/* Decode.
 */

int ps_input_maptm_decode(struct ps_input_maptm *maptm,const char *src,int srcc) {
  if (!maptm) return -1;

  /* Must begin with '{' on its own line. */
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  if (ps_line_reader_next(&reader)<1) return -1;
  if ((reader.linec!=1)||(reader.line[0]!='{')) return -1;

  while (ps_line_reader_next(&reader)>0) {

    /* '}' on its own line terminates the block. */
    if ((reader.linec==1)&&(reader.line[0]=='}')) {
      return reader.srcp;
    }

    /* If we hit a declaration of uninstalled provider, continue reading to the end but don't evaluate anything.
     * This is important because we might not be able to decode button names from the missing provider.
     */
    if (!maptm->invalid_provider) {
      if (ps_input_maptm_decode_line(maptm,reader.line,reader.linec)<0) return -1;
    }
    
  }
  return -1; // Unterminated.
}
