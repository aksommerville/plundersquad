#include "ps.h"
#include "ps_animation.h"
#include "ps_widget.h"

/* Object lifecycle.
 */
 
struct ps_animation *ps_animation_new(struct ps_widget *widget) {
  if (!widget) return 0;
  struct ps_animation *animation=calloc(1,sizeof(struct ps_animation));
  if (!animation) return 0;
  if (ps_widget_ref(widget)<0) {
    ps_animation_del(animation);
    return 0;
  }
  animation->widget=widget;
  return animation;
}

void ps_animation_del(struct ps_animation *animation) {
  if (!animation) return;
  ps_widget_del(animation->widget);
  free(animation);
}

/* Setup.
 */

int ps_animation_setup(
  struct ps_animation *animation,
  int k,int v,int duration
) {
  if (!animation) return -1;
  if (duration<0) return -1;
  animation->k=k;
  animation->ktype=ps_widget_get_property_type(animation->widget,k);
  if (animation->ktype==PS_WIDGET_PROPERTY_TYPE_UNDEFINED) return -1;
  if (ps_widget_get_property(&animation->va,animation->widget,k)<0) return -1;
  animation->vz=v;
  animation->p=0;
  if (animation->va==animation->vz) {
    // Redundant request, let it die fast.
    animation->c=0;
  } else {
    animation->c=duration;
  }
  return 0;
}

/* Calculate value.
 */

static int ps_animation_calculate_value(int type,int va,int vz,int p,int c) {
  if (p<=0) return va;
  if (p>=c) return vz;
  switch (type) {
    case PS_WIDGET_PROPERTY_TYPE_INTEGER:
    case PS_WIDGET_PROPERTY_TYPE_HOTINTEGER: {
        return va+(p*(vz-va))/c;
      }
    case PS_WIDGET_PROPERTY_TYPE_RGBA: {
        int v,i;
        uint8_t *vv=(uint8_t*)&v;
        uint8_t *vav=(uint8_t*)&va;
        uint8_t *vzv=(uint8_t*)&vz;
        for (i=0;i<4;i++) {
          vv[i]=vav[i]+(p*(vzv[i]-vav[i]))/c;
        }
        return v;
      }
  }
  return 0;
}

/* Update one animation.
 */

static int ps_animation_update(struct ps_animation *animation) {
  if (!animation) return -1;
  if (animation->p<animation->c) animation->p++;
  int v=ps_animation_calculate_value(animation->ktype,animation->va,animation->vz,animation->p,animation->c);
  if (ps_widget_set_property(animation->widget,animation->k,v)<0) return -1;
  return 0;
}

/* Update multiple animations.
 */

int ps_animations_update(struct ps_animation **animationv,int animationc) {
  if (animationc<1) return 0;
  if (!animationv) return -1;
  int i;

  /* Update all animations. */
  for (i=0;i<animationc;i++) {
    if (ps_animation_update(animationv[i])<0) return -1;
  }

  /* If we changed any HOTINTEGER fields, repack the widget -- but don't repack the same widget twice.
   * It's worth checking, because often 'w' and 'h' change together.
   */
  for (i=0;i<animationc;i++) {
    if (animationv[i]->ktype!=PS_WIDGET_PROPERTY_TYPE_HOTINTEGER) continue;
    int already=0;
    int j=i; while (j-->0) {
      if (animationv[j]->ktype!=PS_WIDGET_PROPERTY_TYPE_HOTINTEGER) continue;
      if (animationv[j]->widget==animationv[i]->widget) {
        already=1;
        break;
      }
    }
    if (already) continue;
    if (ps_widget_pack(animationv[i]->widget)<0) return -1;
  }
  
  return 0;
}

/* Eliminate completed animations.
 */
 
int ps_animations_gc(struct ps_animation **animationv,int animationc) {
  if (animationc<1) return 0;
  if (!animationv) return 0;
  int i;
  for (i=0;i<animationc;) {
    struct ps_animation *animation=animationv[i];
    if (!animation) return i;
    if (animation->p>=animation->c) {
      animationc--;
      memmove(animationv+i,animationv+i+1,sizeof(void*)*(animationc-i));
      ps_animation_del(animation);
    } else {
      i++;
    }
  }
  return animationc;
}
