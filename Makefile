all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

include etc/make/configure.mk
include etc/make/generate.mk
include etc/make/build.mk
include etc/make/commands.mk
