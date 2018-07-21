#!/bin/sh

echo ""
echo "This script will examine your GLES and EGL libraries to ensure that Plunder Squad can find them."
echo "If Plunder Squad can run already, you don't need this script."
echo "Between the Jessie and Stretch versions of Raspbian, the names of these libraries changed."
echo "Plunder Squad uses the Jessie names."
echo ""

EXPECT_GLES=/opt/vc/lib/libGLESv2.so
EXPECT_EGL=/opt/vc/lib/libEGL.so

OTHER_GLES=/opt/vc/lib/libbrcmGLESv2.so
OTHER_EGL=/opt/vc/lib/libbrcmEGL.so

if [ -f "$EXPECT_GLES" ] ; then
  echo "$EXPECT_GLES: Looks good."
elif [ ! -f "$OTHER_GLES" ] ; then
  echo "ERROR: Can't find $EXPECT_GLES or $OTHER_GLES"
  exit 1
else
  echo "Creating symlink '$EXPECT_GLES' pointing to '$OTHER_GLES'"
  sudo ln -s "$OTHER_GLES" "$EXPECT_GLES" || exit 1
fi

if [ -f "$EXPECT_EGL" ] ; then
  echo "$EXPECT_EGL: Looks good."
elif [ ! -f "$EXPECT_EGL" ] ; then
  echo "ERROR: Can't find $EXPECT_EGL or $OTHER_EGL"
  exit 1
else
  echo "Creating symlink '$EXPECT_EGL' pointing to '$OTHER_EGL'"
  sudo ln -s "$OTHER_EGL" "$EXPECT_EGL" || exit 1
fi

echo "OK, let's play!"
