# Copyright (C) 2007-2009 G.P. Halkes

#===========================================================================
VERSION?=debug
CC:=gcc
ifdef COVERAGE
	COVERAGEFLAGS:=-fprofile-arcs -ftest-coverage
endif
ifeq ($(VERSION),debug)
	CFLAGS:=-Wall -W -ggdb -DDEBUG -Wswitch-default \
		-Wcast-align -Wbad-function-cast \
		-Wcast-qual -Wwrite-strings -Wstrict-prototypes \
		$(COVERAGEFLAGS)
else
	CFLAGS:=-Wall -W $(ANSIFLAGS) -O2
endif
CFLAGS+=-I. -I.objects -fPIC


CFILES.libwindow.so:= window.c terminal.c curses_interface.c
LDFLAGS.libwindow.so:= -shared
CFLAGS.window:= -D_XOPEN_SOURCE

TARGETS:= libwindow.so
#===========================================================================
.PHONY: all clean

all: $(TARGETS)

CFILES:= $(foreach PART, $(TARGETS), $(CFILES.$(PART)))
GFILES:= $(foreach PART, $(TARGETS), $(GFILES.$(PART)))
LFILES:= $(foreach PART, $(TARGETS), $(LFILES.$(PART)))
SOURCES:= $(CFILES) $(GFILES) $(LFILES)

# force use of our pattern rule for lex files
$(foreach FILE, $(LFILES), $(eval $(patsubst %.l, .objects/%.o, $(FILE)): $(patsubst %.l, .objects/%.c,$(FILE))))

OBJECTS:=$(CFILES:.c=.o) $(LFILES:.l=.o) $(GFILES:.g=.o)
$(foreach PART, $(TARGETS), $(eval OBJECTS.$(PART):= \
	$$(patsubst %.c, .objects/%.o, $$(CFILES.$(PART))) \
	$$(patsubst %.g, .objects/%.o, $$(GFILES.$(PART))) \
	$$(patsubst %.l, .objects/%.o, $$(LFILES.$(PART)))))
DEPENDENCIES:= $(patsubst %, .deps/%, $(SOURCES))

$(foreach PART, $(TARGETS), $(eval $(PART): $$(OBJECTS.$(PART)) ; \
	$$(CC) $$(CFLAGS) $$(CFLAGS.$(PART)) $$(LDFLAGS) $$(LDFLAGS.$(PART)) \
		-o $$@ $$^ $$(LDLIBS) $$(LDLIBS.$(PART))))
# Add dependency rules for grammar files. Header files generated from grammar
# files are needed by the lexical analyser and other files
$(foreach FILE, $(GFILES), $(if $(DEPS.$(FILE)), $(eval $(patsubst %.c, %.o, $(patsubst %.l, %.c, $(patsubst %, .objects/%, $(DEPS.$(FILE))))): $(patsubst %.g, .objects/%.h, $(FILE)))))

.objects/%.o: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@

.objects/%.o: .objects/%.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@

.objects/%.c .objects/%.h: %.g
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	LLnextgen --base-name=.objects/$* $<

.objects/%.c: %.l
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	flex -o $@ $<

clean:
	rm -rf $(TARGETS) .deps .objects >/dev/null 2>&1

-include $(DEPENDENCIES)
