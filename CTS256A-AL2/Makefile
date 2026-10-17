# Text to speech with the SP0256-AL2, as the CTS256A-AL2 does, for ElfC
#
#   make                    build the library and the program for Elf/OS
#   make TARGET_OS=elfdos   build them for ELF-DOS
#   make install            copy the library and its header into ElfC
#   make clean              remove the generated files
#
# The program speaks with the say library of ../SP0256-AL2, which is
# built first.  The gpio and conio libraries have to be installed in ElfC.

SHELL := /bin/bash

# The folder ElfC is installed in, for the install target
ELFC_DIR ?= /opt/elfc

# The port and the EF line of the SP0256-AL2, see ../SP0256-AL2/libsay/Makefile
SAY_GROUP ?= 1
SAY_INDEX ?= 2
SAY_EF ?= 4

TARGET_OS ?= elfos

export SAY_GROUP SAY_INDEX SAY_EF TARGET_OS

.PHONY: all libs programs install clean

all: programs

libs:
	$(MAKE) -C ../SP0256-AL2 libs
	$(MAKE) -C libcts

programs: libs
	$(MAKE) -C programs

install: libs
	cp lib/cts.lib $(ELFC_DIR)/lib/
	cp include/cts.h $(ELFC_DIR)/include/

clean:
	$(MAKE) -C libcts clean
	$(MAKE) -C programs clean
