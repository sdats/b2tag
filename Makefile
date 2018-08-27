CP     ?= cp
MKDIR  ?= mkdir

PREFIX ?= /usr/local

CFLAGS += -MMD -Wall -Wextra -Werror -O2 -D_GNU_SOURCE
CFLAGS += $(EXTRA_CFLAGS)
LDLIBS = -lcrypto

OBJECTS = cshatag.o hash.o version.o xa.o

VERSION ?= $(shell git describe --dirty=+ 2>/dev/null || echo 0.1-nogit)

.PHONY: all clean install

# Secondary expansion allows using $@ and co in the dependencies
.SECONDEXPANSION:

all: cshatag

include *.d

cshatag: $(OBJECTS)

# If the version string differs from the last build, update the last version
ifneq ($(VERSION),$(shell cat .version 2>/dev/null))
.PHONY: .version
endif
.version:
	echo '$(VERSION)' > $@

# Rebuild the 'version' output any time the version string changes
version.o : CFLAGS += -DVERSION_STRING='"$(VERSION)"'
version.o : .version

# Don't delete any created directories
.PRECIOUS: $(PREFIX)/%/
$(PREFIX)/%/:
	$(MKDIR) -p $@

$(PREFIX)/%: $$(@F) | $$(@D)/
	$(CP) $< $@

install: $(PREFIX)/bin/cshatag $(PREFIX)/share/man/man1/cshatag.1

clean:
	$(RM) cshatag .version $(OBJECTS)
