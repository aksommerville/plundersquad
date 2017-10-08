/* ps_enums.h
 * There are a whole bunch of constant string sets that must be easily convertible to integers.
 * This file is meant to organize them all in one place.
 * TODO: Move all enums here.
 */

#ifndef PS_ENUMS_H
#define PS_ENUMS_H

/* ps.h:PS_SKILL_*
 * 16-bit mask.
 */
int ps_skill_eval(const char *src,int srcc);
const char *ps_skill_repr(uint16_t skill);

#endif
