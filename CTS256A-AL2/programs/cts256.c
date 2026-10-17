/*
 * cts256.c - text to speech with the SP0256-AL2, as the CTS256A-AL2 does
 *
 *   cts256 [-x file] [-o file] [-p] [-q] [text ...]
 *   cts256 [-x file] [-o file] [-p] [-q] -f file
 *
 * Speaks the text, the text of a file, or the lines typed at the console
 * if there is neither.
 *
 *   -f file   speak the text of a file
 *   -x file   use the exception words of an exception EPROM image
 *   -o file   write the allophones to a phrase file for say, do not speak
 *   -p        print the allophones
 *   -q        do not speak
 *
 * The conversion is in cts.lib, which gives the same allophones as the
 * chip.  The allophones are spoken while the text after them is being
 * converted, as on the chip.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <say.h>
#include <cts.h>

/* the size of a line typed at the console */
#define LINE_SIZE  128

/* the history of the lines typed at the console */
#define HISTORY_SIZE  512

/* the allophones waiting to be spoken */
#define QUEUE_SIZE  256

/* the size of the blocks a file is read and written in */
#define BLOCK_SIZE  256

/* the largest exception EPROM image, an 8K EPROM */
#define MAX_EPROM  0x2000

static byte queue[QUEUE_SIZE];
static int qr, qw;

/* the allophones of a rule */
static byte allo[CTS_MAX];

static bool speak;   /* speak the allophones */
static bool print;   /* print the allophones */
static int column;   /* where printing is on the line */
static int out;      /* the phrase file, or -1 */

/* a block of the text file, and of the phrase file */
static byte block[BLOCK_SIZE];
static byte outblock[BLOCK_SIZE];
static int outlen;

/* the lists of exception words */
static byte *exceptions[27];

static char line[LINE_SIZE];
static char last[LINE_SIZE];
static char history[HISTORY_SIZE];

/*
 * Give the chip the next allophone if it can take it.  It is called
 * between the rules and, by cts_step, while it converts.
 */
static int pump(void) {
  if (qr != qw && say_ready()) {
    say_allophone(queue[qr]);
    qr = (qr + 1) & (QUEUE_SIZE - 1);
  }
  return 0;
}

/* speak all the allophones that are waiting */
static void drain(void) {
  while (qr != qw) {
    say_allophone(queue[qr]);
    qr = (qr + 1) & (QUEUE_SIZE - 1);
  }
}

/* an allophone of the text */
static void emit(byte a) {
  char *s;
  int n;

  if (out >= 0) {
    outblock[outlen++] = a;
    if (outlen == BLOCK_SIZE) {
      write(out, outblock, BLOCK_SIZE);
      outlen = 0;
    }
  }

  if (print) {
    s = say_name(a);
    if (column > 72) {
      printf("\n");
      column = 0;
    }
    printf("%s ", s);
    column += strlen(s) + 1;
  }

  if (speak) {
    n = (qw + 1) & (QUEUE_SIZE - 1);
    if (n == qr) {
      /* the queue is full, wait for the chip */
      say_allophone(queue[qr]);
      qr = (qr + 1) & (QUEUE_SIZE - 1);
    }
    queue[qw] = a;
    qw = n;
    pump();
  }
}

/* convert what can be converted, and speak it */
static void convert(void) {
  int i, n;

  while ((n = cts_step(allo)) >= 0) {
    for (i = 0; i < n; i++)
      emit(allo[i]);
    pump();
  }
}

/* a character of the text */
static void feed(int c) {
  cts_feed(c);
  convert();
}

/* the end of the text */
static void finish(void) {
  cts_finish();
  convert();
  if (print && column > 0) {
    printf("\n");
    column = 0;
  }
}

/* speak a string */
static void feed_string(char *s) {
  while (*s)
    feed(*s++);
}

/*
 * Speak a file, false if it could not be opened.  It is read with read,
 * which takes a third of the time of fread, so the chip does not run out
 * of allophones while a block is read on a slow 1802.
 */
static bool feed_file(char *name) {
  int fd, i, n;

  fd = open(name, O_RDONLY);
  if (fd < 0)
    return false;
  while ((n = read(fd, block, BLOCK_SIZE)) > 0)
    for (i = 0; i < n; i++)
      feed(block[i]);
  close(fd);
  finish();
  return true;
}

/* speak the lines typed at the console, until Ctrl-C */
static void feed_console(void) {
  printf("CTS256A-AL2 text to speech\n");
  printf("Type text to speak, Enter to repeat, Ctrl-C to quit\n");

  cgethistory(history, HISTORY_SIZE);
  for (;;) {
    cputs("> ");
    if (cgetline(line, LINE_SIZE) == CGETLINE_CANCEL)
      break;
    if (line[0] != 0)
      strcpy(last, line);
    feed_string(last);
    finish();
    drain();
  }
}

/* read 16 bits, high byte first */
static unsigned int get16(byte *p) {
  return (p[0] << 8) | p[1];
}

/* the size of a file, or -1 if it can not be opened */
static int file_size(char *name) {
  int fd, n, size;

  fd = open(name, O_RDONLY);
  if (fd < 0)
    return -1;
  size = 0;
  while ((n = read(fd, block, BLOCK_SIZE)) > 0 && size <= MAX_EPROM)
    size += n;
  close(fd);
  return size;
}

/*
 * Load the exception words of an exception EPROM image.  The image starts
 * with the five bytes the chip looks for, and has the addresses of the 27
 * lists of exception words at 0A3.  The EPROM is at a 4K boundary, where
 * the chip looks for it.  Returns NULL, or what is wrong.
 */
static char *load_exceptions(char *name) {
  int fd, size, i;
  byte *rom;
  unsigned int base, a;
  static byte signature[5] = { 0x80, 0x48, 0x28, 0x58, 0x85 };

  size = file_size(name);
  if (size < 0)
    return "File not found:";
  if (size < 0x0a3 + 54 || size > MAX_EPROM)
    return "Not an exception EPROM image:";

  rom = malloc(size);
  if (rom == NULL)
    return "Not enough memory for";
  fd = open(name, O_RDONLY);
  if (fd < 0 || read(fd, rom, size) != size)
    return "Can not read";
  close(fd);

  for (i = 0; i < 5; i++)
    if (rom[i] != signature[i])
      return "Not an exception EPROM image:";

  base = get16(rom + 0x0a3) & 0xf000;
  for (i = 0; i < 27; i++) {
    a = get16(rom + 0x0a3 + 2 * i) - base;
    if (a >= size)
      return "Not an exception EPROM image:";
    exceptions[i] = rom + a;
  }
  return NULL;
}

static void usage(void) {
  printf("Usage: cts256 [-x file] [-o file] [-p] [-q] [text ...]\n");
  printf("       cts256 [-x file] [-o file] [-p] [-q] -f file\n");
  printf("  -f file  speak the text of a file\n");
  printf("  -x file  use the exception words of an EPROM image\n");
  printf("  -o file  write the allophones to a phrase file\n");
  printf("  -p       print the allophones\n");
  printf("  -q       do not speak\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  int i;
  char *s, *text, *xname, *oname, *msg;

  text = NULL;
  xname = NULL;
  oname = NULL;
  out = -1;
  speak = true;

  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    s = argv[i];
    if (s[1] == 0 || s[2] != 0)
      usage();
    switch (s[1]) {
    case 'f':
    case 'F':
      if (++i == argc)
        usage();
      text = argv[i];
      break;
    case 'x':
    case 'X':
      if (++i == argc)
        usage();
      xname = argv[i];
      break;
    case 'o':
    case 'O':
      if (++i == argc)
        usage();
      oname = argv[i];
      speak = false;
      break;
    case 'p':
    case 'P':
      print = true;
      break;
    case 'q':
    case 'Q':
      speak = false;
      break;
    default:
      usage();
    }
  }
  if (text != NULL && i < argc)
    usage();

  if (xname != NULL) {
    msg = load_exceptions(xname);
    if (msg != NULL) {
      printf("%s %s\n", msg, xname);
      return 1;
    }
    cts_init(exceptions);
  }
  else
    cts_init(NULL);
  if (speak)
    cts_poll(pump);

  if (oname != NULL) {
    out = open(oname, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) {
      printf("Can not create %s\n", oname);
      return 1;
    }
  }

  if (text != NULL) {
    if (!feed_file(text)) {
      printf("File not found: %s\n", text);
      return 1;
    }
  }
  else if (i < argc) {
    for (; i < argc; i++) {
      feed_string(argv[i]);
      if (i < argc - 1)
        feed(' ');
    }
    finish();
  }
  else
    feed_console();

  drain();

  if (out >= 0) {
    outblock[outlen++] = SAY_EOP;
    write(out, outblock, outlen);
    close(out);
  }
  return 0;
}
