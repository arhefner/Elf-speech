SHELL := /bin/bash
CC = elfc

# The ssd1305 library of ELFC-GPIO, for its header
SSD1305_DIR ?= ../../../ELFC-GPIO/examples/ssd1305

CFLAGS = -L -i $(SSD1305_DIR) $(FACE_CFLAGS)

TARGET = face.lib

all: ../lib/$(TARGET)

../lib/$(TARGET): face_init.prg
	mkdir -p ../lib
	cat $^ > $@

face_init.prg: face_init.c face_images.h ../include/face.h ../include/say.h
	$(CC) $(CFLAGS) $<

# The images are made by a Python script, and kept in the repository
face_images.h: ../tools/mkface.py
	python3 $< > $@

clean:
	rm -f *.asm *.lst *.build *.prg ../lib/$(TARGET)
