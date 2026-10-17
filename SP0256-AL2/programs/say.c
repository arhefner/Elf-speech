/*
 * say.c - speak phrase files with the SP0256-AL2
 *
 *   say file [file ...]
 *
 * A phrase file is allophone numbers, one byte each, ended by a byte
 * with bit 7 set or by the end of the file.  The files are spoken one
 * after the other.  There are some in the phrases folder.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <say.h>

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
      say_allophone(buf[i]);
    }
  }

  fclose(f);
  return true;
}

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    printf("Usage: say file [file ...]\n");
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if (!say_file(argv[i])) {
      printf("File not found: %s\n", argv[i]);
      return 1;
    }
  }

  return 0;
}
