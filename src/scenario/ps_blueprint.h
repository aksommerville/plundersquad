/* ps_blueprint.h
 * Template from which to generate one screen of geography ("grid").
 * Blueprints are part of the permanent data set; grids are part of the scenario.
 * Blueprint cells contain physics only; they are skinned with graphics in a later phase.
 *
 * The scenario is assembled based on challenges.
 * Each blueprint may define one challenge, meaning that only a party of certain composition can pass.
 * The direction of the challenge must be left-to-right or top-to-bottom.
 * Blueprints may be flipped on either axis during grid assembly.
 *
 * Blueprints are 4 cells smaller than grids on each axis.
 * Every blueprint must have at least one passable cell on each edge.
 * Grid generator will compose the two outer rings as needed to match neighbors.
 *
 */

#ifndef PS_BLUEPRINT_H
#define PS_BLUEPRINT_H

#define PS_BLUEPRINT_CELL_VACANT       0x00 /* Pass through freely. */
#define PS_BLUEPRINT_CELL_SOLID        0x01 /* Nothing may pass. */
#define PS_BLUEPRINT_CELL_HOLE         0x02 /* Pass aerially only. */
#define PS_BLUEPRINT_CELL_LATCH        0x03 /* SOLID, and gadgeteer can grab it. */
#define PS_BLUEPRINT_CELL_HEROONLY     0x04 /* VACANT, but monsters may not pass. */
#define PS_BLUEPRINT_CELL_HAZARD       0x05 /* VACANT, but harmful to heroes. Illegal as an edge. */
#define PS_BLUEPRINT_CELL_HEAL         0x06 /* VACANT, but resuccitates heroes. */

#define PS_BLUEPRINT_POI_NOOP          0x00
#define PS_BLUEPRINT_POI_SPRITE        0x01 /* [0]=sprdefid, [1,2]=argv */
#define PS_BLUEPRINT_POI_TREASURE      0x02
#define PS_BLUEPRINT_POI_HERO          0x03 /* [0]=sprdefid, [1,2]=argv */
#define PS_BLUEPRINT_POI_BARRIER       0x04 /* [0]=barrierid. Toggles between selected cell and zero. */
#define PS_BLUEPRINT_POI_DEATHGATE     0x05 /* [0]=blocked tile, [1]=open tile */

#define PS_BLUEPRINT_XFORM_NONE        0x00
#define PS_BLUEPRINT_XFORM_HORZ        0x01
#define PS_BLUEPRINT_XFORM_VERT        0x02
#define PS_BLUEPRINT_XFORM_BOTH        0x03

struct ps_blueprint_poi {
  uint8_t x,y;
  uint8_t type;
  int argv[3];
};

struct ps_blueprint_solution {
  uint8_t plo,phi;    // 1..8
  uint8_t difficulty; // 1..9
  uint8_t preference; // 1..255
  uint16_t skills;    // mask of PS_SKILL_*
};

struct ps_blueprint {
  int refc;
  struct ps_blueprint_poi *poiv;
  int poic,poia;
  struct ps_blueprint_solution *solutionv;
  int solutionc,solutiona;
  uint8_t monsterc_min,monsterc_max;
  uint8_t cellv[PS_BLUEPRINT_SIZE];
};

struct ps_blueprint *ps_blueprint_new();
void ps_blueprint_del(struct ps_blueprint *blueprint);
int ps_blueprint_ref(struct ps_blueprint *blueprint);

/* Get one cell value, applying transform.
 * Any error, or anything out of bounds, reports as PS_BLUEPRINT_CELL_SOLID.
 */
uint8_t ps_blueprint_get_cell(const struct ps_blueprint *blueprint,int x,int y,uint8_t xform);

static inline int ps_blueprint_cell_is_passable(uint8_t cell) {
  switch (cell) {
    case PS_BLUEPRINT_CELL_VACANT:
    case PS_BLUEPRINT_CELL_HEROONLY:
    case PS_BLUEPRINT_CELL_HAZARD:
    case PS_BLUEPRINT_CELL_HEAL:
      return 1;
  }
  return 0;
}

/* Count POIs at the given cell and put pointers to them in (dst).
 * Always returns the correct count; never writes beyond (dsta).
 */
int ps_blueprint_get_pois_for_cell(
  struct ps_blueprint_poi **dst,int dsta,
  const struct ps_blueprint *blueprint,
  int x,int y,uint8_t xform
);

int ps_blueprint_count_poi_of_type(const struct ps_blueprint *blueprint,uint8_t type);

/* Nonzero if this blueprint's challenge can be solved by the given skill set.
 * A blueprint with no solutions is presumed to be universally solvable.
 * We optionally return the minimum difficulty of all matching solutions.
 */
int ps_blueprint_is_solvable(
  uint8_t *difficulty,
  const struct ps_blueprint *blueprint,
  int playerc,uint16_t skills
);

/* Return the subjective "preference" value for this blueprint with the given skill set.
 * When selecting among blueprints, the generator may bias towards higher preferences.
 * Solutions should have a nonzero preference, we return zero if not solvable.
 * The default preference for challengeless blueprints is one.
 * We select the preference attached to the least difficult matching solution.
 */
uint8_t ps_blueprint_get_preference(
  const struct ps_blueprint *blueprint,
  int playerc,uint16_t skills
);

int ps_blueprint_clear(struct ps_blueprint *blueprint);

/* Create a new POI of straight zeroes, add it to the list.
 * POIs' positions are untransformed.
 * They are limited to 255.
 */
struct ps_blueprint_poi *ps_blueprint_add_poi(struct ps_blueprint *blueprint);

/* Declare a possible solution to the blueprint's challenge.
 * A blueprint with no declared solutions is presumed to have no challenge, ie universally solvable.
 * We validate inputs but don't check for redundancy.
 * We may sanitize inputs.
 */
int ps_blueprint_add_solution(
  struct ps_blueprint *blueprint,
  uint8_t plo,uint8_t phi,
  uint8_t difficulty,uint8_t preference,
  uint16_t skills
);

/* Blueprints encode to a binary format:
 *   0001   2 cellc, for validation only. must be PS_BLUEPRINT_SIZE. (21*10==210)
 *   0002   1 solutionc
 *   0003   1 poic
 *   0004   1 monsterc_min
 *   0005   1 monsterc_max
 *   0006  10 reserved
 *   0010 ... cells
 *   .... ... solutions:
 *              0000   1 plo
 *              0001   1 phi
 *              0002   1 difficulty
 *              0003   1 preference
 *              0004   2 skills
 *              0006
 *   .... ... pois:
 *              0000   1 x
 *              0001   1 y
 *              0002   1 type
 *              0003   1 reserved
 *              0004   4 arg0
 *              0008   4 arg1
 *              000c   4 arg2
 *              0010
 *   ....
 *
 * For decoding only, we also accept a text format.
 * Text must begin with a byte 0x20 or greater, that's enough to distinguish it from binary.
 * Text consists of header and body, separated by a line beginning with '='.
 * Header lines are:
 *   solution PLO PHI DIFFICULTY PREFERENCE [SKILL...]
 *   poi X Y TYPE INTEGER INTEGER INTEGER
 *   monsters MIN MAX
 * Body is a 25x14 image, two characters per cell. Blank lines are ignored.
 * Only the first character of each body cell is examined:
 *   , VACANT
 *   X SOLID
 *   O HOLE
 *   L LATCH
 *   ? OPTIONAL
 *   - HEROONLY
 *   ! HAZARD
 *   H HEAL
 */
int ps_blueprint_encode(void *dst,int dsta,const struct ps_blueprint *blueprint);
int ps_blueprint_decode(struct ps_blueprint *blueprint,const void *src,int srcc);

int ps_blueprint_cell_eval(const char *src,int srcc);

#endif
