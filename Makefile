CP     ?= cp
MKDIR  ?= mkdir

PREFIX ?= /usr/local

CFLAGS += -MMD -Wall -Wextra -Werror -O2 -D_GNU_SOURCE $(EXTRA_CFLAGS)
LDLIBS = -lcrypto

OBJECTS = cshatag.o hash.o xa.o

.PHONY: all clean install

# Secondary expansion allows using $@ and co in the dependencies
.SECONDEXPANSION:

all: cshatag

include *.d

cshatag: $(OBJECTS)

# Don't delete any created directories
.PRECIOUS: $(PREFIX)/%/
$(PREFIX)/%/:
	$(MKDIR) -p $@

$(PREFIX)/%: $$(@F) | $$(@D)/
	$(CP) $< $@

install: $(PREFIX)/bin/cshatag $(PREFIX)/share/man/man1/cshatag.1

clean:
	$(RM) cshatag $(OBJECTS)
