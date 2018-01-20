/* ps_keyfocus.h
 * Helper for coordinating keyboard focus ring.
 * One of these is owned by the root widget.
 */

#ifndef PS_KEYFOCUS_H
#define PS_KEYFOCUS_H

struct ps_widget;

struct ps_keyfocus {
  int refc;
  struct ps_widget **v; // Ring of eligible widgets.
  int c,a;
  int p; // Current focus, or -1
};

/* We do not fire any callbacks on deletion.
 */
struct ps_keyfocus *ps_keyfocus_new();
void ps_keyfocus_del(struct ps_keyfocus *keyfocus);
int ps_keyfocus_ref(struct ps_keyfocus *keyfocus);

/* Rebuild the focus ring.
 * Only (container) and its children are eligible for focus.
 * If a widget is currently focussed:
 *  - If it remains eligible, it will see no change.
 *  - If no longer eligible but still attached to a root widget, we ceremoniously unfocus it.
 *  - If no longer eligible and orphaned from the root, we drop it without any callbacks.
 * If no widget was focussed, or we drop the existing focus, we will focus the first eligible widget.
 * When a widget receives focus we trigger its focus callback.
 */
int ps_keyfocus_refresh(struct ps_keyfocus *keyfocus,struct ps_widget *container);

/* Get the current focus widget.
 * We do a quick orphan check before returning it; focussed widget must be descended from a root.
 */
struct ps_widget *ps_keyfocus_get_focus(struct ps_keyfocus *keyfocus);

/* Routine change-of-focus events.
 * These trigger callbacks as warranted.
 * Setting focus manually only works if we already know it as a candidate.
 */
int ps_keyfocus_advance(struct ps_keyfocus *keyfocus);
int ps_keyfocus_retreat(struct ps_keyfocus *keyfocus);
int ps_keyfocus_drop(struct ps_keyfocus *keyfocus);
int ps_keyfocus_set_focus(struct ps_keyfocus *keyfocus,struct ps_widget *requested);

#endif
