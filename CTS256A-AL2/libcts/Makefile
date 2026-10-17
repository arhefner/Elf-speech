SHELL := /bin/bash
ASM = asm02
ASMFLAGS = -q -L

SRCS = $(wildcard *.asm)
OBJS = $(SRCS:.asm=.prg)
TARGET = cts.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p ../lib
	cat $^ > ../lib/$@

%.prg: %.asm rules.inc
	$(ASM) $(ASMFLAGS) $<

clean:
	rm -f *.lst *.build $(OBJS) ../lib/$(TARGET)
