#include "ps.h"
#include "ps_widget.h"
#include "video/ps_video.h"
#include "corewidgets/ps_corewidgets.h"

/* New.
 */

struct ps_widget *ps_widget_new(const struct ps_widget_type *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ps_widget)) return 0;

  struct ps_widget *widget=calloc(1,type->objlen);
  if (!widget) return 0;

  widget->refc=1;
  widget->type=type;

  if (type->mouseenter) widget->accept_mouse_focus=1;
  if (type->key) widget->accept_keyboard_focus=1;

  if (type->init) {
    if (type->init(widget)<0) {
      ps_widget_del(widget);
      return 0;
    }
  }

  return widget;
}

/* Delete.
 */
 
void ps_widget_del(struct ps_widget *widget) {
  if (!widget) return;
  if (widget->refc-->1) return;

  if (widget->parent) ps_log(GUI,ERROR,"Deleting widget %p (%s), parent==%p.",widget,widget->type->name,widget->parent);
  if (widget->childv) {
    while (widget->childc-->0) {
      struct ps_widget *child=widget->childv[widget->childc];
      child->parent=0;
      ps_widget_del(child);
    }
    free(widget->childv);
  }

  if (widget->type->del) widget->type->del(widget);

  free(widget);
}

/* Retain.
 */
 
int ps_widget_ref(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->refc<1) return -1;
  if (widget->refc==INT_MAX) return -1;
  widget->refc++;
  return 0;
}

/* Test ancestry.
 */

int ps_widget_is_ancestor(const struct ps_widget *ancestor,const struct ps_widget *descendant) {
  if (!ancestor) return 0;
  while (descendant) {
    if (ancestor==descendant) return 1;
    descendant=descendant->parent;
  }
  return 0;
}

int ps_widget_has_child(const struct ps_widget *parent,const struct ps_widget *child) {
  if (!parent) return 0;
  return (parent==child->parent)?1:0;
}

/* Add child.
 */

int ps_widget_insert_child(struct ps_widget *parent,int p,struct ps_widget *child) {
  if (!parent||!child) return -1;
  if (child->parent) return -1;
  if (ps_widget_is_ancestor(child,parent)) return -1;
  if ((p<0)||(p>parent->childc)) p=parent->childc;

  if (parent->childc>=parent->childa) {
    int na=parent->childa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(parent->childv,sizeof(void*)*na);
    if (!nv) return -1;
    parent->childv=nv;
    parent->childa=na;
  }

  if (ps_widget_ref(child)<0) return -1;
  memmove(parent->childv+p+1,parent->childv+p,sizeof(void*)*(parent->childc-p));
  parent->childc++;
  parent->childv[p]=child;
  child->parent=parent;

  return 0;
}
 
int ps_widget_add_child(struct ps_widget *parent,struct ps_widget *child) {
  return ps_widget_insert_child(parent,-1,child);
}

/* Remove child.
 */
  
int ps_widget_remove_child(struct ps_widget *parent,struct ps_widget *child) {
  if (!parent||!child) return -1;
  if (child->parent!=parent) return -1;
  child->parent=0;
  int i; for (i=0;i<parent->childc;i++) {
    if (parent->childv[i]==child) {
      parent->childc--;
      memmove(parent->childv+i,parent->childv+i+1,sizeof(void*)*(parent->childc-i));
      ps_widget_del(child);
      return 0;
    }
  }
  return -1;
}

/* Remove all children.
 */
 
int ps_widget_remove_all_children(struct ps_widget *widget) {
  if (!widget) return -1;
  while (widget->childc>0) {
    widget->childc--;
    struct ps_widget *child=widget->childv[widget->childc];
    child->parent=0;
    ps_widget_del(child);
  }
  return 0;
}

/* Spawn child.
 */

struct ps_widget *ps_widget_spawn(struct ps_widget *parent,const struct ps_widget_type *type) {
  if (!parent) return 0;
  struct ps_widget *child=ps_widget_new(type);
  if (!child) return 0;
  if (ps_widget_add_child(parent,child)<0) {
    ps_widget_del(child);
    return 0;
  }
  ps_widget_del(child);
  return child;
}

/* Remove from parent, then repack parent.
 */
 
int ps_widget_kill(struct ps_widget *widget) {
  if (!widget) return -1;
  struct ps_widget *parent=widget->parent;
  if (!parent) return -1;
  if (ps_widget_remove_child(parent,widget)<0) return -1;
  if (ps_widget_pack(parent)<0) return -1;
  return 0;
}

/* Convert coordinates.
 */

int ps_widget_coords_to_screen(int *screenx,int *screeny,const struct ps_widget *widget,int widgetx,int widgety) {
  if (!widget) return -1;
  while (widget) {
    widgetx+=widget->x;
    widgety+=widget->y;
    widget=widget->parent;
  }
  if (screenx) *screenx=widgetx;
  if (screeny) *screeny=widgety;
  return 0;
}

int ps_widget_coords_from_screen(int *widgetx,int *widgety,const struct ps_widget *widget,int screenx,int screeny) {
  if (!widget) return -1;
  while (widget) {
    screenx-=widget->x;
    screeny-=widget->y;
    widget=widget->parent;
  }
  if (widgetx) *widgetx=screenx;
  if (widgety) *widgety=screeny;
  return 0;
}

int ps_widget_contains_screen_point(const struct ps_widget *widget,int x,int y) {
  if (ps_widget_coords_from_screen(&x,&y,widget,x,y)<0) return -1;
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=widget->w) return 0;
  if (y>=widget->h) return 0;
  return 1;
}

/* Get root and gui.
 */

struct ps_widget *ps_widget_get_root(const struct ps_widget *widget) {
  if (!widget) return 0;
  while (widget->parent) widget=widget->parent;
  return (struct ps_widget*)widget;
}

struct ps_gui *ps_widget_get_gui(const struct ps_widget *widget) {
  if (!(widget=ps_widget_get_root(widget))) return 0;
  return ps_widget_root_get_gui(widget);
}

/* Generic property accessors.
 */
 
int ps_widget_get_property(int *v,const struct ps_widget *widget,int k) {
  if (!v||!widget) return -1;

  /* Built-in properties. */
  switch (k) {
    case PS_WIDGET_PROPERTY_x: *v=widget->x; return 0;
    case PS_WIDGET_PROPERTY_y: *v=widget->y; return 0;
    case PS_WIDGET_PROPERTY_w: *v=widget->w; return 0;
    case PS_WIDGET_PROPERTY_h: *v=widget->h; return 0;
    case PS_WIDGET_PROPERTY_bgrgba: *v=widget->bgrgba; return 0;
    case PS_WIDGET_PROPERTY_fgrgba: *v=widget->fgrgba; return 0;
  }
  
  if (widget->type->get_property) return widget->type->get_property(v,widget,k);
  return -1;
}

int ps_widget_set_property(struct ps_widget *widget,int k,int v) {
  if (!widget) return -1;

  /* Built-in properties. */
  switch (k) {
    case PS_WIDGET_PROPERTY_x: widget->x=v; return 0;
    case PS_WIDGET_PROPERTY_y: widget->y=v; return 0;
    case PS_WIDGET_PROPERTY_w: widget->w=v; return 0;
    case PS_WIDGET_PROPERTY_h: widget->h=v; return 0;
    case PS_WIDGET_PROPERTY_bgrgba: widget->bgrgba=v; return 0;
    case PS_WIDGET_PROPERTY_fgrgba: widget->fgrgba=v; return 0;
  }

  if (widget->type->set_property) return widget->type->set_property(widget,k,v);
  return -1;
}

int ps_widget_get_property_type(const struct ps_widget *widget,int k) {
  if (!widget) return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;

  /* Built-in properties. */
  switch (k) {
    case PS_WIDGET_PROPERTY_x: return PS_WIDGET_PROPERTY_TYPE_INTEGER;
    case PS_WIDGET_PROPERTY_y: return PS_WIDGET_PROPERTY_TYPE_INTEGER;
    case PS_WIDGET_PROPERTY_w: return PS_WIDGET_PROPERTY_TYPE_HOTINTEGER;
    case PS_WIDGET_PROPERTY_h: return PS_WIDGET_PROPERTY_TYPE_HOTINTEGER;
    case PS_WIDGET_PROPERTY_bgrgba: return PS_WIDGET_PROPERTY_TYPE_RGBA;
    case PS_WIDGET_PROPERTY_fgrgba: return PS_WIDGET_PROPERTY_TYPE_RGBA;
  }

  if (widget->type->get_property_type) return widget->type->get_property_type(widget,k);
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

int ps_widget_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (!widget) return -1;
  if (widget->type->draw) {
    if (widget->type->draw(widget,parentx,parenty)<0) return -1;
  } else {
    if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
    if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  }
  return 0;
}

/* Draw, helpers.
 */
 
int ps_widget_draw_background(struct ps_widget *widget,int parentx,int parenty) {
  if (!widget) return -1;
  if (ps_video_draw_rect(parentx+widget->x,parenty+widget->y,widget->w,widget->h,widget->bgrgba)<0) return -1;
  return 0;
}

int ps_widget_draw_children(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  int i; for (i=0;i<widget->childc;i++) {
    if (ps_widget_draw(widget->childv[i],parentx,parenty)<0) return -1;
  }
  return 0;
}

/* Measure.
 */
 
int ps_widget_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (!widget) return -1;
  int _w; if (!w) w=&_w; *w=0;
  int _h; if (!h) h=&_h; *h=0;
  if (maxw<0) maxw=0;
  if (maxh<0) maxh=0;
  if (widget->type->measure) {
    if (widget->type->measure(w,h,widget,maxw,maxh)<0) return -1;
    if (*w>maxw) *w=maxw;
    if (*h>maxh) *h=maxh;
  } else {
    *w=maxw;
    *h=maxh;
  }
  return 0;
}

/* Pack.
 */

int ps_widget_pack(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->pack) {
    if (widget->type->pack(widget)<0) return -1;
  } else {
    int i; for (i=0;i<widget->childc;i++) {
      struct ps_widget *child=widget->childv[i];
      child->x=0;
      child->y=0;
      child->w=widget->w;
      child->h=widget->h;
      if (ps_widget_pack(child)<0) return -1;
    }
  }
  return 0;
}

/* Update children.
 */
 
int ps_widget_update_children(struct ps_widget *widget) {
  if (!widget) return -1;
  int i=0; for (;i<widget->childc;i++) {
    if (ps_widget_update(widget->childv[i])<0) return -1;
  }
  return 0;
}

/* Event hooks.
 */

int ps_widget_update(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->update) {
    return widget->type->update(widget);
  } else {
    return ps_widget_update_children(widget);
  }
}

int ps_widget_mousemotion(struct ps_widget *widget,int x,int y) {
  if (!widget) return -1;
  if (widget->type->mousemotion) return widget->type->mousemotion(widget,x,y);
  return 0;
}

int ps_widget_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (!widget) return -1;
  if (widget->type->mousebutton) return widget->type->mousebutton(widget,btnid,value);
  return 0;
}

int ps_widget_mousewheel(struct ps_widget *widget,int dx,int dy) {
  if (!widget) return -1;
  if (widget->type->mousewheel) return widget->type->mousewheel(widget,dx,dy);
  return 0;
}

int ps_widget_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  if (!widget) return -1;
  if (widget->type->key) return widget->type->key(widget,keycode,codepoint,value);
  return 0;
}

int ps_widget_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (!widget) return -1;
  if (widget->type->userinput) return widget->type->userinput(widget,plrid,btnid,value);
  return 0;
}

int ps_widget_mouseenter(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->mouseenter) return widget->type->mouseenter(widget);
  return 0;
}

int ps_widget_mouseexit(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->mouseexit) return widget->type->mouseexit(widget);
  return 0;
}

int ps_widget_activate(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->activate) return widget->type->activate(widget);
  return 0;
}

int ps_widget_cancel(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->cancel) return widget->type->cancel(widget);
  return 0;
}

int ps_widget_adjust(struct ps_widget *widget,int d) {
  if (!widget) return -1;
  if (widget->type->adjust) return widget->type->adjust(widget,d);
  return 0;
}

int ps_widget_focus(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->focus) return widget->type->focus(widget);
  return 0;
}

int ps_widget_unfocus(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->unfocus) return widget->type->unfocus(widget);
  return 0;
}
