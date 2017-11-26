/* ps_sound_effects.h
 * Glue to AKAU for playing foley.
 * Use this; don't call AKAU directly.
 */

#ifndef PS_SOUND_EFFECTS_H
#define PS_SOUND_EFFECTS_H

#include "akau/akau.h"

#define PS_SFX_SWORD akau_play_sound(12,0x80,0);
#define PS_SFX_HERO_HEAL akau_play_sound(16,0x80,0);
#define PS_SFX_HERO_HURT
#define PS_SFX_HERO_DEAD akau_play_sound(13,0x80,0);
#define PS_SFX_ARROW akau_play_sound(5,0x80,0);
#define PS_SFX_HOOKSHOT_BEGIN akau_play_sound(17,0x80,0);
#define PS_SFX_HOOKSHOT_BONK akau_play_sound(18,0x80,0);
#define PS_SFX_HOOKSHOT_GRAB akau_play_sound(19,0x80,0);
#define PS_SFX_FLAME
#define PS_SFX_HEALMISSILE
#define PS_SFX_TRANSFORM
#define PS_SFX_UNTRANSFORM
#define PS_SFX_EXPLODE akau_play_sound(7,0x80,0);

#define PS_SFX_BEGIN_PLAY
#define PS_SFX_SHIFT_SCREEN
#define PS_SFX_VICTORY
#define PS_SFX_PAUSE
#define PS_SFX_TREASURE
#define PS_SFX_DEATHGATE

#define PS_SFX_MONSTER_DEAD akau_play_sound(15,0x80,0);
#define PS_SFX_MONSTER_HURT akau_play_sound(14,0x80,0);
#define PS_SFX_DRAGONBUG_EXHALE
#define PS_SFX_RABBIT_FIRE
#define PS_SFX_RABBIT_TONGUE
#define PS_SFX_RABBIT_SPIT
#define PS_SFX_RABBIT_SWALLOW
#define PS_SFX_RABBIT_GRAB
#define PS_SFX_SEAMONSTER_FIRE
#define PS_SFX_SEAMONSTER_DIVE
#define PS_SFX_SEAMONSTER_SURFACE

#define PS_SFX_SWITCH_PRESS
#define PS_SFX_SWITCH_RELEASE
#define PS_SFX_SWORDSWITCH_UNLOCK
#define PS_SFX_SWORDSWITCH_LOCK

#define PS_SFX_GUI_ACTIVATE akau_play_sound(6,0x80,0);
#define PS_SFX_GUI_CANCEL akau_play_sound(4,0x80,0);
#define PS_SFX_MENU_MOVE akau_play_sound(3,0x80,0);
#define PS_SFX_MENU_ADJUST akau_play_sound(2,0x80,0);

#endif
