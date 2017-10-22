#include "ps_gui_internal.h"
#include "video/ps_video.h"

/* Object lifecycle.
 */

struct ps_widget *ps_widget_new(const struct ps_widget_type *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ps_widget)) return 0;

  struct ps_widget *widget=calloc(1,type->objlen);
  if (!widget) return 0;

  widget->type=type;
  widget->refc=1;

  if (type->init) {
    if (type->init(widget)<0) {
      ps_widget_del(widget);
      return 0;
    }
  }

  return widget;
}

void ps_widget_del(struct ps_widget *widget) {
  if (!widget) return;
  if (widget->refc-->1) return;

  if (widget->parent) ps_log(GUI,ERROR,"Deleting widget %p (%s), parent=%p (%s).",widget,widget->type->name,widget->parent,widget->parent->type->name);
  if (widget->childv) {
    while (widget->childc>0) {
      widget->childc--;
      struct ps_widget *child=widget->childv[widget->childc];
      child->parent=0;
      ps_widget_del(child);
    }
    free(widget->childv);
  }

  if (widget->type->del) widget->type->del(widget);

  free(widget);
}

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

/* Add child.
 */

static int ps_widget_childv_require(struct ps_widget *widget) {
  if (widget->childc<widget->childa) return 0;
  int na=widget->childa+8;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(widget->childv,sizeof(void*)*na);
  if (!nv) return -1;
  widget->childv=nv;
  widget->childa=na;
  return 0;
}
 
int ps_widget_add_child(struct ps_widget *parent,struct ps_widget *child) {
  if (!parent||!child) return -1;
  if (child->parent==parent) return 0;
  if (child->parent) return -1;
  if (ps_widget_is_ancestor(child,parent)) return -1;
  if (ps_widget_childv_require(parent)<0) return -1;
  if (ps_widget_ref(child)<0) return -1;
  parent->childv[parent->childc++]=child;
  child->parent=parent;
  return 0;
}

/* Remove child.
 */

int ps_widget_remove_child(struct ps_widget *parent,struct ps_widget *child) {
  if (!parent||!child) return -1;
  if (child->parent!=parent) return -1;
  child->parent=0;
  int i=parent->childc; while (i-->0) {
    if (parent->childv[i]==child) {
      parent->childc--;
      memmove(parent->childv+i,parent->childv+i+1,sizeof(void*)*(parent->childc-i));
      ps_widget_del(child);
      return 0;
    }
  }
  return -1;
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

/* Draw widget.
 */
 
int ps_widget_draw(struct ps_widget *widget,int x0,int y0) {
  if (!widget) return -1;
  //TODO should we establish a scissor for each widget?
  if (widget->type->draw) {
    if (widget->type->draw(widget,x0,y0)<0) return -1;
  } else {
    if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
    if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  }
  return 0;
}

/* Draw a widget's background.
 */
 
int ps_widget_draw_background(struct ps_widget *widget,int x0,int y0) {
  if (!widget) return -1;
  if (!(widget->bgrgba&0x000000ff)) return 0;
  if (ps_video_draw_rect(x0+widget->x,y0+widget->y,widget->w,widget->h,widget->bgrgba)<0) return -1;
  return 0;
}

/* Draw a widget's children.
 */
 
int ps_widget_draw_children(struct ps_widget *widget,int x0,int y0) {
  if (!widget) return -1;
  x0+=widget->x;
  y0+=widget->y;
  int i=0; for (;i<widget->childc;i++) {
    if (ps_widget_draw(widget->childv[i],x0,y0)<0) return -1;
  }
  return 0;
}

/* Measure widget.
 */

int ps_widget_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (!widget) return -1;
  int _w; if (!w) w=&_w;
  int _h; if (!h) h=&_h;
  if (maxw<0) maxw=0;
  if (maxh<0) maxh=0;
  if (widget->type->measure) {
    if (widget->type->measure(w,h,widget,maxw,maxh)<0) return -1;
  } else {
    *w=maxw;
    *h=maxh;
  }
  if (*w<0) *w=0; else if (*w>maxw) *w=maxw;
  if (*h<0) *h=0; else if (*h>maxh) *h=maxh;
  return 0;
}

/* Pack widget.
 */

int ps_widget_pack(struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type->pack) {
    if (widget->type->pack(widget)<0) return -1;
  } else {
    int i=0; for (;i<widget->childc;i++) {
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