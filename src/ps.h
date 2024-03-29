#ifndef PS_H
#define PS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "ps_config.h"
#include "os/ps_log.h"

// Master frame rate, in hertz.
#define PS_FRAME_RATE 60

// Size of a tile, in pixels. Tiles are always square.
#define PS_TILESIZE 16

// Size of a screen, in tiles.
#define PS_GRID_COLC 25
#define PS_GRID_ROWC 14
#define PS_GRID_SIZE (PS_GRID_COLC*PS_GRID_ROWC)
#define PS_GRID_SIZE_LARGER PS_GRID_COLC

// Size of our main framebuffer, which is fixed.
#define PS_SCREENW (PS_TILESIZE*PS_GRID_COLC)
#define PS_SCREENH (PS_TILESIZE*PS_GRID_ROWC)

#define PS_BLUEPRINT_COLC (PS_GRID_COLC-4)
#define PS_BLUEPRINT_ROWC (PS_GRID_ROWC-4)
#define PS_BLUEPRINT_SIZE (PS_BLUEPRINT_COLC*PS_BLUEPRINT_ROWC)
#define PS_BLUEPRINT_SIZE_LARGER PS_BLUEPRINT_COLC

// World size limits, in screens.
// This is a sanity limit. Must not exceed 255 because we serialize coordinates as bytes.
#define PS_WORLD_W_LIMIT 32
#define PS_WORLD_H_LIMIT 32

// Loose advisory parameters for scenario construction.
#define PS_DIFFICULTY_MIN 1
#define PS_DIFFICULTY_MAX 9
#define PS_DIFFICULTY_DEFAULT 4
#define PS_LENGTH_MIN 1
#define PS_LENGTH_MAX 9
#define PS_LENGTH_DEFAULT 4

// Maximum count of treasures the generator will place in one world.
#define PS_TREASURE_LIMIT 16

// Maximum count of players. Minimum is 1.
#define PS_PLAYER_LIMIT 8

// Each player has a few bits for his skills, and each blueprint solution too.
#define PS_SKILL_SWORD       0x0001
#define PS_SKILL_ARROW       0x0002
#define PS_SKILL_HOOKSHOT    0x0004
#define PS_SKILL_FLAME       0x0008
#define PS_SKILL_HEAL        0x0010
#define PS_SKILL_IMMORTAL    0x0020
#define PS_SKILL_BOMB        0x0040
#define PS_SKILL_FLY         0x0080
#define PS_SKILL_MARTYR      0x0100
#define PS_SKILL_COMBAT      0x1000 /* Generic: hero can kill monsters */
// Remaining 3 reserved for generic skills

// If enabled, the B button swaps input. For testing only.
#define PS_B_TO_SWAP_INPUT 0

#endif
