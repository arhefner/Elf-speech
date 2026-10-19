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
 * cts256face.c builds this with FACE defined, as cts256face, which shows
 * the talking face of face.h on the OLED display as it speaks.  Without
 * the face, cts256 is small enough for a 32K Elf: it prints with cputs
 * and not printf, which would add 3K.
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

#ifdef FACE
#include <face.h>

/* the ports of the SPI adapter of the display, as on the 1802/MAX */
#ifndef SPI_GROUP
#define SPI_GROUP  0
#endif
#ifndef SPI_CTL
#define SPI_CTL    2
#endif
#ifndef SPI_DATA
#define SPI_DATA   3
#endif

/* give the chip an allophone, and move the mouth */
#define SPEAK(a)  face_allophone(a)
#define NAME      "cts256face"
#else
#define SPEAK(a)  say_allophone(a)
#define NAME      "cts256"
#endif

/*
 * The size of a line typed at the console.  cts256 reads it with gets,
 * which reads up to 255 characters, and cts256face with cgetline.
 */
#ifdef FACE
#define LINE_SIZE  128
#else
#define LINE_SIZE  256
#endif

/* the history of the lines typed at the console, for cgetline */
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
#ifdef FACE
static char last[LINE_SIZE];
static char history[HISTORY_SIZE];
#endif

/*
 * Give the chip the next allophone if it can take it.  It is called
 * after each allophone and, by cts_step, while it converts.  With the
 * face, the mouth is moved when the chip begins an allophone, also if
 * there is no next one to give it yet.
 */
static int pump(void) {
  if (qr != qw && say_ready()) {
    SPEAK(queue[qr]);
    qr = (qr + 1) & (QUEUE_SIZE - 1);
  }
#ifdef FACE
  else
    face_poll();
#endif
  return 0;
}

/* speak all the allophones that are waiting, and close the mouth */
static void drain(void) {
  while (qr != qw) {
    SPEAK(queue[qr]);
    qr = (qr + 1) & (QUEUE_SIZE - 1);
  }
#ifdef FACE
  face_done();
#endif
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
      cputs("\r\n");
      column = 0;
    }
    cputs(s);
    cputs(" ");
    column += strlen(s) + 1;
  }

  if (speak) {
    n = (qw + 1) & (QUEUE_SIZE - 1);
    if (n == qr) {
      /* the queue is full, wait for the chip */
      SPEAK(queue[qr]);
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
    cputs("\r\n");
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

#ifdef FACE
/*
 * Speak the lines typed at the console, until Ctrl-C.  They are edited
 * and recalled with cgetline of the conio library.
 */
static void feed_console(void) {
  cputs("CTS256A-AL2 text to speech\r\n");
  cputs("Type text to speak, Enter to repeat, Ctrl-C to quit\r\n");

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
#else
/*
 * Speak the lines typed at the console, until an empty one.  They are
 * read with gets, the line input of the operating system, as cgetline
 * would not leave room on a 32K Elf for an exception EPROM image.
 */
static void feed_console(void) {
  cputs("CTS256A-AL2 text to speech\r\n");
  cputs("Type text to speak, an empty line to quit\r\n");

  for (;;) {
    cputs("> ");
    gets(line);
    cputs("\r\n");
    if (line[0] == 0)
      break;
    feed_string(line);
    finish();
    drain();
  }
}
#endif

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

/* print a message about a file */
static void message(char *msg, char *name) {
  cputs(msg);
  cputs(" ");
  cputs(name);
  cputs("\r\n");
}

static void usage(void) {
  cputs("Usage: " NAME " [-x file] [-o file] [-p] [-q] [text ...]\r\n");
  cputs("       " NAME " [-x file] [-o file] [-p] [-q] -f file\r\n");
  cputs("  -f file  speak the text of a file\r\n");
  cputs("  -x file  use the exception words of an EPROM image\r\n");
  cputs("  -o file  write the allophones to a phrase file\r\n");
  cputs("  -p       print the allophones\r\n");
  cputs("  -q       do not speak\r\n");
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
      message(msg, xname);
      return 1;
    }
    cts_init(exceptions);
  }
  else
    cts_init(NULL);
  if (speak) {
#ifdef FACE
    /* the face moves with the speech, so there is none without it */
    face_init(PORT(SPI_GROUP, SPI_CTL), PORT(SPI_GROUP, SPI_DATA));
#endif
    cts_poll(pump);
  }

  if (oname != NULL) {
    out = open(oname, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) {
      message("Can not create", oname);
      return 1;
    }
  }

  if (text != NULL) {
    if (!feed_file(text)) {
      message("File not found:", text);
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
