CCWARN:=-Werror -Wimplicit -Wno-overflow
CCRPI:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
CCINC:=-Isrc -I$(MIDDIR)
LDRPI:=-L/opt/vc/lib -lGLESv2 -lEGL -lbcm_host

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN) $(CCRPI)
OBJC:=
LD:=gcc $(LDRPI)
LDPOST:=-lz -lm -lasound -lpthread

OPT_ENABLE:=bcm alsa evdev genioc

PS_GLSL_VERSION:=100
