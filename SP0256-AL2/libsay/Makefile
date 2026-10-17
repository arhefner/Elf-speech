SHELL := /bin/bash
CC = elfc

# The port and the EF line a program uses if it does not call say_init,
# the first parallel port of the 1802/MAX and EF4 unless given:
#   make SAY_GROUP=1 SAY_INDEX=2 SAY_EF=4
SAY_GROUP ?= 1
SAY_INDEX ?= 2
SAY_EF ?= 4

CFLAGS = -L -D SAY_GROUP=$(SAY_GROUP) -D SAY_INDEX=$(SAY_INDEX) -D SAY_EF=$(SAY_EF)

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.prg)
TARGET = say.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p ../lib
	cat $^ > ../lib/$@

%.prg: %.c say_int.h ../include/say.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.asm *.lst *.build $(OBJS) ../lib/$(TARGET)
