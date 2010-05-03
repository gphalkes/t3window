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
CFLAGS+=-I. -DT3_WINDOW_BUILD_DSO


CFILES.libt3window.la:= window.c terminal.c curses_interface.c convert_output.c misc.c
LDFLAGS.libt3window.la:= -shared -Lunicode
CFLAGS.window:= -D_XOPEN_SOURCE
CFLAGS.misc:= -DUSE_GETTEXT
LDLIBS.libt3window.la:= -lncurses -lt3unicode
CFLAGS.convert_output:= -Iunicode

CFILES.test:= test.c
LDFLAGS.test:= -L.libs
LDLIBS.test:=-lt3window

LD_LIBRARY_PATH:=unicode
export LD_LIBRARY_PATH

TARGETS:= libt3window.la test
#===========================================================================
.PHONY: all clean

all: $(TARGETS)

CFILES:= $(foreach PART, $(TARGETS), $(CFILES.$(PART)))
GFILES:= $(foreach PART, $(TARGETS), $(GFILES.$(PART)))
LFILES:= $(foreach PART, $(TARGETS), $(LFILES.$(PART)))
SOURCES:= $(CFILES) $(GFILES) $(LFILES)

# force use of our pattern rule for lex files
$(foreach FILE, $(LFILES), $(eval $(patsubst %.l, .objects/%.lo, $(FILE)): $(patsubst %.l, .objects/%.c,$(FILE))))

OBJECTS:=$(CFILES:.c=.lo) $(LFILES:.l=.lo) $(GFILES:.g=.lo)
$(foreach PART, $(TARGETS), $(eval OBJECTS.$(PART):= \
	$$(patsubst %.c, .objects/%.lo, $$(CFILES.$(PART))) \
	$$(patsubst %.g, .objects/%.lo, $$(GFILES.$(PART))) \
	$$(patsubst %.l, .objects/%.lo, $$(LFILES.$(PART)))))
DEPENDENCIES:= $(patsubst %, .deps/%, $(SOURCES))

$(foreach PART, $(TARGETS), $(eval $(PART): $$(OBJECTS.$(PART)) ; \
	libtool --mode=link $$(CC) $$(CFLAGS) $$(CFLAGS.$(PART)) $$(LDFLAGS) $$(LDFLAGS.$(PART)) \
		-o $$@ $$^ $$(LDLIBS) $$(LDLIBS.$(PART)) -rpath /usr/lib))
# Add dependency rules for grammar files. Header files generated from grammar
# files are needed by the lexical analyser and other files
$(foreach FILE, $(GFILES), $(if $(DEPS.$(FILE)), $(eval $(patsubst %.c, %.lo, $(patsubst %.l, %.c, $(patsubst %, .objects/%, $(DEPS.$(FILE))))): $(patsubst %.g, .objects/%.h, $(FILE)))))

.objects/%.lo: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	libtool --mode=compile $(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

.objects/%.lo: .objects/%.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	libtool --mode=compile $(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

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
#===========================================================================
test: .libs/libt3window.so
