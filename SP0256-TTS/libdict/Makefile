SHELL := /bin/bash
CC = elfc
ASM = asm02
ASMFLAGS = -q -L

OBJS = dict_lookup.prg dict_scan.prg
TARGET = dict.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p ../lib
	cat $^ > ../lib/$@

dict_lookup.prg: dict_lookup.c ../include/dict.h
	$(CC) -L $<

dict_scan.prg: dict_scan.asm
	$(ASM) $(ASMFLAGS) $<

clean:
	rm -f *.lst *.build dict_lookup.asm $(OBJS) ../lib/$(TARGET)
