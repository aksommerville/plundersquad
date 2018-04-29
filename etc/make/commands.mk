run:$(EXE_MAIN);$(CMD_MAIN)

# This $(subst) is necessary because we can't take '=' through MAKECMDGOALS.
# Maybe that's just a Mac thing, I'd never heard of it before.
# This is for developers' use only, so whatever. Use '+' instead of '=' on the command line.
run-%:$(EXE_MAIN);$(CMD_MAIN) $(subst +,=,$*)

test:$(EXE_TEST);$(CMD_TEST)
test-%:$(EXE_TEST);$(CMD_TEST) $*
edit:$(EXE_EDIT);$(CMD_EDIT) --resources=src/data
edit-%:$(EXE_EDIT);$(CMD_EDIT) --resources=src/data $*

clean:;rm -rf mid out
