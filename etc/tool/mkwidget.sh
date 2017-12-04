#!/bin/bash

EDITOR="open -a /Applications/gedit.app"

if [ "$#" -ne 2 ] ; then
  echo "Usage: $0 SUBSET NAME"
  exit 1
fi

SUBSET="$1"
NAME="$2"
D=src/gui/$SUBSET
CORED=src/gui/corewidgets

if ! [ -d $D ] ; then
  echo "Undefined widget subset: $SUBSET"
  exit 1
fi

if ! ( echo "$NAME" | grep -q '^[a-zA-Z_][0-9a-zA-Z_]*$' ) ; then
  echo "Not a C identifier: $NAME"
  exit 1
fi

DSTPATH=$D/ps_widget_$NAME.c
SRCPATH=$CORED/ps_widget_skeleton.c

if [ -e $DSTPATH ] ; then
  echo "Already exists: $DSTPATH"
  exit 1
fi

sed -E s/skeleton/$NAME/g $SRCPATH > $DSTPATH

if [ -n "$EDITOR" ] ; then
  $EDITOR $DSTPATH
fi
