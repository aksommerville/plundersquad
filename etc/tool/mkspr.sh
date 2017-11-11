#!/bin/bash

EDITOR="open -a /Applications/gedit.app"

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

echo "OK. Remember to add to src/game/ps_sprite.h and src/game/ps_sprtype.c"

if [ -n "$EDITOR" ] ; then
  $EDITOR $DSTPATH
fi
