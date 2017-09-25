#include "ps.h"
#include "ps_sprite.h"
#include "ps_game.h"

/* Object lifecycle.
 */

struct ps_sprgrp *ps_sprgrp_new() {
  struct ps_sprgrp *grp=calloc(1,sizeof(struct ps_sprgrp));
  if (!grp) return 0;
  grp->refc=1;
  return grp;
}

void ps_sprgrp_cleanup(struct ps_sprgrp *grp) {
  if (!grp) return;
  ps_sprgrp_clear(grp);
  if (grp->sprv) free(grp->sprv);
  memset(grp,0,sizeof(struct ps_sprgrp));
}

void ps_sprgrp_del(struct ps_sprgrp *grp) {
  if (!grp) return;
  if (!grp->refc) return;
  if (grp->refc-->1) return;
  ps_sprgrp_cleanup(grp);
  free(grp);
}

int ps_sprgrp_ref(struct ps_sprgrp *grp) {
  if (!grp) return -1;
  if (!grp->refc) return 0;
  if (grp->refc<0) return -1;
  if (grp->refc==INT_MAX) return -1;
  grp->refc++;
  return 0;
}

/* Primitive search (private).
 */

static int _ps_sprite_search(const struct ps_sprite *spr,const struct ps_sprgrp *grp) {
  int lo=0,hi=spr->grpc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (grp<spr->grpv[ck]) hi=ck;
    else if (grp>spr->grpv[ck]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int _ps_sprgrp_search(const struct ps_sprgrp *grp,const struct ps_sprite *spr) {
  switch (grp->order) {

    case PS_SPRGRP_ORDER_ADDR: {
        int lo=0,hi=grp->sprc;
        while (lo<hi) {
          int ck=(lo+hi)>>1;
               if (spr<grp->sprv[ck]) hi=ck;
          else if (spr>grp->sprv[ck]) lo=ck+1;
          else return ck;
        }
        return -lo-1;
      }

    case PS_SPRGRP_ORDER_RENDER: {
        // We could try a binary search first, but I'm not sure it buys us much.
        int insp=0,i=0;
        for (;i<grp->sprc;i++) {
          struct ps_sprite *ckspr=grp->sprv[i];
          if (ckspr==spr) return i;
          if (ckspr->layer<spr->layer) insp=i+1;
          else if ((ckspr->layer==spr->layer)&&(ckspr->y<=spr->y)) insp=i+1;
        }
        return -insp-1;
      }

    default: {
        int i=grp->sprc; while (i-->0) if (grp->sprv[i]==spr) return i;
        return -grp->sprc-1;
      }
  }
}

/* Primitive insertion, private.
 */

static int _ps_sprite_add(struct ps_sprite *spr,int p,struct ps_sprgrp *grp) {
  if (spr->grpc>=spr->grpa) {
    int na=spr->grpa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(spr->grpv,sizeof(void*)*na);
    if (!nv) return -1;
    spr->grpv=nv;
    spr->grpa=na;
  }
  if (ps_sprgrp_ref(grp)<0) return -1;
  memmove(spr->grpv+p+1,spr->grpv+p,sizeof(void*)*(spr->grpc-p));
  spr->grpv[p]=grp;
  spr->grpc++;
  return 0;
}

static int _ps_sprgrp_add(struct ps_sprgrp *grp,int p,struct ps_sprite *spr) {
  if (grp->sprc>=grp->spra) {
    int na=grp->spra+32;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(grp->sprv,sizeof(void*)*na);
    if (!nv) return -1;
    grp->sprv=nv;
    grp->spra=na;
  }
  if (ps_sprite_ref(spr)<0) return -1;
  memmove(grp->sprv+p+1,grp->sprv+p,sizeof(void*)*(grp->sprc-p));
  grp->sprv[p]=spr;
  grp->sprc++;
  return 0;
}

/* Primitive removal, private.
 */

static void _ps_sprite_remove(struct ps_sprite *spr,int p) {
  struct ps_sprgrp *grp=spr->grpv[p];
  spr->grpc--;
  memmove(spr->grpv+p,spr->grpv+p+1,sizeof(void*)*(spr->grpc-p));
  ps_sprgrp_del(grp);
}

static void _ps_sprgrp_remove(struct ps_sprgrp *grp,int p) {
  struct ps_sprite *spr=grp->sprv[p];
  grp->sprc--;
  memmove(grp->sprv+p,grp->sprv+p+1,sizeof(void*)*(grp->sprc-p));
  ps_sprite_del(spr);
}

/* Group/sprite connections, public primitives.
 */

int ps_sprgrp_has_sprite(const struct ps_sprgrp *grp,const struct ps_sprite *spr) {
  if (!grp||!spr) return 0;
  /* Sprite's group list is always sorted and tends to be smaller than groups' sprite lists. */
  return (_ps_sprite_search(spr,grp)>=0)?1:0;
}
  
int ps_sprgrp_add_sprite(struct ps_sprgrp *grp,struct ps_sprite *spr) {
  if (!grp||!spr) return -1;

  int grpp=_ps_sprite_search(spr,grp);
  if (grpp>=0) return 0; // Redundant.
  int sprp=_ps_sprgrp_search(grp,spr);
  if (sprp>=0) return -1; // Panic! Inconsistent lists.
  grpp=-grpp-1;
  sprp=-sprp-1;

  if (_ps_sprite_add(spr,grpp,grp)<0) return -1;
  if (_ps_sprgrp_add(grp,sprp,spr)<0) { _ps_sprite_remove(spr,grpp); return -1; }

  return 1;
}
  
int ps_sprgrp_remove_sprite(struct ps_sprgrp *grp,struct ps_sprite *spr) {
  if (!grp||!spr) return -1;

  int grpp=_ps_sprite_search(spr,grp);
  if (grpp<0) return 0; // Redundant.
  int sprp=_ps_sprgrp_search(grp,spr);
  if (sprp<0) return -1; // Panic! Inconsistent lists.

  if (ps_sprgrp_ref(grp)<0) return -1;
  _ps_sprite_remove(spr,grpp);
  _ps_sprgrp_remove(grp,sprp);
  ps_sprgrp_del(grp);

  return 1;
}

/* Remove all groups from sprite.
 */

int ps_sprite_kill(struct ps_sprite *spr) {
  if (!spr) return -1;
  if (spr->grpc<1) return 0;
  if (ps_sprite_ref(spr)<0) return -1;
  while (spr->grpc>0) {
    spr->grpc--;
    struct ps_sprgrp *grp=spr->grpv[spr->grpc];
    int sprp=_ps_sprgrp_search(grp,spr);
    if (sprp>=0) _ps_sprgrp_remove(grp,sprp);
    ps_sprgrp_del(grp);
  }
  ps_sprite_del(spr);
  return 0;
}

/* Remove all sprites from group.
 */
 
int ps_sprgrp_clear(struct ps_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<1) return 0;
  if (ps_sprgrp_ref(grp)<0) return -1;
  while (grp->sprc>0) {
    grp->sprc--;
    struct ps_sprite *spr=grp->sprv[grp->sprc];
    int grpp=_ps_sprite_search(spr,grp);
    if (grpp>=0) _ps_sprite_remove(spr,grpp);
    ps_sprite_del(spr);
  }
  ps_sprgrp_del(grp);
  return 0;
}

/* Kill all sprites in group.
 */

int ps_sprgrp_kill(struct ps_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<1) return 0;
  if (ps_sprgrp_ref(grp)<0) return -1;
  while (grp->sprc>0) {
    grp->sprc--;
    struct ps_sprite *spr=grp->sprv[grp->sprc];
    ps_sprite_kill(spr);
    ps_sprite_del(spr);
  }
  ps_sprgrp_del(grp);
  return 0;
}

/* Join all groups matching reference.
 */
 
int ps_sprite_join_all(struct ps_sprite *spr,struct ps_sprite *ref) {
  if (!spr||!ref) return -1;
  if (spr==ref) return 0;
  int i=ref->grpc; while (i-->0) {
    if (ps_sprgrp_add_sprite(ref->grpv[i],spr)<0) return -1;
  }
  return 0;
}

/* Add to DEATHROW list.
 */
 
int ps_sprite_kill_later(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||!game) return -1;
  return ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_DEATHROW,spr);
}

/* Compare sprites for rendering.
 */

static int ps_sprite_rendercmp(const struct ps_sprite *a,const struct ps_sprite *b) {
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}

/* Sort sprites.
 */

int ps_sprgrp_sort(struct ps_sprgrp *grp,int completely) {
  if (!grp) return -1;
  if (grp->sprc<2) return 0;
  switch (grp->order) {
  
    case PS_SPRGRP_ORDER_RENDER: {
        int lo=0,hi=grp->sprc-1;
        while (lo<hi) {
          int first,last,i,done=1;
          if (grp->sortd==1) { first=lo; last=hi; }
          else { grp->sortd=-1; first=hi; last=lo; }
          for (i=first;i!=last;i+=grp->sortd) {
            int cmp=ps_sprite_rendercmp(grp->sprv[i],grp->sprv[i+grp->sortd]);
            if (cmp==grp->sortd) {
              done=0;
              struct ps_sprite *tmp=grp->sprv[i];
              grp->sprv[i]=grp->sprv[i+grp->sortd];
              grp->sprv[i+grp->sortd]=tmp;
            }
          }
          if (grp->sortd==1) { grp->sortd=-1; hi--; }
          else { grp->sortd=1; lo++; }
          if (!completely) break;
          if (done) break;
        }
      } break;
      
  }
  return 0;
}
