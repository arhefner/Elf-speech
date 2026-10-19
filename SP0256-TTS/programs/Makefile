SHELL := /bin/bash
CC := elfc

SAY := ../../SP0256-AL2
CTS := ../../CTS256A-AL2

# -M links the smaller stdlib and stdio of ElfC, so that tts fits in 32K.
# -O is not used: the branch relaxation of Link/02 1.14 -r changes the low
# bytes of local addresses in cts.lib and dict.lib (v records), see the
# README.  Add it with a Link/02 that has the fix.
CFLAGS := -M -i ../include -i $(SAY)/include -i $(CTS)/include \
          -l ../lib -l $(SAY)/lib -l $(CTS)/lib

# The operating system to build for:
#   make                    build for Elf/OS
#   make TARGET_OS=elfdos   build for ELF-DOS
TARGET_OS ?= elfos

PROGRAMS := tts

ifeq ($(TARGET_OS),elfdos)
CFLAGS += -E
TARGETS := $(PROGRAMS)
else ifeq ($(TARGET_OS),elfos)
TARGETS := $(PROGRAMS:=.elfos)
else
$(error TARGET_OS must be elfos or elfdos, not '$(TARGET_OS)')
endif

LIBS := ../lib/dict.lib $(CTS)/lib/cts.lib $(SAY)/lib/say.lib
HEADERS := ../include/dict.h $(CTS)/include/cts.h $(SAY)/include/say.h \
           $(SAY)/include/allophones.h

.PHONY: all clean

all: $(TARGETS)

# An Elf/OS program is name.elfos, an ELF-DOS program has no extension
%.elfos: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

%: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.prg *.asm *.lst *.build *.lkb *.sym *.elfos $(PROGRAMS)
