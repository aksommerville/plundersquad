GENERATED_FILES:=$(addprefix $(MIDDIR)/, \
  ps_config.h \
  ps_test_contents.h \
  video/ps_appicon.c \
)

$(MIDDIR)/ps_config.h:etc/tool/mkconfig.sh etc/config/$(PS_CONFIG).mk;$(PRECMD) $< $@
$(MIDDIR)/ps_test_contents.h:etc/tool/mktest.sh $(shell find src/test -name '*.c');$(PRECMD) $^ $@
$(MIDDIR)/video/ps_appicon.c:etc/tool/mkappicon.sh src/video/appicon.*.rgba;$(PRECMD) $^ $@
