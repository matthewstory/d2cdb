SHELL = /bin/sh

CC = gcc 
CFLAGS = -g
OPTIONAL_CFLAGS = -Wall -O2
REQUIRED_CFLAGS = -std=c99 -I/usr/local/include -L/usr/local/lib -lcdb $(CFLAGS)

INSTALL = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_FLAGS = -o root -g wheel -m 0555
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

source = d2cdb.c
output = d2cdb

.PHONY: all 
all: d2cdb

d2cdb: $(source)
	@echo "Making $@"
	$(CC) $(source) -o $@ $(OPTIONAL_CFLAGS) $(REQUIRED_CFLAGS)

.PHONY: clean
clean:
	@echo "Cleaning $(output)"
	rm -f $(output)

.PHONY: install
install:
	$(INSTALL_PROGRAM) $(INSTALL_FLAGS) $(output) $(DESTDIR)$(bindir)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(bindir)/$(output)
