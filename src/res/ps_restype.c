#include "ps_res_internal.h"

/* Cleanup.
 */

void ps_restype_cleanup(struct ps_restype *type) {
  if (!type) return;

  if (type->resv) {
    if (type->del) {
      while (type->resc-->0) {
        struct ps_res *res=type->resv+type->resc;
        if (res->obj) type->del(res->obj);
      }
    }
    free(type->resv);
  }
  
  memset(type,0,sizeof(struct ps_restype));
}

/* Clear.
 */

int ps_restype_clear(struct ps_restype *type) {
  if (!type) return -1;

  if (type->del) {
    while (type->resc>0) {
      type->resc--;
      struct ps_res *res=type->resv+type->resc;
      if (res->obj) type->del(res->obj);
    }
  }

  type->resc=0;
  type->rescontigc=0;
  
  return 0;
}

/* Decode.
 */
 
int ps_restype_decode(struct ps_restype *type,int rid,const void *src,int srcc,const char *refpath) {
  if (!type) return -1;
  if (!type->decode) return -1;
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (!refpath) refpath="<unknown>";

  int p=ps_restype_res_search(type,rid);
  if (p>=0) {
    ps_log(RES,ERROR,"%s: Duplicate resource %s:%d.",refpath,type->name,rid);
    return -1;
  }
  p=-p-1;

  void *obj=0;
  if (type->decode(&obj,src,srcc,rid,refpath)<0) {
    ps_log(RES,ERROR,"%s: Failed to decode resource %s:%d.",refpath,type->name,rid);
    return -1;
  }
  if (!obj) {
    ps_log(RES,ERROR,"%s: Decoder for resource %s:%d returned null.",refpath,type->name,rid);
    return -1;
  }

  if (ps_restype_res_insert(type,p,rid,obj)<0) {
    if (type->del) type->del(obj);
    return -1;
  }
  
  return 0;
}

/* Search resources.
 */
 
int ps_restype_res_search(const struct ps_restype *type,int id) {
  if (!type) return -1;
  if (id<0) return -1;
  if (type->resc<1) return -1;
  if (id<type->rescontigc) return id; // Reasonably likely, if the data are so arranged.
  if (id>type->resv[type->resc-1].id) return -type->resc-1; // Very likely.
  int lo=type->rescontigc,hi=type->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (id<type->resv[ck].id) hi=ck;
    else if (id>type->resv[ck].id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ps_restype_index_by_object(const struct ps_restype *type,const void *obj) {
  if (!type) return -1;
  const struct ps_res *res=type->resv;
  int i=0; for (;i<type->resc;i++,res++) {
    if (res->obj==obj) return i;
  }
  return -1;
}

/* Insert resource.
 */

int ps_restype_res_insert(struct ps_restype *type,int p,int id,void *obj_HANDOFF) {
  if (!type) return -1;
  if ((p<0)||(p>type->resc)) {
    ps_log(RES,ERROR,"%s ID must be in 0..%d (found %d).",type->name,type->rid_limit,id);
    return -1;
  }
  if (id>type->rid_limit) return -1;
  if (p&&(id<=type->resv[p-1].id)) return -1;
  if ((p<type->resc)&&(id>=type->resv[p].id)) return -1;
  if (!obj_HANDOFF) return -1;

  if (type->resc>=type->resa) {
    int na=type->resa+32;
    if (na>INT_MAX/sizeof(struct ps_res)) return -1;
    void *nv=realloc(type->resv,sizeof(struct ps_res)*na);
    if (!nv) return -1;
    type->resv=nv;
    type->resa=na;
  }

  struct ps_res *res=type->resv+p;
  memmove(res+1,res,sizeof(struct ps_res)*(type->resc-p));
  type->resc++;

  res->id=id;
  res->dirty=0;
  res->obj=obj_HANDOFF;

  while ((type->rescontigc<type->resc)&&(type->resv[type->rescontigc].id==type->rescontigc)) type->rescontigc++;

  return 0;
}

/* Link all resources.
 */
 
int ps_restype_link(struct ps_restype *restype) {
  if (!restype) return -1;
  if (!restype->link) return 0; // Link not necessary.
  const struct ps_res *res=restype->resv;
  int i=restype->resc; for (;i-->0;res++) {
    if (restype->link(res->obj)<0) {
      ps_log(RES,ERROR,"Failed to link %s:%d.",restype->name,res->id);
      return -1;
    }
  }
  return 0;
}
