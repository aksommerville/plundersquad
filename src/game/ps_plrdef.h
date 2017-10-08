/* ps_plrdef.h
 * Resource type for a player template.
 * Each plrdef declares the graphics, skills, and a set of colors.
 */

#ifndef PS_PLRDEF_H
#define PS_PLRDEF_H

struct ps_plrdef_palette {
  uint32_t rgba_head;
  uint32_t rgba_body;
};

struct ps_plrdef {
  uint8_t tileid_head;
  uint8_t tileid_body;
  uint16_t skills;
  struct ps_plrdef_palette *palettev;
  int palettec,palettea;
};

struct ps_plrdef *ps_plrdef_new();
void ps_plrdef_del(struct ps_plrdef *plrdef);

/* Serial format is text with comments.
 * Lines are processed as whitespace-delimited words.
 * First word is the key:
 *   head TILEID
 *   body TILEID
 *   skill STRING
 *   colors HEAD BODY
 * "skill" and "colors" may appear more than once.
 */
int ps_plrdef_decode(struct ps_plrdef *plrdef,const void *src,int srcc);

#endif
