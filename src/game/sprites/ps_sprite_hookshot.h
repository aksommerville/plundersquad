/* ps_sprite_hookshot.h
 * Since they are so small, this is a dumping ground for trivial sprite type interfaces.
 */

#ifndef PS_SPRITE_HOOKSHOT_H
#define PS_SPRITE_HOOKSHOT_H

struct ps_sprite;
struct ps_game;

struct ps_sprite *ps_sprite_hookshot_new(struct ps_sprite *user,struct ps_game *game);
struct ps_sprite *ps_sprite_arrow_new(struct ps_sprite *user,struct ps_game *game);
struct ps_sprite *ps_sprite_healmissile_new(struct ps_sprite *user,struct ps_game *game);
struct ps_sprite *ps_sprite_missile_new(int sprdefid,struct ps_sprite *user,double dstx,double dsty,struct ps_game *game);

#endif
