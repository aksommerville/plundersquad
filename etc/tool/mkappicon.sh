#!/bin/sh

if [ "$#" -ne 2 ] ; then
  echo "Usage: $0 INPUT OUTPUT"
  exit 1
fi
SRCPATH=$1
DSTPATH=$2

DIMENSIONS="$(echo "$SRCPATH" | sed -E 's/^.*\.([0-9]+)x([0-9]+)\..*$/\1 \2/')"
W=$(echo $DIMENSIONS | cut -d' ' -f1)
H=$(echo $DIMENSIONS | cut -d' ' -f2)

rm -f $DSTPATH

echo "const int ps_appicon_w=$W;" >>$DSTPATH
echo "const int ps_appicon_h=$H;" >>$DSTPATH
echo "const char ps_appicon[$((W*H*4))]={" >>$DSTPATH
hexdump -ve '64/1 "%u," "\n"' $SRCPATH >>$DSTPATH
echo "};" >>$DSTPATH
