CCWARN:=-Werror -Wimplicit -Wno-overflow
CCINC:=-Isrc -I$(MIDDIR)

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN)
OBJC:=
LD:=gcc
LDPOST:=-lz -lm -lasound -lpthread -lX11 -lGL

OPT_ENABLE:=glx alsa evdev genioc

PS_GLSL_VERSION:=120
