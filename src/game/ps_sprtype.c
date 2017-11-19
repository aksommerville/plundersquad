#include "ps.h"
#include "ps_sprite.h"

/* Get sprtype by name.
 */

const struct ps_sprtype *ps_sprtype_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!namec) return 0;

  if ((namec==5)&&!memcmp(name,"dummy",5)) return &ps_sprtype_dummy;
  if ((namec==4)&&!memcmp(name,"hero",4)) return &ps_sprtype_hero;
  if ((namec==5)&&!memcmp(name,"anim2",5)) return &ps_sprtype_anim2;
  if ((namec==8)&&!memcmp(name,"hookshot",8)) return &ps_sprtype_hookshot;
  if ((namec==5)&&!memcmp(name,"arrow",5)) return &ps_sprtype_arrow;
  if ((namec==11)&&!memcmp(name,"healmissile",11)) return &ps_sprtype_healmissile;
  if ((namec==9)&&!memcmp(name,"explosion",9)) return &ps_sprtype_explosion;
  if ((namec==6)&&!memcmp(name,"switch",6)) return &ps_sprtype_switch;
  if ((namec==3)&&!memcmp(name,"bug",3)) return &ps_sprtype_bug;
  if ((namec==10)&&!memcmp(name,"seamonster",10)) return &ps_sprtype_seamonster;
  if ((namec==7)&&!memcmp(name,"missile",7)) return &ps_sprtype_missile;
  if ((namec==13)&&!memcmp(name,"treasurechest",13)) return &ps_sprtype_treasurechest;
  if ((namec==9)&&!memcmp(name,"dragonbug",9)) return &ps_sprtype_dragonbug;
  if ((namec==9)&&!memcmp(name,"bumblebat",9)) return &ps_sprtype_bumblebat;
  if ((namec==9)&&!memcmp(name,"blueberry",9)) return &ps_sprtype_blueberry;
  if ((namec==6)&&!memcmp(name,"rabbit",6)) return &ps_sprtype_rabbit;
  if ((namec==5)&&!memcmp(name,"prize",5)) return &ps_sprtype_prize;
  if ((namec==11)&&!memcmp(name,"swordswitch",11)) return &ps_sprtype_swordswitch;
  if ((namec==7)&&!memcmp(name,"lobster",7)) return &ps_sprtype_lobster;

  return 0;
}
