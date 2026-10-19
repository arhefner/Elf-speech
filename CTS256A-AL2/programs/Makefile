SHELL := /bin/bash
CC := elfc

SAY := ../../SP0256-AL2

# The ssd1305 library of ELFC-GPIO, for the talking face
SSD1305_DIR ?= ../../../ELFC-GPIO/examples/ssd1305

# The ports of the SPI adapter of the display, as on the 1802/MAX
SPI_GROUP ?= 0
SPI_CTL ?= 2
SPI_DATA ?= 3

CFLAGS := -i ../include -i $(SAY)/include -l ../lib -l $(SAY)/lib \
          -i $(SSD1305_DIR) -l $(SSD1305_DIR) \
          -D SPI_GROUP=$(SPI_GROUP) -D SPI_CTL=$(SPI_CTL) -D SPI_DATA=$(SPI_DATA)

# The operating system to build for:
#   make                    build for Elf/OS
#   make TARGET_OS=elfdos   build for ELF-DOS
TARGET_OS ?= elfos

PROGRAMS := cts256 cts256face

ifeq ($(TARGET_OS),elfdos)
CFLAGS += -E
TARGETS := $(PROGRAMS)
else ifeq ($(TARGET_OS),elfos)
TARGETS := $(PROGRAMS:=.elfos)
else
$(error TARGET_OS must be elfos or elfdos, not '$(TARGET_OS)')
endif

LIBS := ../lib/cts.lib $(SAY)/lib/say.lib $(SAY)/lib/face.lib
HEADERS := ../include/cts.h $(SAY)/include/say.h $(SAY)/include/face.h

.PHONY: all clean

all: $(TARGETS)

# An Elf/OS program is name.elfos, an ELF-DOS program has no extension
%.elfos: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

%: %.c $(LIBS) $(HEADERS)
	$(CC) $(CFLAGS) $<

# cts256face is cts256.c with the talking face
cts256face.elfos cts256face: cts256.c

clean:
	rm -f *.prg *.asm *.lst *.build *.lkb *.sym *.elfos $(PROGRAMS)
