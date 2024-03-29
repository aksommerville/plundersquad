/* ps_enums.h
 * There are a whole bunch of constant string sets that must be easily convertible to integers.
 * This file is meant to organize them all in one place.
 */

#ifndef PS_ENUMS_H
#define PS_ENUMS_H

/* For skills and group masks (and possibly others).
 * Convert between multiple enum bits and a string of multiple names.
 * Names are separated by any character outside [a-zA-Z0-9_].
 * (values_are_mask) is nonzero if the enum's values are already in mask form (skills).
 * Otherwise, we treat the enum value as a bit index (ie (1<<value)).
 */
int ps_enum_eval_multiple(int *dst,const char *src,int srcc,int values_are_mask,int (*eval1)(const char *word,int wordc));
int ps_enum_repr_multiple(char *dst,int dsta,int src,int values_are_mask,const char *(*repr1)(int bit));

/* ps.h:PS_SKILL_*
 * 16-bit mask.
 */
int ps_skill_eval(const char *src,int srcc);
const char *ps_skill_repr(uint16_t skill);

/* ps_sprite.h:PS_SPRDEF_FLD_*
 * Positive integer.
 */
int ps_sprdef_fld_k_eval(const char *src,int srcc);
const char *ps_sprdef_fld_k_repr(int k);

/* ps_game.h:PS_SPRGRP_*
 * Zero-based integer.
 */
int ps_sprgrp_eval(const char *src,int srcc);
const char *ps_sprgrp_repr(int grpindex);

/* ps_sprite.h:PS_SPRITE_SHAPE_*
 * Zero-based integer.
 */
int ps_sprite_shape_eval(const char *src,int srcc);
const char *ps_sprite_shape_repr(int shape);

/* ps_region.h:PS_REGION_SHAPE_STYLE_*
 * Zero-based integer.
 */
int ps_region_shape_style_eval(const char *src,int srcc);
const char *ps_region_shape_style_repr(int style);

/* ps_region.h:PS_REGION_SHAPE_FLAG_*
 * 8-bit mask.
 */
int ps_region_shape_flag_eval(const char *src,int srcc);
const char *ps_region_shape_flag_repr(int flag);

/* ps_blueprint.h:PS_BLUEPRINT_CELL_*
 * Zero-based integer.
 */
int ps_blueprint_cell_eval(const char *src,int srcc);
const char *ps_blueprint_cell_repr(int cell);

/* ps_player.h:PS_AWARD_*
 * Zero-based integer.
 * repr and eval are reversible but 'describe' is usually what you want.
 */
int ps_award_eval(const char *src,int srcc);
const char *ps_award_repr(int award);
const char *ps_award_describe(int award);

/* ps_blueprint.h:PS_BLUEPRINT_POI_*
 * Zero-based integer.
 */
int ps_poi_type_eval(const char *src,int srcc);
const char *ps_poi_type_repr(int type);

/* ps_log.h:PS_LOG_DOMAIN_*
 * Zero-based integer.
 */
int ps_log_domain_eval(const char *src,int srcc);
const char *ps_log_domain_repr(int domain);

/* ps_log.h:PS_LOG_LEVEL_*
 * Zero-based integer.
 */
int ps_log_level_eval(const char *src,int srcc);
const char *ps_log_level_repr(int level);

/* ps_input_button.h:PS_PLRBTN_*
 * 16-bit mask.
 */
const char *ps_plrbtn_repr(int btnid);
int ps_plrbtn_eval(const char *src,int srcc);
int ps_btnid_repr(char *dst,int dsta,int btnid);
int ps_btnid_eval(const char *src,int srcc);

/* ps_input_button.h:PS_INPUT_PROVIDER_*
 * Zero-based integer.
 */
int ps_input_provider_repr(char *dst,int dsta,int providerid);
int ps_input_provider_eval(const char *src,int srcc);

#endif
