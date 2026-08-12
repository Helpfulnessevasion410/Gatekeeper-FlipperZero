# Builds the real parsing and grading engine for the host and takes it apart.
#
# Everything Gatekeeper puts on a screen is a rendering of what gk_verdict_grade()
# decided about bytes gk_ndef_parse_tlv() read, and a screenshot cannot vouch
# for either. So the bands, the ceilings, the suffix table and the parser's
# behaviour on deliberately broken tags are checked on every push.
#
#   make -C test          run the tests
#   make -C test dump     regenerate the JSON the README mockups draw from

CC      ?= cc
CFLAGS  := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  += -I ..

ENGINE := ../helpers/gk_url.c ../helpers/gk_ndef.c ../helpers/gk_verdict.c ../helpers/gk_demo.c
HDR    := ../helpers/gk_url.h ../helpers/gk_ndef.h ../helpers/gk_verdict.h ../helpers/gk_demo.h

BIN      := host_engine_test
DUMP_BIN := host_demo_dump

.PHONY: all run dump clean
all: run

$(BIN): host_engine_test.c $(ENGINE) $(HDR)
	$(CC) $(CFLAGS) host_engine_test.c $(ENGINE) -o $@

run: $(BIN)
	./$(BIN)

# Grades the scripted tags with the real engine and writes the JSON the mockup
# renderer draws from, so the README screenshots cannot drift away from what
# the application actually decides.
$(DUMP_BIN): host_demo_dump.c $(ENGINE) $(HDR)
	$(CC) $(CFLAGS) host_demo_dump.c $(ENGINE) -o $@

dump: $(DUMP_BIN)
	./$(DUMP_BIN) > demo_dump.json
	@echo "wrote test/demo_dump.json"

clean:
	rm -rf $(BIN) $(DUMP_BIN) $(BIN).dSYM $(DUMP_BIN).dSYM demo_dump.json
