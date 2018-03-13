#!/bin/bash

case $(uname -s) in
  Darwin) EDITOR="open -a /Applications/gedit.app" ;;
  Linux) EDITOR="setsid pluma" ;;
esac

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 NAME"
  exit 1
fi

NAME="$1"
D=src/game/sprites/

if ! ( echo "$NAME" | grep -q '^[a-zA-Z_][0-9a-zA-Z_]*$' ) ; then
  echo "Not a C identifier: $NAME"
  exit 1
fi

DSTPATH=$D/ps_sprite_$NAME.c
SRCPATH=$D/ps_sprite_dummy.c

if [ -e $DSTPATH ] ; then
  echo "Already exists: $DSTPATH"
  exit 1
fi

sed -E s/dummy/$NAME/g $SRCPATH > $DSTPATH

# Took some help from Stack Overflow to automatically rewrite the sources:
# https://stackoverflow.com/questions/6111679/insert-linefeed-in-sed-mac-os-x

sed -E '/^\/\/INSERT SPRTYPE DEFINITION HERE$/{x;s/$/extern const struct ps_sprtype ps_sprtype_'$NAME'\;/;G;}' src/game/ps_sprite.h >mkspr-temp
mv mkspr-temp src/game/ps_sprite.h

NAMELEN=${#NAME}
sed -E '/^\/\/INSERT SPRTYPE NAME TEST HERE$/{x;s/$/  if ((namec=='$NAMELEN')\&\&!memcmp(name,"'$NAME'",'$NAMELEN')) return \&ps_sprtype_'$NAME'\;/;G;}' src/game/ps_sprtype.c >mkspr-temp
mv mkspr-temp src/game/ps_sprtype.c

sed -E '/^\/\/INSERT SPRTYPE REFERENCE HERE$/{x;s/$/  \&ps_sprtype_'$NAME',/;G;}' src/game/ps_sprtype.c >mkspr-temp
mv mkspr-temp src/game/ps_sprtype.c

if [ -n "$EDITOR" ] ; then
  $EDITOR $DSTPATH
fi
