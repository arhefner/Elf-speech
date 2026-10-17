SHELL := /bin/bash
CC := elfc

CFLAGS := -i ../include -l ../lib

# The operating system to build for:
#   make                    build for Elf/OS
#   make TARGET_OS=elfdos   build for ELF-DOS
TARGET_OS ?= elfos

PROGRAMS := say saytime speak

ifeq ($(TARGET_OS),elfdos)
CFLAGS += -E
TARGETS := $(PROGRAMS)
else ifeq ($(TARGET_OS),elfos)
TARGETS := $(PROGRAMS:=.elfos)
else
$(error TARGET_OS must be elfos or elfdos, not '$(TARGET_OS)')
endif

LIBS := ../lib/say.lib
HEADERS := $(wildcard ../include/*.h)

.PHONY: all clean

all: $(TARGETS)

# An Elf/OS program is name.elfos, an ELF-DOS program has no extension
%.elfos: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

%: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.prg *.asm *.lst *.build *.lkb *.sym *.elfos $(PROGRAMS)
