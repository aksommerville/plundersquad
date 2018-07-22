#!/bin/sh

DST=/usr/local

read -p "Install prefix? [$DST] " DST_OVERRIDE || exit 1
if [ -n "$DST_OVERRIDE" ] ; then
  DST="$DST_OVERRIDE"
fi

DST_EXE=$DST/bin/plundersquad
DST_DATA=$DST/share/plundersquad/ps-data
DST_DESKTOP=/usr/share/applications/plundersquad.desktop
DST_INPUT_CONFIG=$DST/share/plundersquad/input.cfg
DST_HIGHSCORES=$DST/share/plundersquad/highscores
DST_GENERAL_CONFIG=$DST/share/plundersquad/plundersquad.cfg
DST_APP_ICON=$DST/share/plundersquad/plundersquad.png

abort_if_exists() {
  if [ -f "$1" ] ; then
    echo "File '$1' already exists. Uninstall first if you want to replace it."
    exit 1
  fi
}

abort_if_exists "$DST_EXE"
abort_if_exists "$DST_DATA"
abort_if_exists "$DST_DESKTOP"
abort_if_exists "$DST_INPUT_CONFIG"
abort_if_exists "$DST_HIGHSCORES"
abort_if_exists "$DST_GENERAL_CONFIG"
abort_if_exists "$DST_APP_ICON"

echo "Will write the following files:"
echo "  $DST_EXE"
echo "  $DST_DATA"
echo "  $DST_DESKTOP"
echo "  $DST_INPUT_CONFIG"
echo "  $DST_HIGHSCORES"
echo "  $DST_GENERAL_CONFIG"
echo "  $DST_APP_ICON"

read -p "Proceed? [Y/n] " PROCEED || exit 1
if [ -z "$PROCEED" ] ; then
  PROCEED=Y
fi
if [ "$PROCEED" != "Y" ] && [ "$PROCEED" != "y" ] ; then
  echo "Aborting."
  exit 0
fi

copyfile() {
  mkdir -p "$(dirname $2)" || exit 1
  cp "$1" "$2" || exit 1
  if [ -n "$SUDO_USER" ] ; then
    chown "$SUDO_USER": "$2" || exit 1
  fi
}

copyfile out/linux-default/plundersquad "$DST_EXE"
copyfile out/linux-default/ps-data "$DST_DATA"
copyfile src/main/plundersquad.desktop "$DST_DESKTOP"
copyfile out/linux-default/input.cfg "$DST_INPUT_CONFIG"
copyfile out/linux-default/highscores "$DST_HIGHSCORES"
copyfile out/linux-default/plundersquad.cfg "$DST_GENERAL_CONFIG"
copyfile src/main/appicon.png "$DST_APP_ICON"

echo "Installed."
