# Text to speech with the SP0256-AL2 from a dictionary, for ElfC
#
#   make                    build the library, tts for Elf/OS and the
#                           dictionary
#   make TARGET_OS=elfdos   build them, tts for ELF-DOS
#   make dict               build only the dictionary, sp0256.dic
#   make install            copy dict.lib and dict.h into ElfC
#   make clean              remove the generated files
#
# tts speaks with the say library of ../SP0256-AL2 and converts the words
# that are not in the dictionary with cts.lib of ../CTS256A-AL2, which are
# built first.  The gpio and conio libraries have to be installed in ElfC.

SHELL := /bin/bash

# The folder ElfC is installed in, for the install target
ELFC_DIR ?= /opt/elfc

# The port and the EF line of the SP0256-AL2, see ../SP0256-AL2/libsay/Makefile
SAY_GROUP ?= 1
SAY_INDEX ?= 2
SAY_EF ?= 4

TARGET_OS ?= elfos

# The word files the dictionary is built from
DICT_DIR := ../SP0256-AL2/sp0256_dict

export SAY_GROUP SAY_INDEX SAY_EF TARGET_OS

.PHONY: all libs programs dict install clean

all: programs dict

libs:
	$(MAKE) -C ../SP0256-AL2 libs
	$(MAKE) -C ../CTS256A-AL2/libcts
	$(MAKE) -C libdict

programs: libs
	$(MAKE) -C programs

dict: sp0256.dic

sp0256.dic: tools/mkdict.py $(wildcard $(DICT_DIR)/*.txt) ../SP0256-AL2/include/allophones.h
	python3 tools/mkdict.py -d $(DICT_DIR) -o $@

install: libs
	cp lib/dict.lib $(ELFC_DIR)/lib/
	cp include/dict.h $(ELFC_DIR)/include/

clean:
	$(MAKE) -C libdict clean
	$(MAKE) -C programs clean
	rm -f sp0256.dic
