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
#define PS_SFX_FLAMES_BEGIN akau_play_sound(40,0x80,0);
#define PS_SFX_FLAMES_THROW akau_play_sound(41,0x80,0);
#define PS_SFX_HEALMISSILE akau_play_sound(20,0x80,0);
#define PS_SFX_FRY_HEART akau_play_sound(42,0x80,0);
#define PS_SFX_TRANSFORM akau_play_sound(21,0x80,0);
#define PS_SFX_UNTRANSFORM akau_play_sound(22,0x80,0);
#define PS_SFX_EXPLODE akau_play_sound(7,0x80,0);

#define PS_SFX_BEGIN_PLAY
#define PS_SFX_SHIFT_SCREEN
#define PS_SFX_VICTORY
#define PS_SFX_PAUSE akau_play_sound(23,0x80,0);
#define PS_SFX_TREASURE akau_play_sound(24,0x80,0);
#define PS_SFX_DEATHGATE akau_play_sound(25,0x80,0);

#define PS_SFX_MONSTER_DEAD akau_play_sound(15,0x80,0);
#define PS_SFX_MONSTER_HURT akau_play_sound(14,0x80,0);
#define PS_SFX_DRAGONBUG_EXHALE akau_play_sound(26,0x80,0);
#define PS_SFX_RABBIT_FIRE akau_play_sound(30,0x80,0);
#define PS_SFX_RABBIT_TONGUE akau_play_sound(31,0x80,0);
#define PS_SFX_RABBIT_SPIT akau_play_sound(27,0x80,0);
#define PS_SFX_RABBIT_SWALLOW akau_play_sound(28,0x80,0);
#define PS_SFX_RABBIT_GRAB akau_play_sound(29,0x80,0);
#define PS_SFX_SEAMONSTER_FIRE akau_play_sound(32,0x80,0);
#define PS_SFX_SEAMONSTER_DIVE akau_play_sound(33,0x80,0);
#define PS_SFX_SEAMONSTER_SURFACE akau_play_sound(34,0x80,0);
#define PS_SFX_WOOF akau_play_sound(39,0x80,0);

#define PS_SFX_SWITCH_PRESS akau_play_sound(35,0x80,0);
#define PS_SFX_SWITCH_RELEASE akau_play_sound(36,0x80,0);
#define PS_SFX_SWORDSWITCH_UNLOCK akau_play_sound(37,0x80,0);
#define PS_SFX_SWORDSWITCH_LOCK akau_play_sound(38,0x80,0);

#define PS_SFX_GUI_ACTIVATE akau_play_sound(6,0x80,0);
#define PS_SFX_GUI_CANCEL akau_play_sound(4,0x80,0);
#define PS_SFX_MENU_MOVE akau_play_sound(3,0x80,0);
#define PS_SFX_MENU_ADJUST akau_play_sound(2,0x80,0);
#define PS_SFX_BTNMAP akau_play_sound(2,0x80,0);

#endif
