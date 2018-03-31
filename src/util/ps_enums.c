#include "ps.h"
#include "ps_text.h"
#include "ps_enums.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "scenario/ps_region.h"
#include "scenario/ps_blueprint.h"

/* Generic integer properties.
 */

static int ps_int_is_single_bit_16(int n) {
  int q=0x8000;
  for (;q;q>>=1) if (n==q) return 1;
  return 0;
}

/* Eval multiple.
 */

static int ps_enum_is_ident(char ch) {
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  return 0;
}
 
int ps_enum_eval_multiple(int *dst,const char *src,int srcc,int values_are_mask,int (*eval1)(const char *word,int wordc)) {
  if (!dst||!eval1) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  *dst=0;
  int srcp=0;
  while (srcp<srcc) {
    if (!ps_enum_is_ident(src[srcp])) {
      srcp++;
      continue;
    }
    const char *token=src+srcp;
    int tokenc=1;
    srcp++;
    while ((srcp<srcc)&&ps_enum_is_ident(src[srcp])) { srcp++; tokenc++; }
    int value=eval1(token,tokenc);
    if (value<0) {
      ps_log(TEXT,ERROR,"Unexpected token '%.*s'.",tokenc,token);
      return -1;
    }
    if (!values_are_mask) value=1<<value;
    (*dst)|=value;
  }
  return 0;
}

/* Represent multiple.
 */
 
int ps_enum_repr_multiple(char *dst,int dsta,int src,int values_are_mask,const char *(*repr1)(int bit)) {
  if (!repr1) return -1;
  if (!dst||(dsta<0)) dsta=0;
  int dstc=0;
  if (src) {
    int mask=1,index=0;
    for (;index<32;mask<<=1,index++) {
      if (src&mask) {
        if (dstc) {
          if (dstc<dsta) dst[dstc]=' ';
          dstc++;
        }
        const char *name=repr1(values_are_mask?mask:index);
        if (!name) return -1;
        dstc+=ps_strcpy(dst+dstc,dsta-dstc,name,-1);
      }
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
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
        if (!ps_memcasecmp(src,"BOMB",4)) return PS_SKILL_BOMB;
      } break;
    case 5: {
        if (!ps_memcasecmp(src,"SWORD",5)) return PS_SKILL_SWORD;
        if (!ps_memcasecmp(src,"ARROW",5)) return PS_SKILL_ARROW;
        if (!ps_memcasecmp(src,"FLAME",5)) return PS_SKILL_FLAME;
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
    case PS_SKILL_BOMB: return "BOMB";
    case PS_SKILL_FLY: return "FLY";
    case PS_SKILL_MARTYR: return "MARTYR";
    case PS_SKILL_SPEED: return "SPEED";
    case PS_SKILL_WEIGHT: return "WEIGHT";
    case PS_SKILL_FROG: return "FROG";
    case PS_SKILL_COMBAT: return "COMBAT";
  }
  return 0;
}

/* sprdef extra keys.
 */
 
int ps_sprdef_fld_k_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    case 4: {
        if (!ps_memcasecmp(src,"type",4)) return PS_SPRDEF_FLD_type;
      } break;
    case 5: {
        if (!ps_memcasecmp(src,"layer",5)) return PS_SPRDEF_FLD_layer;
        if (!ps_memcasecmp(src,"shape",5)) return PS_SPRDEF_FLD_shape;
      } break;
    case 6: {
        if (!ps_memcasecmp(src,"radius",6)) return PS_SPRDEF_FLD_radius;
        if (!ps_memcasecmp(src,"tileid",6)) return PS_SPRDEF_FLD_tileid;
        if (!ps_memcasecmp(src,"framec",6)) return PS_SPRDEF_FLD_framec;
      } break;
    case 7: {
        if (!ps_memcasecmp(src,"grpmask",7)) return PS_SPRDEF_FLD_grpmask;
      } break;
    case 8: {
        if (!ps_memcasecmp(src,"switchid",8)) return PS_SPRDEF_FLD_switchid;
      } break;
    case 9: {
        if (!ps_memcasecmp(src,"frametime",9)) return PS_SPRDEF_FLD_frametime;
      } break;
    case 10: {
        if (!ps_memcasecmp(src,"impassable",10)) return PS_SPRDEF_FLD_impassable;
        if (!ps_memcasecmp(src,"difficulty",10)) return PS_SPRDEF_FLD_difficulty;
      } break;
  }
  return -1;
}

const char *ps_sprdef_fld_k_repr(int k) {
  switch (k) {
    case PS_SPRDEF_FLD_layer: return "layer";
    case PS_SPRDEF_FLD_grpmask: return "grpmask";
    case PS_SPRDEF_FLD_radius: return "radius";
    case PS_SPRDEF_FLD_shape: return "shape";
    case PS_SPRDEF_FLD_type: return "type";
    case PS_SPRDEF_FLD_tileid: return "tileid";
    case PS_SPRDEF_FLD_impassable: return "impassable";
    case PS_SPRDEF_FLD_difficulty: return "difficulty";
    case PS_SPRDEF_FLD_framec: return "framec";
    case PS_SPRDEF_FLD_frametime: return "frametime";
    case PS_SPRDEF_FLD_switchid: return "switchid";
  }
  return 0;
}

/* Built-in sprite groups.
 */
 
int ps_sprgrp_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    case 4: {
        if (!ps_memcasecmp(src,"HERO",4)) return PS_SPRGRP_HERO;
      } break;
    case 5: {
        if (!ps_memcasecmp(src,"LATCH",5)) return PS_SPRGRP_LATCH;
        if (!ps_memcasecmp(src,"SOLID",5)) return PS_SPRGRP_SOLID;
        if (!ps_memcasecmp(src,"PRIZE",5)) return PS_SPRGRP_PRIZE;
      } break;
    case 6: {
        if (!ps_memcasecmp(src,"UPDATE",6)) return PS_SPRGRP_UPDATE;
        if (!ps_memcasecmp(src,"HAZARD",6)) return PS_SPRGRP_HAZARD;
      } break;
    case 7: {
        if (!ps_memcasecmp(src,"VISIBLE",7)) return PS_SPRGRP_VISIBLE;
        if (!ps_memcasecmp(src,"PHYSICS",7)) return PS_SPRGRP_PHYSICS;
        if (!ps_memcasecmp(src,"FRAGILE",7)) return PS_SPRGRP_FRAGILE;
        if (!ps_memcasecmp(src,"BARRIER",7)) return PS_SPRGRP_BARRIER;
      } break;
    case 8: {
        if (!ps_memcasecmp(src,"DEATHROW",8)) return PS_SPRGRP_DEATHROW;
        if (!ps_memcasecmp(src,"TREASURE",8)) return PS_SPRGRP_TREASURE;
        if (!ps_memcasecmp(src,"TELEPORT",8)) return PS_SPRGRP_TELEPORT;
      } break;
    case 9: {
        if (!ps_memcasecmp(src,"KEEPALIVE",9)) return PS_SPRGRP_KEEPALIVE;
      } break;
    case 10: {
        if (!ps_memcasecmp(src,"HEROHAZARD",10)) return PS_SPRGRP_HEROHAZARD;
      } break;
  }
  return -1;
}

const char *ps_sprgrp_repr(int grpindex) {
  switch (grpindex) {
    #define _(tag) case PS_SPRGRP_##tag: return #tag;
    _(KEEPALIVE)
    _(DEATHROW)
    _(VISIBLE)
    _(UPDATE)
    _(PHYSICS)
    _(HERO)
    _(HAZARD)
    _(HEROHAZARD)
    _(FRAGILE)
    _(TREASURE)
    _(LATCH)
    _(SOLID)
    _(PRIZE)
    _(BARRIER)
    _(TELEPORT)
    #undef _
  }
  return 0;
}

/* Sprite shape.
 */
 
int ps_sprite_shape_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc==6)&&!ps_memcasecmp(src,"SQUARE",6)) return PS_SPRITE_SHAPE_SQUARE;
  if ((srcc==6)&&!ps_memcasecmp(src,"CIRCLE",6)) return PS_SPRITE_SHAPE_CIRCLE;
  return -1;
}

const char *ps_sprite_shape_repr(int shape) {
  switch (shape) {
    case PS_SPRITE_SHAPE_SQUARE: return "SQUARE";
    case PS_SPRITE_SHAPE_CIRCLE: return "CIRCLE";
  }
  return 0;
}

/* Region shape style.
 */
 
int ps_region_shape_style_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_REGION_SHAPE_STYLE_##tag;
  _(SINGLE)
  _(ALT4)
  _(ALT8)
  _(EVEN4)
  _(SKINNY)
  _(FAT)
  _(3X3)
  _(ALT16)
  #undef _
  return -1;
}

const char *ps_region_shape_style_repr(int shape) {
  switch (shape) {
    #define _(tag) case PS_REGION_SHAPE_STYLE_##tag: return #tag;
    _(SINGLE)
    _(ALT4)
    _(ALT8)
    _(EVEN4)
    _(SKINNY)
    _(FAT)
    _(3X3)
    _(ALT16)
    #undef _
  }
  return 0;
}

/* Region shape flag.
 */

int ps_region_shape_flag_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_REGION_SHAPE_FLAG_##tag;
  _(ROUND)
  #undef _
  return -1;
}

const char *ps_region_shape_flag_repr(int flag) {
  switch (flag) {
    case 0: return "";
    case PS_REGION_SHAPE_FLAG_ROUND: return "ROUND";
  }
  return 0;
}

/* Blueprint cell.
 */
 
int ps_blueprint_cell_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_BLUEPRINT_CELL_##tag;
  _(VACANT)
  _(SOLID)
  _(HOLE)
  _(LATCH)
  _(HEROONLY)
  _(HAZARD)
  _(HEAL)
  _(STATUSREPORT)
  #undef _
  return -1;
}

const char *ps_blueprint_cell_repr(int cell) {
  switch (cell) {
    #define _(tag) case PS_BLUEPRINT_CELL_##tag: return #tag;
    _(VACANT)
    _(SOLID)
    _(HOLE)
    _(LATCH)
    _(HEROONLY)
    _(HAZARD)
    _(HEAL)
    _(STATUSREPORT)
    #undef _
  }
  return 0;
}

/* Awards.
 */
 
int ps_award_eval(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_AWARD_##tag;
  _(NONE)
  _(PARTICIPANT)
  _(WALKER)
  _(KILLER)
  _(TRAITOR)
  _(DEATH)
  _(ACTUATOR)
  _(LIVELY)
  _(GHOST)
  _(TREASURE)
  _(LAZY)
  _(PACIFIST)
  _(GENEROUS)
  _(HARD_TARGET)
  #undef _
  return -1;
}

const char *ps_award_repr(int award) {
  switch (award) {
    #define _(tag) case PS_AWARD_##tag: return #tag;
    _(NONE)
    _(PARTICIPANT)
    _(WALKER)
    _(KILLER)
    _(TRAITOR)
    _(DEATH)
    _(ACTUATOR)
    _(LIVELY)
    _(GHOST)
    _(TREASURE)
    _(LAZY)
    _(PACIFIST)
    _(GENEROUS)
    _(HARD_TARGET)
    #undef _
  }
  return 0;
}

const char *ps_award_describe(int award) {
  switch (award) {
    #define _(tag) case PS_AWARD_##tag: return #tag;
    case PS_AWARD_NONE:        return "No award";
    case PS_AWARD_PARTICIPANT: return "Participation Award";
    case PS_AWARD_WALKER:      return "Most Walkative";
    case PS_AWARD_KILLER:      return "Deadliest";
    case PS_AWARD_TRAITOR:     return "Most Dishonorable";
    case PS_AWARD_DEATH:       return "Most Killable";
    case PS_AWARD_ACTUATOR:    return "Button Pusher";
    case PS_AWARD_LIVELY:      return "Most Lively";
    case PS_AWARD_GHOST:       return "Mostly Ghostly";
    case PS_AWARD_TREASURE:    return "Treasure Hog";
    case PS_AWARD_LAZY:        return "Most Lazy";
    case PS_AWARD_PACIFIST:    return "Least Deadly";
    case PS_AWARD_GENEROUS:    return "Most Generous";
    case PS_AWARD_HARD_TARGET: return "Hard Target";
    #undef _
  }
  return 0;
}

/* POI type.
 */

int ps_poi_type_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_BLUEPRINT_POI_##tag;
  _(NOOP)
  _(SPRITE)
  _(TREASURE)
  _(HERO)
  _(BARRIER)
  _(DEATHGATE)
  _(STATUSREPORT)
  _(SUMMONER)
  _(PERMASWITCH)
  #undef _
  return -1;
}

const char *ps_poi_type_repr(int type) {
  switch (type) {
    #define _(tag) case PS_BLUEPRINT_POI_##tag: return #tag;
    _(NOOP)
    _(SPRITE)
    _(TREASURE)
    _(HERO)
    _(BARRIER)
    _(DEATHGATE)
    _(STATUSREPORT)
    _(SUMMONER)
    _(PERMASWITCH)
    #undef _
  }
  return 0;
}
