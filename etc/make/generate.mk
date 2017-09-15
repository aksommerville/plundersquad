GENERATED_FILES:=$(addprefix $(MIDDIR)/, \
  ps_config.h \
  ps_test_contents.h \
)

$(MIDDIR)/ps_config.h:etc/tool/mkconfig.sh etc/config/$(PS_CONFIG).mk;$(PRECMD) $< $@
$(MIDDIR)/ps_test_contents.h:etc/tool/mktest.sh $(shell find src/test -name '*.c');$(PRECMD) $^ $@
