#include "ps_input_internal.h"
#include "util/ps_text.h"

/* Cleanup.
 */

void ps_maptm_cleanup(struct ps_maptm *maptm) {
  if (!maptm) return;

  if (maptm->namepattern) free(maptm->namepattern);
  if (maptm->fldv) free(maptm->fldv);

  free(maptm);
}

/* Set name pattern.
 */
 
int ps_maptm_set_namepattern(struct ps_maptm *maptm,const char *src,int srcc) {
  if (!maptm) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>1024) return -1; // Arbitrary sanity limit.
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

/* Search fields.
 */
 
int ps_maptm_search_srcbtnid(const struct ps_maptm *maptm,int srcbtnid) {
  if (!maptm) return -1;
  int lo=0,hi=maptm->fldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<maptm->fldv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>maptm->fldv[ck].srcbtnid) lo=ck+1;
    else {
      while ((ck>lo)&&(maptm->fldv[ck-1].srcbtnid==srcbtnid)) ck--;
      return ck;
    }
  }
  return -lo-1;
}

/* Insert field.
 */
 
int ps_maptm_insert_fld(struct ps_maptm *maptm,int p,int srcbtnid,int srclo,int srchi,int dstbtnid) {
  if (!maptm) return -1;
  if (srcbtnid<1) return -1;
  if (!dstbtnid) return -1;

  if ((p<0)||(p>maptm->fldc)) {
    p=ps_maptm_search_srcbtnid(maptm,srcbtnid);
    if (p<0) {
      p=-p-1;
    } else {
      while ((p<maptm->fldc)&&(maptm->fldv[p].srcbtnid==srcbtnid)) p++;
    }
  } else {
    if ((p>0)&&(maptm->fldv[p-1].srcbtnid>srcbtnid)) return -1;
    if ((p<maptm->fldc&&(maptm->fldv[p].srcbtnid<srcbtnid)) return -1;
  }

  if (maptm->fldc>=maptm->flda) {
    int na=maptm->flda+16;
    if (na>INT_MAX/sizeof(struct ps_maptm_fld)) return -1;
    void *nv=realloc(maptm->fldv,sizeof(struct ps_maptm_fld)*na);
    if (!nv) return -1;
    maptm->fldv=nv;
    maptm->flda=na;
  }

  struct ps_maptm_fld *fld=maptm->fldv+p;
  memmove(fld+1,fld,sizeof(struct ps_maptm_fld)*(maptm->fldc-p));
  maptm->fldc++;

  fld->srcbtnid=srcbtnid;
  if (srclo<=srchi) {
    fld->srclo=srclo;
    fld->srchi=srchi;
  } else {
    fld->srclo=srchi;
    fld->srchi=srclo;
  }
  fld->dstbtnid=dstbtnid;

  return 0;
}

/* Match device.
 */
 
int ps_maptm_match_device(const struct ps_maptm *maptm,const char *name,int namec,uint16_t vendorid,uint16_t deviceid,int provider) {
  if (!maptm) return 0;

  int score=ps_pattern_match(maptm->namepattern,-1,name,namec);
  if (score<1) return 0;

  #define ADDSCORE(src) { \
    int _src=(src); \
    if (score>=INT_MAX-_src) return INT_MAX; \
    score+=_src; \
  }

  if (maptm->vendorid) {
    if (maptm->vendorid!=vendorid) return 0;
    ADDSCORE(10)
  }
  if (maptm->deviceid) {
    if (maptm->deviceid!=deviceid) return 0;
    ADDSCORE(10)
  }
  if (maptm->provider) {
  	  if (maptm->provider!=provider) return 0;
  	  ADDSCORE(10)
  	}

  #undef ADDSCORE
  return score;
}
