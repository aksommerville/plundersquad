/* ps_animation.h
 * Most users do not need this interface, only ps_gui should use it.
 */

#ifndef PS_ANIMATION_H
#define PS_ANIMATION_H

struct ps_animation {
  struct ps_widget *widget;
  int k,ktype;
  int va,vz;
  int p,c;
};

struct ps_animation *ps_animation_new(struct ps_widget *widget);
void ps_animation_del(struct ps_animation *animation);

int ps_animation_setup(
  struct ps_animation *animation,
  int k,int v,int duration
);

int ps_animations_update(struct ps_animation **animationv,int animationc);
int ps_animations_gc(struct ps_animation **animationv,int animationc); // => animationc, infallible

#endif
