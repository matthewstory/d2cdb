SHELL = /bin/sh

CC = gcc 
CFLAGS = -g
OPTIONAL_CFLAGS = -Wall -O2
REQUIRED_CFLAGS = -std=c99 -I/usr/local/include -L/usr/local/lib -lcdb $(CFLAGS)
# CDEBUG = -DDEBUG
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
