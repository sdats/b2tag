INSTALL ?= install
MKDIR   ?= mkdir

PREFIX ?= /usr/local

# Remove a trailing slash (if present)
override PREFIX  := $(PREFIX:/=)
# Append a trailing slash (if not already present)
override DESTDIR := $(addsuffix /,$(DESTDIR:/=))

CFLAGS += -Wall -Wextra -Werror -O2 -D_GNU_SOURCE -DNDEBUG
CFLAGS += $(EXTRA_CFLAGS)
LDLIBS = -lcrypto
LDLIBS += $(EXTRA_LDLIBS)

OBJECTS = cshatag.o file.o hash.o utilities.o xa.o

VERSION ?= $(shell git describe --dirty=+ 2>/dev/null || echo 0.1-nogit)

.PHONY: all clean debug install test

# Secondary expansion allows using $@ and co in the dependencies
.SECONDEXPANSION:

all: cshatag

debug: CFLAGS := -ggdb3 $(filter-out -DNDEBUG, $(CFLAGS))
debug: cshatag

cshatag: $(OBJECTS) | $(OBJECTS:.o=.d)

MAKECMDGOALS ?= all

ifneq ($(filter %.o %.d all cshatag debug install,$(MAKECMDGOALS)),)
include $(wildcard *.d)
endif

%.o %.d: %.c
	$(CC) -MMD $(CFLAGS) -c -o $(@:.d=.o) $<

# If the version string differs from the last build, update the last version
ifneq ($(VERSION),$(shell cat .version 2>/dev/null))
.PHONY: .version
endif
.version:
	echo '$(VERSION)' > $@

# Rebuild the 'version' output any time the version string changes
utilities.o utilities.d: CFLAGS += -DVERSION_STRING='"$(VERSION)"'
utilities.o utilities.d: .version

README: cshatag.1
	MANWIDTH=80 man --nh --nj -l $< > $@

test: cshatag
	./test.sh

# Don't delete any created directories
.PRECIOUS: $(DESTDIR)$(PREFIX)/%/
$(DESTDIR)$(PREFIX)/%/:
	$(MKDIR) -p $@

$(DESTDIR)$(PREFIX)/bin/%: $$(@F) | $$(@D)/
	$(INSTALL) -m0755 $< $@

$(DESTDIR)$(PREFIX)/%: $$(@F) | $$(@D)/
	$(INSTALL) -m0644 $< $@

install: $(DESTDIR)$(PREFIX)/bin/cshatag $(DESTDIR)$(PREFIX)/share/man/man1/cshatag.1

clean:
	$(RM) cshatag .version $(OBJECTS) $(OBJECTS:.o=.d)
