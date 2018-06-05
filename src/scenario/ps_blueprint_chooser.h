/* ps_blueprint_chooser.h
 * Minor helper to ps_scgen, to choose blueprints for every screen.
 * This is a sensitive and well-bounded task, so it makes sense in a unit of its own.
 */

#ifndef PS_BLUEPRINT_CHOOSER_H
#define PS_BLUEPRINT_CHOOSER_H

struct ps_blueprint_chooser;
struct ps_scgen;

/* No memory management.
 * Caller must keep the (scgen) alive, and we don't count references to ps_blueprint_chooser.
 */
struct ps_blueprint_chooser *ps_blueprint_chooser_new(struct ps_scgen *scgen);
void ps_blueprint_chooser_del(struct ps_blueprint_chooser *chooser);

/* Do all the work.
 * After this succeeds, every screen in (chooser->scgen) will have a valid blueprint and xform.
 * We will ask for a global list of blueprints, if (scgen->blueprints) is unset.
 */
int ps_blueprint_chooser_populate_screens(struct ps_blueprint_chooser *chooser);

#endif
