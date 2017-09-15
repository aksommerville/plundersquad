#!/bin/bash

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 OUTPUT"
  exit 1
fi

DSTPATH=$1
rm -f $DSTPATH

cat - >>$DSTPATH <<EOF
/* ps_config.h
 * Generated automatically. Do not edit.
 */

#ifndef PS_CONFIG_H
#define PS_CONFIG_H

#define PS_ARCH_macos    1
#define PS_ARCH_raspi    2
#define PS_ARCH_linux    3
#define PS_ARCH_mswin    4

EOF

echo "#define PS_ARCH PS_ARCH_${PS_ARCH}" >>$DSTPATH
echo "#define PS_ARCH_NAME \"${PS_ARCH}\"" >>$DSTPATH
echo "#define PS_CONFIG_NAME \"${PS_CONFIG}\"" >>$DSTPATH
echo "" >>$DSTPATH

for U in $OPT_ENABLE ; do
  echo "#define PS_USE_$U 1" >>$DSTPATH
done
for U in $OPT_DISABLE ; do
  echo "#define PS_USE_$U 0" >>$DSTPATH
done
echo "" >>$DSTPATH

for K in $COMPILE_TIME_CONSTANTS ; do
  echo "#define $K ${!K}" >>$DSTPATH
done

cat - >>$DSTPATH <<EOF

#endif
EOF
