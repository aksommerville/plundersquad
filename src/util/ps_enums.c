#include "ps.h"
#include "ps_text.h"
#include "ps_enums.h"

/* Generic integer properties.
 */

static int ps_int_is_single_bit_16(int n) {
  int q=0x8000;
  for (;q;q>>=1) if (n==q) return 1;
  return 0;
}

/* Hero skills.
 */
 
int ps_skill_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  switch (srcc) {
    case 3: {
        if (!ps_memcasecmp(src,"FLY",3)) return PS_SKILL_FLY;
      } break;
    case 4: {
        if (!ps_memcasecmp(src,"HEAL",4)) return PS_SKILL_HEAL;
        if (!ps_memcasecmp(src,"FROG",4)) return PS_SKILL_FROG;
      } break;
    case 5: {
        if (!ps_memcasecmp(src,"SWORD",5)) return PS_SKILL_SWORD;
        if (!ps_memcasecmp(src,"ARROW",5)) return PS_SKILL_ARROW;
        if (!ps_memcasecmp(src,"FLAME",5)) return PS_SKILL_FLAME;
        if (!ps_memcasecmp(src,"CARRY",5)) return PS_SKILL_CARRY;
        if (!ps_memcasecmp(src,"SPEED",5)) return PS_SKILL_SPEED;
      } break;
    case 6: {
        if (!ps_memcasecmp(src,"MARTYR",6)) return PS_SKILL_MARTYR;
        if (!ps_memcasecmp(src,"WEIGHT",6)) return PS_SKILL_WEIGHT;
        if (!ps_memcasecmp(src,"COMBAT",6)) return PS_SKILL_COMBAT;
      } break;
    case 8: {
        if (!ps_memcasecmp(src,"HOOKSHOT",8)) return PS_SKILL_HOOKSHOT;
        if (!ps_memcasecmp(src,"IMMORTAL",8)) return PS_SKILL_IMMORTAL;
      } break;
  }
  
  int n;
  if (ps_int_eval(&n,src,srcc)>=0) {
    if (ps_int_is_single_bit_16(n)) return n;
  }

  return -1;
}

const char *ps_skill_repr(uint16_t skill) {
  switch (skill) {
    case PS_SKILL_SWORD: return "SWORD";
    case PS_SKILL_ARROW: return "ARROW";
    case PS_SKILL_HOOKSHOT: return "HOOKSHOT";
    case PS_SKILL_FLAME: return "FLAME";
    case PS_SKILL_HEAL: return "HEAL";
    case PS_SKILL_IMMORTAL: return "IMMORTAL";
    case PS_SKILL_CARRY: return "CARRY";
    case PS_SKILL_FLY: return "FLY";
    case PS_SKILL_MARTYR: return "MARTYR";
    case PS_SKILL_SPEED: return "SPEED";
    case PS_SKILL_WEIGHT: return "WEIGHT";
    case PS_SKILL_FROG: return "FROG";
    case PS_SKILL_COMBAT: return "COMBAT";
  }
  return 0;
}
