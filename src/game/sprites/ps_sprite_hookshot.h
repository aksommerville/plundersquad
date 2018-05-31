/* ps_sprite_hookshot.h
 * Since they are so small, this is a dumping ground for trivial sprite type interfaces.
 */

#ifndef PS_SPRITE_HOOKSHOT_H
#define PS_SPRITE_HOOKSHOT_H

struct ps_sprite;
struct ps_game;

struct ps_sprite *ps_sprite_hookshot_new(struct ps_sprite *user,struct ps_game *game);

/* This one is unusual because we are in a position to determine whether the arrow fire must be rejected.
 * Normally we create a new sprite of type 'arrow' and return a weak reference to it, as expected.
 * Anything goes really wrong, we return null, as expected.
 * If the fire is legitimately rejected, eg firing into a wall, we return (user).
 * Callers should check for that case.
 */
struct ps_sprite *ps_sprite_arrow_new(struct ps_sprite *user,struct ps_game *game);

/* See ps_sprite_arrow_new(). We do the same thing re legitimate rejection.
 */
struct ps_sprite *ps_sprite_healmissile_new(struct ps_sprite *user,struct ps_game *game);

struct ps_sprite *ps_sprite_missile_new(int sprdefid,struct ps_sprite *user,double dstx,double dsty,struct ps_game *game);
int ps_sprite_missile_set_stop_at_walls(struct ps_sprite *spr,int stop);

#endif
