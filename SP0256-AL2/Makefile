# Software for the SP0256-AL2 speech synthesizer, for ElfC
#
#   make                    build the libraries and the programs for Elf/OS
#   make TARGET_OS=elfdos   build them for ELF-DOS
#   make install            copy the libraries and headers into ElfC
#   make clean              remove the generated files
#
# The gpio and conio libraries have to be installed in ElfC.

SHELL := /bin/bash

# The folder ElfC is installed in, for the install target
ELFC_DIR ?= /opt/elfc

# The port and the EF line of the chip, see libsay/Makefile
SAY_GROUP ?= 1
SAY_INDEX ?= 2
SAY_EF ?= 4

TARGET_OS ?= elfos

export SAY_GROUP SAY_INDEX SAY_EF TARGET_OS

.PHONY: all libs programs install clean

all: programs

libs:
	$(MAKE) -C libsay

programs: libs
	$(MAKE) -C programs

install: libs
	cp lib/say.lib $(ELFC_DIR)/lib/
	cp include/say.h include/allophones.h $(ELFC_DIR)/include/

clean:
	$(MAKE) -C libsay clean
	$(MAKE) -C programs clean
