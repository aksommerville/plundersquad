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
  if ((namec==9)&&!memcmp(name,"fireworks",9)) return &ps_sprtype_fireworks;
  if ((namec==10)&&!memcmp(name,"bloodhound",10)) return &ps_sprtype_bloodhound;
  if ((namec==6)&&!memcmp(name,"turtle",6)) return &ps_sprtype_turtle;
  if ((namec==6)&&!memcmp(name,"dragon",6)) return &ps_sprtype_dragon;
  if ((namec==5)&&!memcmp(name,"toast",5)) return &ps_sprtype_toast;
  if ((namec==4)&&!memcmp(name,"bomb",4)) return &ps_sprtype_bomb;
  if ((namec==8)&&!memcmp(name,"skeleton",8)) return &ps_sprtype_skeleton;
  if ((namec==6)&&!memcmp(name,"flames",6)) return &ps_sprtype_flames;
  if ((namec==8)&&!memcmp(name,"killozap",8)) return &ps_sprtype_killozap;
  if ((namec==10)&&!memcmp(name,"friedheart",10)) return &ps_sprtype_friedheart;
  if ((namec==13)&&!memcmp(name,"heroindicator",13)) return &ps_sprtype_heroindicator;
  if ((namec==7)&&!memcmp(name,"giraffe",7)) return &ps_sprtype_giraffe;
  if ((namec==3)&&!memcmp(name,"yak",3)) return &ps_sprtype_yak;
  if ((namec==7)&&!memcmp(name,"chicken",7)) return &ps_sprtype_chicken;
  if ((namec==3)&&!memcmp(name,"egg",3)) return &ps_sprtype_egg;
  if ((namec==12)&&!memcmp(name,"motionsensor",12)) return &ps_sprtype_motionsensor;
  if ((namec==7)&&!memcmp(name,"gorilla",7)) return &ps_sprtype_gorilla;
  if ((namec==7)&&!memcmp(name,"penguin",7)) return &ps_sprtype_penguin;
  if ((namec==5)&&!memcmp(name,"snake",5)) return &ps_sprtype_snake;
  if ((namec==10)&&!memcmp(name,"teleporter",10)) return &ps_sprtype_teleporter;
  if ((namec==11)&&!memcmp(name,"boxingglove",11)) return &ps_sprtype_boxingglove;
  if ((namec==7)&&!memcmp(name,"lwizard",7)) return &ps_sprtype_lwizard;
  if ((namec==8)&&!memcmp(name,"elefence",8)) return &ps_sprtype_elefence;
  if ((namec==8)&&!memcmp(name,"conveyor",8)) return &ps_sprtype_conveyor;
  if ((namec==7)&&!memcmp(name,"shooter",7)) return &ps_sprtype_shooter;
  if ((namec==9)&&!memcmp(name,"teleffect",9)) return &ps_sprtype_teleffect;
  if ((namec==5)&&!memcmp(name,"inert",5)) return &ps_sprtype_inert;
  if ((namec==11)&&!memcmp(name,"edgecrawler",11)) return &ps_sprtype_edgecrawler;
  if ((namec==5)&&!memcmp(name,"mimic",5)) return &ps_sprtype_mimic;
  if ((namec==6)&&!memcmp(name,"piston",6)) return &ps_sprtype_piston;
  if ((namec==11)&&!memcmp(name,"multipiston",11)) return &ps_sprtype_multipiston;
//INSERT SPRTYPE NAME TEST HERE

  return 0;
}

/* List of all sprtype.
 */

const struct ps_sprtype *ps_all_sprtypes[]={
  &ps_sprtype_dummy,
  &ps_sprtype_hero,
  &ps_sprtype_anim2,
  &ps_sprtype_hookshot,
  &ps_sprtype_arrow,
  &ps_sprtype_healmissile,
  &ps_sprtype_explosion,
  &ps_sprtype_switch,
  &ps_sprtype_bug,
  &ps_sprtype_seamonster,
  &ps_sprtype_missile,
  &ps_sprtype_treasurechest,
  &ps_sprtype_dragonbug,
  &ps_sprtype_bumblebat,
  &ps_sprtype_blueberry,
  &ps_sprtype_rabbit,
  &ps_sprtype_prize,
  &ps_sprtype_swordswitch,
  &ps_sprtype_lobster,
  &ps_sprtype_fireworks,
  &ps_sprtype_bloodhound,
  &ps_sprtype_turtle,
  &ps_sprtype_dragon,
  &ps_sprtype_toast,
  &ps_sprtype_bomb,
  &ps_sprtype_skeleton,
  &ps_sprtype_flames,
  &ps_sprtype_killozap,
  &ps_sprtype_friedheart,
  &ps_sprtype_heroindicator,
  &ps_sprtype_giraffe,
  &ps_sprtype_yak,
  &ps_sprtype_chicken,
  &ps_sprtype_egg,
  &ps_sprtype_motionsensor,
  &ps_sprtype_gorilla,
  &ps_sprtype_penguin,
  &ps_sprtype_snake,
  &ps_sprtype_teleporter,
  &ps_sprtype_boxingglove,
  &ps_sprtype_lwizard,
  &ps_sprtype_elefence,
  &ps_sprtype_conveyor,
  &ps_sprtype_shooter,
  &ps_sprtype_teleffect,
  &ps_sprtype_inert,
  &ps_sprtype_edgecrawler,
  &ps_sprtype_mimic,
  &ps_sprtype_piston,
  &ps_sprtype_multipiston,
//INSERT SPRTYPE REFERENCE HERE
0};
