CCWARN:=-Werror -Wimplicit -Wno-overflow
CCINC:=-Isrc -I$(MIDDIR)
WINDEF:=-m32 -mwindows -D_WIN32_WINNT=0x0501 -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -DPS_NO_OPENGL2=1

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN) $(WINDEF)
OBJC:=
AR:=ar rc
LD:=gcc -m32 -mwindows
LDPOST:=-lz -lglew32s -lopengl32 -lwinmm -lpthread

OPT_ENABLE:=mswm msaudio mshid genioc

PS_GLSL_VERSION:=120

EXE_MAIN:=$(OUTDIR)/plundersquad.exe
EXE_TEST:=$(OUTDIR)/test.exe
EXE_EDIT:=$(OUTDIR)/edit.exe
EXE_RESPACK:=$(OUTDIR)/respack.exe
