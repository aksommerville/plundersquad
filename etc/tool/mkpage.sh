#!/bin/bash

EDITOR="open -a /Applications/gedit.app"

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 NAME"
  exit 1
fi

NAME="$1"
D=src/gui/pages/

if ! ( echo "$NAME" | grep -q '^[a-zA-Z_][0-9a-zA-Z_]*$' ) ; then
  echo "Not a C identifier: $NAME"
  exit 1
fi

DSTPATH=$D/ps_page_$NAME.c
SRCPATH=$D/ps_page_skeleton.c

if [ -e $DSTPATH ] ; then
  echo "Already exists: $DSTPATH"
  exit 1
fi

sed -E s/skeleton/$NAME/g $SRCPATH > $DSTPATH

if [ -n "$EDITOR" ] ; then
  $EDITOR $DSTPATH
fi
