/* ps_widget_resedit.c
 * Master view for resource editors.
 * We don't know how to load resources and we don't know anything about the data types.
 *
 * What we do:
 *  - Display resource ID.
 *  - Provide for stepping across the list of resources.
 *  - Hand off most of the screen to a type-specific editor.
 *  - Provide a "return to menu" control.
 *  - Provide a skeleton menu bar for the controller to populate.
 *
 * Children:
 *  [0] Menu bar.
 *  [1] Content. Transient, provided by controller.
 */

#include "ps.h"
#include "gui/ps_gui.h"

#define PS_RESEDIT_MENUBAR_HEIGHT 12

/* Widget definition.
 */

struct ps_widget_resedit {
  struct ps_widget hdr;
  struct ps_resedit_delegate delegate;
  int resindex;
};

#define WIDGET ((struct ps_widget_resedit*)widget)

/* Menubar callbacks.
 */
 
static int ps_resedit_cb_new(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (WIDGET->delegate.res_new) {
    struct ps_page *page=ps_widget_get_page(widget);
    if (page) {
      int index=WIDGET->delegate.res_new(page);
      if (index<0) return -1;
      if (WIDGET->delegate.res_load) {
        WIDGET->resindex=index;
        if (WIDGET->delegate.res_load(page,index)<0) return -1;
      }
    }
  }
  return 0;
}
 
static int ps_resedit_cb_del(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (WIDGET->delegate.res_del) {
    struct ps_page *page=ps_widget_get_page(widget);
    if (page) {
      if (WIDGET->delegate.res_del(page,WIDGET->resindex)<0) return -1;
      if (WIDGET->resindex>0) {
        if (WIDGET->delegate.res_count) {
          int resc=WIDGET->delegate.res_count(page);
          if (WIDGET->resindex>=resc) WIDGET->resindex--;
        }
      }
      if (WIDGET->delegate.res_load) {
        if (WIDGET->delegate.res_load(page,WIDGET->resindex)<0) return -1;
      }
    }
  }
  return 0;
}
 
static int ps_resedit_cb_prev(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (WIDGET->delegate.res_load) {
    struct ps_page *page=ps_widget_get_page(widget);
    if (page) {
      WIDGET->resindex--;
      if (WIDGET->resindex<0) {
        if (WIDGET->delegate.res_count) {
          WIDGET->resindex=WIDGET->delegate.res_count(page);
          if (WIDGET->resindex<0) return -1;
          if (WIDGET->resindex>0) WIDGET->resindex--;
        } else {
          WIDGET->resindex=0;
        }
      }
      if (WIDGET->delegate.res_load(page,WIDGET->resindex)<0) return -1;
    }
  }
  return 0;
}
 
static int ps_resedit_cb_next(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (WIDGET->delegate.res_load) {
    struct ps_page *page=ps_widget_get_page(widget);
    if (page) {
      WIDGET->resindex++;
      if (WIDGET->delegate.res_count) {
        int c=WIDGET->delegate.res_count(page);
        if (WIDGET->resindex>=c) WIDGET->resindex=0;
      }
      if (WIDGET->delegate.res_load(page,WIDGET->resindex)<0) return -1;
    }
  }
  return 0;
}

/* Delete.
 */

static void _ps_resedit_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_resedit_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_reseditmenu))) return -1;
  if (!ps_widget_reseditmenu_add_menu(child,"New",3,ps_resedit_cb_new,widget,0)) return -1;
  if (!ps_widget_reseditmenu_add_menu(child,"Del",3,ps_resedit_cb_del,widget,0)) return -1;
  if (!ps_widget_reseditmenu_add_menu(child,"Prev",4,ps_resedit_cb_prev,widget,0)) return -1;
  if (!ps_widget_reseditmenu_add_menu(child,"Next",4,ps_resedit_cb_next,widget,0)) return -1;

  if (!(child=ps_widget_spawn_label(widget,"Content not loaded",-1,0x000000ff))) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_resedit_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_resedit_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_resedit_pack(struct ps_widget *widget) {

  if (widget->childc!=2) return -1;
  struct ps_widget *menubar=widget->childv[0];
  struct ps_widget *content=widget->childv[1];

  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=PS_RESEDIT_MENUBAR_HEIGHT;

  content->x=0;
  content->y=PS_RESEDIT_MENUBAR_HEIGHT;
  content->w=widget->w;
  content->h=widget->h-content->y;

  if (ps_widget_pack(menubar)<0) return -1;
  if (ps_widget_pack(content)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_resedit={
  .name="resedit",
  .objlen=sizeof(struct ps_widget_resedit),
  .del=_ps_resedit_del,
  .init=_ps_resedit_init,
  .draw=_ps_resedit_draw,
  .measure=_ps_resedit_measure,
  .pack=_ps_resedit_pack,
};

/* Set editor.
 */

int ps_widget_resedit_set_editor(struct ps_widget *widget,struct ps_widget *editor) {
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (widget->childc!=2) return -1;

  if (ps_widget_remove_child(widget,widget->childv[1])<0) return -1;
  if (editor) {
    if (ps_widget_add_child(widget,editor)<0) return -1;
  } else {
    if (!ps_widget_spawn_label(widget,"Content not loaded",-1,0x000000ff)) return -1;
  }

  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

struct ps_widget *ps_widget_resedit_get_editor(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return 0;
  if (widget->childc<2) return 0;
  return widget->childv[1];
}

/* Unload resource.
 */

static int ps_resedit_unload_resource(struct ps_widget *widget) {
  WIDGET->resindex=0;
  if (ps_widget_resedit_set_editor(widget,0)<0) return -1;
  return 0;
}

/* Set delegate.
 */
 
int ps_widget_resedit_set_delegate(struct ps_widget *widget,const struct ps_resedit_delegate *delegate) {
  if (!widget||(widget->type!=&ps_widget_type_resedit)) return -1;
  if (ps_resedit_unload_resource(widget)<0) return -1;
  if (delegate) {
    memcpy(&WIDGET->delegate,delegate,sizeof(struct ps_resedit_delegate));
    if (WIDGET->delegate.res_load) {
      if (WIDGET->delegate.res_load(ps_widget_get_page(widget),0)<0) return -1;
    }
  } else {
    memset(&WIDGET->delegate,0,sizeof(struct ps_resedit_delegate));
  }
  return 0;
}
