/* ps_region.
 * A region is the theme for a grid.
 * Grids are generated from a region and a blueprint, and some extra parameters.
 * Region declares one tilesheet and song, and describes the tilesheet.
 * This is a resource type.
 */

#ifndef PS_REGION_H
#define PS_REGION_H

#define PS_REGION_SHAPE_STYLE_SINGLE    0x00 /* One tile. */
#define PS_REGION_SHAPE_STYLE_ALT4      0x01 /* Four random alternatives 4x1 (1/2,1/4,1/8,1/8). */
#define PS_REGION_SHAPE_STYLE_ALT8      0x02 /* Eight random alteratives 4x2 (1/2,1/4,1/8,1/16,,1/32,1/64,1/128,1/128). */
#define PS_REGION_SHAPE_STYLE_EVEN4     0x03 /* Four random alternatives 4x1 equally likely. */
#define PS_REGION_SHAPE_STYLE_SKINNY    0x04 /* 4x4 combining tiles for single-width lines. */
#define PS_REGION_SHAPE_STYLE_FAT       0x05 /* 5x3 combining tiles for massive shapes. */
#define PS_REGION_SHAPE_STYLE_3X3       0x06 /* 3x3 exactly. */
#define PS_REGION_SHAPE_STYLE_ALT16     0x07 /* 4x4 singletons with heavy randomization, intended for VACANT mostly. */

#define PS_REGION_SHAPE_FLAG_ROUND    0x01 /* Round corners. */

#define PS_REGION_MONSTER_LIMIT 8

struct ps_region_shape {
  uint8_t physics; // Matches cell in blueprint. 
  uint8_t weight;  // Higher are more likely to be selected. Zero to disable.
  uint8_t tileid;  // First tile of this group.
  uint8_t style;   // How these tiles combine with each other.
  uint8_t flags;   // PS_REGION_SHAPE_FLAG_*
};

struct ps_region {
  int tsid;
  int songid;
  int monster_sprdefidv[PS_REGION_MONSTER_LIMIT];
  int shapec;
  struct ps_region_shape shapev[];
};

/* The size of the region's shape list is fixed at construction.
 * These are built to be instantiated from a resource, not for programmatic construction.
 * [Update, some months later: That previous guy was a jackass and should not have done it this way]
 */
struct ps_region *ps_region_new(int shapec);
void ps_region_del(struct ps_region *region);

/* Shapes must be sorted by physics before use.
 */
int ps_region_sort_shapes(struct ps_region *region);

/* The process for selecting a shape:
 *  - ps_region_get_shapes() to get the list of choices.
 *  - ps_region_shapes_calculate_weight() to get the weight limit. <1 is an error.
 *  - Choose a random number 0 to weight limit exclusive.
 *  - ps_region_shapes_get_by_weight() with your random number.
 */
int ps_region_get_shapes(const struct ps_region_shape **dst,const struct ps_region *region,uint8_t physics); // => c
int ps_region_shapes_calculate_weight(const struct ps_region_shape *shapev,int shapec);
const struct ps_region_shape *ps_region_shapes_get_by_weight(const struct ps_region_shape *shapev,int shapec,int selection);

int ps_region_count_monsters(const struct ps_region *region);
int ps_region_get_monster(const struct ps_region *region,int p); // (p) in (0..count-1), sparseness managed for you.
int ps_region_has_monster(const struct ps_region *region,int sprdefid);
int ps_region_add_monster(struct ps_region *region,int sprdefid); // Fallible, as there is a constant limit.
int ps_region_add_any_valid_monster(struct ps_region *region); // Add any valid sprdefid not yet used. Returns sprdefid.

/* Count and get with a difficulty filter.
 * These are used by the game when it generates the random monsters.
 * Any sprite which does not declare a difficulty is presumed valid.
 * Otherwise the argument you provide must be greater or equal to the sprite's difficulty.
 */
int ps_region_count_monsters_at_difficulty(const struct ps_region *region,int difficulty);
int ps_region_get_monster_at_difficulty(const struct ps_region *region,int p,int difficulty); // (p) in (0..count-1), sparseness managed for you.

/* Region resource is line-oriented text.
 * '#' begins a line comment.
 * Words are separated by whitespace.
 * Numbers are all unsigned and decimal or hexadecimal.
 * First word is the command:
 *   tilesheet ID                      # required once
 *   song ID
 *   shape PHYSICS WEIGHT TILEID STYLE [FLAGS...] # any count
 *     PHYSICS: ps_blueprint_cell_eval() (VACANT,SOLID,HOLE,LATCH,HAZARD,HEAL,HEROONLY)
 *     WEIGHT: integer
 *     TILEID: integer
 *     STYLE: ps_region_shape_style_eval() (SINGLE,ALT4,ALT8,EVEN4,SKINNY,FAT)
 *     FLAGS: ps_region_shape_flag_eval() (ROUND)
 *   monster SPRDEFID                  # 0..8
 * TODO binary format for region
 */
struct ps_region *ps_region_decode(const char *src,int srcc);
int ps_region_encode(void *dstpp,const struct ps_region *region);

#endif
