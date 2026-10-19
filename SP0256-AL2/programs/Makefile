SHELL := /bin/bash
CC := elfc

# The ssd1305 library of ELFC-GPIO, for sayface
SSD1305_DIR ?= ../../../ELFC-GPIO/examples/ssd1305

# The ports of the SPI adapter of the display, as on the 1802/MAX
SPI_GROUP ?= 0
SPI_CTL ?= 2
SPI_DATA ?= 3

CFLAGS := -i ../include -l ../lib -i $(SSD1305_DIR) -l $(SSD1305_DIR) \
          -D SPI_GROUP=$(SPI_GROUP) -D SPI_CTL=$(SPI_CTL) -D SPI_DATA=$(SPI_DATA)

# The operating system to build for:
#   make                    build for Elf/OS
#   make TARGET_OS=elfdos   build for ELF-DOS
TARGET_OS ?= elfos

PROGRAMS := say saytime speak calculate sayface

ifeq ($(TARGET_OS),elfdos)
CFLAGS += -E
TARGETS := $(PROGRAMS)
else ifeq ($(TARGET_OS),elfos)
TARGETS := $(PROGRAMS:=.elfos)
else
$(error TARGET_OS must be elfos or elfdos, not '$(TARGET_OS)')
endif

LIBS := ../lib/say.lib ../lib/face.lib
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
