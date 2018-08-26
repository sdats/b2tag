CP     ?= cp
MKDIR  ?= mkdir

PREFIX ?= /usr/local

CFLAGS += -Wall -Wextra -Werror
LDLIBS = -lcrypto

.PHONY: all clean install

# Secondary expansion allows using $@ and co in the dependencies
.SECONDEXPANSION:

all: cshatag

# Don't delete any created directories
.PRECIOUS: $(PREFIX)/%/
$(PREFIX)/%/:
	$(MKDIR) -p $@

$(PREFIX)/%: $$(@F) | $$(@D)/
	$(CP) $< $@

install: $(PREFIX)/bin/cshatag $(PREFIX)/share/man/man1/cshatag.1

clean:
	$(RM) cshatag
