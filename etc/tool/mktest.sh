#!/bin/bash

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 [INPUT...] OUTPUT"
  exit 1
fi

LISTPATH=mid/testlist
rm -f $LISTPATH
touch $LISTPATH

while [ "$#" -gt 1 ] ; do
  SRCPATH=$1
  shift 1
  SRCBASE=$(basename $SRCPATH)
  nl -ba -s' ' $SRCPATH | sed -En 's/^ *([0-9]+) *PS_TEST *\( *([0-9a-zA-Z_]+) *(, *([^)]*))?\).*$/'$SRCBASE' \1 \2 \4/p' >>$LISTPATH
done

DSTPATH=$1
rm -f $DSTPATH
touch $DSTPATH

cat - >>$DSTPATH <<EOF
/* ps_test_contents.h
 * Generated file. Do not edit.
 */

#ifndef PS_TEST_CONTENTS_H
#define PS_TEST_CONTENTS_H

struct ps_test {
  int (*fn)();
  const char *name;
  const char *file;
  int line;
  const char *groups;
};

EOF

# Declare all test functions.
while read FNAME LNUM TNAME GRPS ; do
  echo "int $TNAME();" >>$DSTPATH
done < $LISTPATH

cat - >>$DSTPATH <<EOF

static const struct ps_test ps_testv[]={
EOF

while read FNAME LNUM TNAME GRPS ; do
  echo "  {$TNAME,\"$TNAME\",\"$FNAME\",$LNUM,\"$GRPS\"}," >>$DSTPATH
done < $LISTPATH

cat - >>$DSTPATH <<EOF
};

static const int ps_testc=sizeof(ps_testv)/sizeof(struct ps_test);

#endif
EOF

rm -f $LISTPATH
