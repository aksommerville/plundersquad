#!/bin/bash
# Look for a few test-only things that must be disabled before releasing.

if [ -s "$INPUTCFG" ] ; then
  echo "$INPUTCFG: Input configuration should be empty."
  exit 1
fi

while read KEY VALUE ; do
  #echo "KEY:'$KEY', VALUE:'$VALUE'"
  case "$KEY" in
    resources|input|highscores)
        echo "$MAINCFG: Key '$KEY' must be unset."
        exit 1
      ;;
    fullscreen)
        if [ "$VALUE" != "true" ] ; then
          echo "$MAINCFG: Fullscreen must be true."
          exit 1
        fi
      ;;
    music)
        if [ "$VALUE" != "128" ] ; then
          echo "$MAINCFG: Music level must be 128."
          exit 1
        fi
      ;;
    sound)
        if [ "$VALUE" != "255" ] ; then
          echo "$MAINCFG: Sound level must be 255."
          exit 1
        fi
      ;;
    soft-render)
      ;;
    *)
        echo "$MAINCFG: Key '$KEY' was not expected."
        exit 1
      ;;
  esac
done <<<$(
  sed -En 's/^ *([0-9a-zA-Z_.-]+) *= *([^#]+)$/\1 \2/p' $MAINCFG
)

PS_PRODUCTION_STARTUP=$(sed -En 's/^#define PS_PRODUCTION_STARTUP *([0-9])$/\1/p' src/main/ps_developer_setup.h)
PS_PAUSEPAGE_ENABLE_CHEAT_MENU=$(sed -En 's/#define PS_PAUSEPAGE_ENABLE_CHEAT_MENU *([0-9])$/\1/p' src/gui/menus/ps_widget_pausepage.c)
PS_B_TO_SWAP_INPUT=$(sed -En 's/#define PS_B_TO_SWAP_INPUT *([0-9])$/\1/p' src/ps.h)

if [ "$PS_PRODUCTION_STARTUP$PS_PAUSEPAGE_ENABLE_CHEAT_MENU$PS_B_TO_SWAP_INPUT" = "100" ] ; then
  exit 0
fi

echo '!!! Please correct the following errors before releasing:'
if [ "$PS_PRODUCTION_STARTUP" != "1" ] ; then
  echo "src/main/ps_developer_setup.h: PS_PRODUCTION_STARTUP zero"
fi
if [ "$PS_PAUSEPAGE_ENABLE_CHEAT_MENU" != "0" ] ; then
  echo "src/gui/menus/ps_widget_pausepage.c: PS_PAUSEPAGE_ENABLE_CHEAT_MENU nonzero"
fi
if [ "$PS_B_TO_SWAP_INPUT" != "0" ] ; then
  echo "src/ps.h: PS_B_TO_SWAP_INPUT nonzero"
fi

exit 1
