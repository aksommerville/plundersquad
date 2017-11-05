#include "ps_gui_internal.h"

/* Clean up transition object.
 */
 
void ps_transition_cleanup(struct ps_transition *transition) {
  if (!transition) return;
  ps_widget_del(transition->widget);
  memset(transition,0,sizeof(struct ps_transition));
}

/* Get in-progress transition.
 */

static struct ps_transition *ps_gui_get_transition(const struct ps_gui *gui,const struct ps_widget *widget,int k) {
  struct ps_transition *transition=gui->transitionv;
  int i=gui->transitionc; for (;i-->0;transition++) {
    if (transition->widget!=widget) continue;
    if (transition->k!=k) continue;
    return transition;
  }
  return 0;
}

/* Create new transition.
 */

static struct ps_transition *ps_gui_new_transition(struct ps_gui *gui,struct ps_widget *widget) {

  if (gui->transitionc>=gui->transitiona) {
    int na=gui->transitiona+8;
    if (na>INT_MAX/sizeof(struct ps_transition)) return 0;
    void *nv=realloc(gui->transitionv,sizeof(struct ps_transition)*na);
    if (!nv) return 0;
    gui->transitionv=nv;
    gui->transitiona=na;
  }

  if (ps_widget_ref(widget)<0) return 0;

  struct ps_transition *transition=gui->transitionv+gui->transitionc++;
  memset(transition,0,sizeof(struct ps_transition));
  transition->widget=widget;
  return transition;
}

/* Begin property transition.
 */
 
int ps_gui_transition_property(struct ps_gui *gui,struct ps_widget *widget,int k,int v,int duration) {
  if (!gui||!widget) return -1;
  if (duration<1) return -1;
  struct ps_transition *transition=ps_gui_get_transition(gui,widget,k);

  /* Modify in-progress transition. */
  if (transition) {
    transition->va=ps_widget_get_property(widget,k);
    transition->vz=v;
    transition->p=0;
    transition->c=duration;
    transition->mode=PS_TRANSITION_MODE_ONCE;

  /* Create new transition. */
  } else {
    if (!(transition=ps_gui_new_transition(gui,widget))) return -1;
    transition->k=k;
    transition->va=ps_widget_get_property(widget,k);
    transition->vz=v;
    transition->p=0;
    transition->c=duration;
    transition->mode=PS_TRANSITION_MODE_ONCE;
  }

  return 0;
}

/* Begin property animation.
 */
 
int ps_gui_animate_property(struct ps_gui *gui,struct ps_widget *widget,int k,int va,int vz,int duration) {
  if (!gui||!widget) return -1;
  if (duration<2) return 0;
  struct ps_transition *transition=ps_gui_get_transition(gui,widget,k);

  /* Modify in-progress transition. */
  if (transition) {
    transition->va=ps_widget_get_property(widget,k);
    transition->vz=vz;
    transition->p=0;
    transition->c=duration;
    transition->mode=PS_TRANSITION_MODE_REPEAT;

  /* Create new transition. */
  } else {
    if (!(transition=ps_gui_new_transition(gui,widget))) return -1;
    transition->k=k;
    transition->va=va;
    transition->vz=vz;
    transition->p=0;
    transition->c=duration;
    transition->mode=PS_TRANSITION_MODE_REPEAT;
  }

  return 0;
}

/* End all transitions.
 */
 
int ps_gui_finish_transitions(struct ps_gui *gui) {
  if (!gui) return -1;
  while (gui->transitionc>0) {
    gui->transitionc--;
    struct ps_transition *transition=gui->transitionv+gui->transitionc;
    if (ps_widget_set_property(transition->widget,transition->k,transition->vz)<0) {
      ps_transition_cleanup(transition);
      return -1;
    }
    ps_transition_cleanup(transition);
  }
  return 0;
}

/* Get transition value.
 */

static int ps_transition_get_value(const struct ps_transition *transition) {

  /* (p,c) have different meanings depending on mode.
   * Adjust so they always behave such that (0..c) is (va..vz).
   */
  int p=transition->p;
  int c=transition->c;
  switch (transition->mode) {
    case PS_TRANSITION_MODE_ONCE: break;
    case PS_TRANSITION_MODE_REPEAT: {
        c>>=1;
        if (c>0) {
          if (p>c) p=(c<<1)-p;
        }
      } break;
  }

  /* Consider a few edge cases.
   */
  if (p<=0) return transition->va;
  if (p>=c) return transition->vz;
  if (transition->va==transition->vz) return transition->va;

  /* Take special behavior depending on format.
   */
  switch (transition->k) {

    /* Colors operate on a per-channel basis. */
    case PS_GUI_PROPERTY_fgrgba:
    case PS_GUI_PROPERTY_bgrgba: {
        const uint8_t *a=(uint8_t*)&transition->va;
        const uint8_t *z=(uint8_t*)&transition->vz;
        uint8_t v[4];
        int i=0; for (;i<4;i++) {
          v[i]=a[i]+((z[i]-a[i])*p)/c;
        }
        return *(int*)v;
      }

    /* Most properties scale directly. */
    default: {
        return transition->va+((transition->vz-transition->va)*p)/c;
      }
  }
}

/* Respond to changed property (eg repack if dimensions changed)
 */

static int ps_gui_property_changed(struct ps_widget *widget,int k) {
  switch (k) {
    case PS_GUI_PROPERTY_w:
    case PS_GUI_PROPERTY_h: {
        if (ps_widget_pack(widget)<0) return -1;
      } break;
  }
  return 0;
}

/* Update transitions.
 */
 
int ps_gui_update_transitions(struct ps_gui *gui) {
  if (!gui) return -1;

  /* First pass detect and remove dead widgets. */
  struct ps_transition *transition=gui->transitionv;
  int i=0; for (;i<gui->transitionc;) {
    if (!transition->widget||!transition->widget->parent) {
      ps_transition_cleanup(transition);
      gui->transitionc--;
      memmove(transition,transition+1,sizeof(struct ps_transition)*(gui->transitionc-i));
    } else {
      i++;
      transition++;
    }
  }

  /* Second pass updates values. */
  for (i=0,transition=gui->transitionv;i<gui->transitionc;i++,transition++) {
    transition->p++;
    int v=ps_transition_get_value(transition);
    int err=ps_widget_set_property(transition->widget,transition->k,v);
    if (err<0) return err;
    if (err>0) {
      if (ps_gui_property_changed(transition->widget,transition->k)<0) return -1;
    }
  }

  /* Third pass remove finished transitions. */
  for (i=0,transition=gui->transitionv;i<gui->transitionc;) {
    if (transition->p>=transition->c) {
      switch (transition->mode) {
        case PS_TRANSITION_MODE_REPEAT: {
            transition->p=0;
            i++;
            transition++;
          } break;
        case PS_TRANSITION_MODE_ONCE:
        default: {
            ps_transition_cleanup(transition);
            gui->transitionc--;
            memmove(transition,transition+1,sizeof(struct ps_transition)*(gui->transitionc-i));
          }
      }
    } else {
      i++;
      transition++;
    }
  }

  return 0;
}
