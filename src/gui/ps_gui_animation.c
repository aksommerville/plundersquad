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

  /* Create new transition. */
  } else {
    if (!(transition=ps_gui_new_transition(gui,widget))) return -1;
    transition->k=k;
    transition->va=ps_widget_get_property(widget,k);
    transition->vz=v;
    transition->p=0;
    transition->c=duration;
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
  if (transition->p<=0) return transition->va;
  if (transition->p>=transition->c) return transition->vz;
  if (transition->va==transition->vz) return transition->va;
  switch (transition->k) {

    /* Colors operate on a per-channel basis. */
    case PS_GUI_PROPERTY_fgrgba:
    case PS_GUI_PROPERTY_bgrgba: {
        const uint8_t *a=(uint8_t*)&transition->va;
        const uint8_t *z=(uint8_t*)&transition->vz;
        uint8_t v[4];
        int i=0; for (;i<4;i++) {
          v[i]=a[i]+((z[i]-a[i])*transition->p)/transition->c;
        }
        return *(int*)v;
      }

    /* Most properties scale directly. */
    default: {
        return transition->va+((transition->vz-transition->va)*transition->p)/transition->c;
      }
  }
}

/* Update transitions.
 */
 
int ps_gui_update_transitions(struct ps_gui *gui) {
  if (!gui) return -1;

  /* First pass updates values. */
  struct ps_transition *transition=gui->transitionv;
  int i=0; for (;i<gui->transitionc;i++,transition++) {
    transition->p++;
    int v=ps_transition_get_value(transition);
    if (ps_widget_set_property(transition->widget,transition->k,v)<0) return -1;
  }

  /* Second pass remove finished transitions. */
  for (i=0,transition=gui->transitionv;i<gui->transitionc;) {
    if (transition->p>=transition->c) {
      ps_transition_cleanup(transition);
      gui->transitionc--;
      memmove(transition,transition+1,sizeof(struct ps_transition)*(gui->transitionc-i));
    } else {
      i++;
      transition++;
    }
  }

  return 0;
}
