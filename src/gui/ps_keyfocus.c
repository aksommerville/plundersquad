#include "ps.h"
#include "ps_widget.h"
#include "ps_keyfocus.h"

extern const struct ps_widget_type ps_widget_type_root;

/* Object lifecycle.
 */
 
struct ps_keyfocus *ps_keyfocus_new() {
  struct ps_keyfocus *keyfocus=calloc(1,sizeof(struct ps_keyfocus));
  if (!keyfocus) return 0;

  keyfocus->refc=1;
  keyfocus->p=-1;

  return keyfocus;
}

void ps_keyfocus_del(struct ps_keyfocus *keyfocus) {
  if (!keyfocus) return;
  if (keyfocus->refc-->1) return;

  if (keyfocus->v) {
    while (keyfocus->c-->0) {
      ps_widget_del(keyfocus->v[keyfocus->c]);
    }
    free(keyfocus->v);
  }

  free(keyfocus);
}

int ps_keyfocus_ref(struct ps_keyfocus *keyfocus) {
  if (!keyfocus) return -1;
  if (keyfocus->refc<1) return -1;
  if (keyfocus->refc==INT_MAX) return -1;
  keyfocus->refc++;
  return 0;
}

/* Grow buffer.
 */

static int ps_keyfocus_require(struct ps_keyfocus *keyfocus) {
  if (keyfocus->c<keyfocus->a) return 0;
  int na=keyfocus->a+8;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(keyfocus->v,sizeof(void*)*na);
  if (!nv) return -1;
  keyfocus->v=nv;
  keyfocus->a=na;
  return 0;
}

/* Test descendence from root.
 */

static int ps_keyfocus_widget_is_orphan(const struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  if (!root) return 1;
  if (root->type==&ps_widget_type_root) return 0;
  return 1;
}

/* Forcibly remove a widget from the ring.
 */

static int ps_keyfocus_remove(struct ps_keyfocus *keyfocus,struct ps_widget *widget) {
  int i=keyfocus->c; while (i-->0) {
    if (keyfocus->v[i]==widget) {
      keyfocus->c--;
      memmove(keyfocus->v+i,keyfocus->v+i+1,sizeof(void*)*(keyfocus->c-i));
      ps_widget_del(widget);
      if (keyfocus->p==i) keyfocus->p=-1;
      else if (keyfocus->p>i) keyfocus->p--;
      return 1;
    }
  }
  return 0;
}

/* Fire callbacks.
 */

static int ps_keyfocus_farewell(struct ps_keyfocus *keyfocus,struct ps_widget *focus) {
  if (ps_keyfocus_widget_is_orphan(focus)) return 0;
  return ps_widget_unfocus(focus);
}

static int ps_keyfocus_hello(struct ps_keyfocus *keyfocus,struct ps_widget *focus) {
  return ps_widget_focus(focus);
}

/* Change the current focus.
 * Fires callbacks as warranted.
 */

static int ps_keyfocus_replace(struct ps_keyfocus *keyfocus,int np) {
  if (!keyfocus) return -1;
  if ((np<0)||(np>=keyfocus->c)) np=-1;
  if (np==keyfocus->p) return 0;

  if (keyfocus->p>=0) {
    if (ps_keyfocus_farewell(keyfocus,keyfocus->v[keyfocus->p])<0) return -1;
    keyfocus->p=-1;
  }

  if (np>=0) {
    struct ps_widget *nfocus=keyfocus->v[np];
    if (ps_keyfocus_widget_is_orphan(nfocus)) {
      ps_keyfocus_remove(keyfocus,nfocus);
      np=-1;
    } else {
      if (ps_keyfocus_hello(keyfocus,nfocus)<0) return -1;
    }
  }
  keyfocus->p=np;
  
  return 0;
}

/* Test eligibility.
 */

static int ps_keyfocus_widget_is_eligible(const struct ps_widget *candidate,const struct ps_widget *container) {
  if (!candidate) return 0;
  if (!candidate->accept_keyboard_focus) return 0;
  if (!container) return 0;
  if (!ps_widget_is_ancestor(container,candidate)) return 0;
  return 1;
}

/* Scan recursively for focusable widgets.
 */

static int ps_keyfocus_add_if_absent(struct ps_keyfocus *keyfocus,struct ps_widget *widget) {
  int i=keyfocus->c; while (i-->0) {
    if (keyfocus->v[i]==widget) return 0;
  }
  if (ps_keyfocus_require(keyfocus)<0) return -1;
  if (ps_widget_ref(widget)<0) return -1;
  keyfocus->v[keyfocus->c++]=widget;
  return 0;
}

static int ps_keyfocus_scan(struct ps_keyfocus *keyfocus,struct ps_widget *container) {
  if (!container) return 0;
  if (container->accept_keyboard_focus) {
    if (ps_keyfocus_add_if_absent(keyfocus,container)<0) return -1;
  }
  int i=0; for (;i<container->childc;i++) {
    if (ps_keyfocus_scan(keyfocus,container->childv[i])<0) return -1;
  }
  return 0;
}

/* Refresh.
 */

int ps_keyfocus_refresh(struct ps_keyfocus *keyfocus,struct ps_widget *container) {
  if (!keyfocus) return -1;

  /* Optimization for null container: drop everything.
   * In this case, we already know that any existing focus will not need a callback.
   */
  if (!container) {
    while (keyfocus->c>0) {
      keyfocus->c--;
      ps_widget_del(keyfocus->v[keyfocus->c]);
    }
    keyfocus->p=-1;
    return 0;
  }

  /* Remove any widgets no longer eligible.
   */
  int i=keyfocus->c; while (i-->0) {
    struct ps_widget *candidate=keyfocus->v[i];
    if (!ps_keyfocus_widget_is_eligible(candidate,container)) {
      keyfocus->c--;
      memmove(keyfocus->v+i,keyfocus->v+i+1,sizeof(void*)*(keyfocus->c-i));
      if (ps_keyfocus_farewell(keyfocus,candidate)<0) return -1;
      ps_widget_del(candidate);
      if (keyfocus->p==i) keyfocus->p=-1;
      else if (keyfocus->p>i) keyfocus->p--;
    }
  }

  /* Scan container for focusable widgets; add them if not already present.
   */
  if (ps_keyfocus_scan(keyfocus,container)<0) return -1;

  //TODO Is it worth the effort to sort the new list?
  // I'm not quite sure what that would look like, and I don't know if missorting is a realistic possibility.

  /* If we don't have a focus right now, but have valid candidates, focus the first one.
   */
  if ((keyfocus->p<0)&&(keyfocus->c>0)) {
    if (ps_keyfocus_hello(keyfocus,keyfocus->v[0])<0) return -1;
    keyfocus->p=0;
  }
  
  return 0;
}

/* Get focus.
 */

struct ps_widget *ps_keyfocus_get_focus(struct ps_keyfocus *keyfocus) {
  if (!keyfocus) return 0;
  if (keyfocus->p<0) return 0;
  struct ps_widget *focus=keyfocus->v[keyfocus->p];
  if (ps_keyfocus_widget_is_orphan(focus)) {
    ps_keyfocus_remove(keyfocus,focus);
    keyfocus->p=-1;
    return 0;
  } else {
    return focus;
  }
}

/* Change focus, public entry points.
 */

static int ps_keyfocus_move(struct ps_keyfocus *keyfocus,int d) {
  if (!keyfocus) return -1;
  if (keyfocus->c<1) return 0;
  int np=keyfocus->p+d;
  if (np<0) np=keyfocus->c-1;
  else if (np>=keyfocus->c) np=0;
  return ps_keyfocus_replace(keyfocus,np);
}

int ps_keyfocus_advance(struct ps_keyfocus *keyfocus) {
  return ps_keyfocus_move(keyfocus,1);
}

int ps_keyfocus_retreat(struct ps_keyfocus *keyfocus) {
  return ps_keyfocus_move(keyfocus,-1);
}

int ps_keyfocus_drop(struct ps_keyfocus *keyfocus) {
  return ps_keyfocus_replace(keyfocus,-1);
}

int ps_keyfocus_set_focus(struct ps_keyfocus *keyfocus,struct ps_widget *requested) {
  if (!keyfocus||!requested) return -1;
  int i=keyfocus->c; while (i-->0) {
    if (keyfocus->v[i]==requested) {
      return ps_keyfocus_replace(keyfocus,i);
    }
  }
  return -1;
}
