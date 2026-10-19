/*
 * sayface.c - speak phrase files with a talking face on the OLED display
 *
 *   sayface file [file ...]
 *
 * As say, but the face of face.h is drawn on an SSD1305 display on the
 * SPI adapter, and its mouth moves with the allophones.  A phrase file
 * is allophone numbers, one byte each, ended by a byte with bit 7 set or
 * by the end of the file.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 *
 * The ports of the SPI adapter are those of the 1802/MAX unless the
 * makefile is told otherwise:
 *
 *   make SPI_GROUP=0 SPI_CTL=2 SPI_DATA=3
 */
#include <stdio.h>
#include <face.h>

#ifndef SPI_GROUP
#define SPI_GROUP  0
#endif
#ifndef SPI_CTL
#define SPI_CTL    2
#endif
#ifndef SPI_DATA
#define SPI_DATA   3
#endif

#define BUF_SIZE  256

static byte buf[BUF_SIZE];

/* speak a file, false if it could not be opened */
static bool say_file(char *name) {
  FILE *f;
  int n, i;

  f = fopen(name, "r");
  if (f == NULL)
    return false;

  while ((n = fread(buf, 1, BUF_SIZE, f)) > 0) {
    for (i = 0; i < n; i++) {
      if (buf[i] & 0x80) {
        fclose(f);
        return true;
      }
      face_allophone(buf[i]);
    }
  }

  fclose(f);
  return true;
}

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    printf("Usage: sayface file [file ...]\n");
    return 1;
  }

  face_init(PORT(SPI_GROUP, SPI_CTL), PORT(SPI_GROUP, SPI_DATA));

  for (i = 1; i < argc; i++) {
    if (!say_file(argv[i])) {
      face_done();
      printf("File not found: %s\n", argv[i]);
      return 1;
    }
  }

  face_done();
  return 0;
}
