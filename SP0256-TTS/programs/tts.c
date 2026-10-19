/*
 * tts.c - text to speech with the SP0256-AL2, from a dictionary
 *
 *   tts [-d file] [-o file] [-p] [-q] [-u] [text ...]
 *   tts [-d file] [-o file] [-p] [-q] [-u] -f file
 *
 * Speaks the text, the text of a file, or the lines typed at the console
 * if there is neither.
 *
 *   -d file   the dictionary, sp0256.dic in the current folder or in the
 *             root folder if not given
 *   -f file   speak the text of a file
 *   -o file   write the allophones to a phrase file for say, do not speak
 *   -p        print the allophones
 *   -q        do not speak
 *   -u        print the words that are not in the dictionary
 *
 * The words are looked up in the dictionary that tools/mkdict.py makes,
 * with dict.lib, and a word that is not there is converted with the
 * letter to sound rules of the CTS256A-AL2, with cts.lib.  The allophones
 * are spoken while the next words are looked up.
 *
 * The program is written to be small, so that it fits in 32K under
 * ELF-DOS: it prints with cputs and not printf, and the characters of the
 * text are classed with a table.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <say.h>
#include <allophones.h>
#include <cts.h>
#include <dict.h>

/* the allophones waiting to be spoken, about ten seconds of speech */
#define QUEUE_SIZE  128

/*
 * The size of the blocks the text file is read in, and of a line typed at
 * the console, which gets reads into the same buffer, and of the blocks
 * the phrase file is written in.
 */
#define BLOCK_SIZE  256
#define OUT_SIZE    128

/*
 * The pauses between words, from the characters between them.  The
 * longest pause of the characters is made:
 *
 *   P_HYPHEN  -                  PA1
 *   P_SPACE   spaces and quotes  PA2
 *   P_OTHER   the others         PA3
 *   P_COMMA   , ;                PA4
 *   P_COLON   :                  PA5
 *   P_STOP    . ! ?              PA5 PA5
 */
#define P_NONE    0
#define P_HYPHEN  1
#define P_SPACE   2
#define P_OTHER   3
#define P_COMMA   4
#define P_COLON   5
#define P_STOP    6

/*
 * The size of the stack of the program.  The runtime of -M has a stack of
 * 63 bytes, which a word converted by the rules nearly fills with the poll
 * functions of cts.lib, and a read of the dictionary under ELF-DOS needs
 * room for the kernel too, so main moves the stack here.
 */
#define STACK_SIZE  256

/* numbers of up to this many digits are said as words */
#define NUMBER_DIGITS  9

/*
 * The class of each character from space to ~: w for one that is in a
 * word, the letters, the digits and ', 1 to 6 for the pause of one
 * between words, and a to g for a symbol that is said as a word.
 */
static char classes[] =
  "262dcbaw223e4163wwwwwwwwww543f36"
  "gwwwwwwwwwwwwwwwwwwwwwwwwww23233"
  "3wwwwwwwwwwwwwwwwwwwwwwwwww2323";

/* the words of the symbols a to g: & % $ # + = @ */
static char *symbols[] = {
  "AND", "PERCENT", "DOLLARS", "NUMBER", "PLUS", "EQUALS", "AT"
};

/* the allophones of the pauses, P_HYPHEN to P_STOP, which is two PA5 */
static byte pauses[] = { PA1, PA1, PA2, PA3, PA4, PA5, PA5 };

/*
 * The last allophones of a word that are followed by IH ZZ for an S, and
 * those followed by SS.  An S is ZZ after the others.
 */
static char hissing[] = { SS, ZZ, SH, ZH, CH, JH, 0 };
static char voiceless[] = { PP, TT1, TT2, KK1, KK2, KK3, FF, TH, 0 };

static char *ones[] = {
  "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT",
  "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
  "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"
};

static char *tens[] = {
  "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY",
  "NINETY"
};

static char *scales[] = { "HUNDRED", "THOUSAND", "MILLION" };

static byte queue[QUEUE_SIZE];
static int qr, qw;

/* the allophones of a word or of a rule */
static byte allo[CTS_MAX];

static bool speak;     /* speak the allophones */
static bool print;     /* print the allophones */
static bool unknown;   /* print the words that are not in the dictionary */
static int column;     /* where printing is on the line */
static int out;        /* the phrase file, or -1 */

/* a block of the text file or a typed line, and of the phrase file */
static byte block[BLOCK_SIZE];
static byte outblock[OUT_SIZE];
static int outlen;

/* the word being read, and a copy of it */
static char word[DICT_WORD_MAX + 1];
static char other[DICT_WORD_MAX + 1];
static int wlen;
static bool digits;    /* the word is only digits */
static bool spell;     /* say its digits one at a time, after a point */
static int pend;       /* a , or . after digits, which may be in a number */

static int pause;      /* the pause before the next word */
static bool spoken;    /* a word has been said */

/* the stack, and the stack pointer of the runtime */
static byte stack[STACK_SIZE];
static int runtime_sp;

/* give the chip the next allophone of the queue */
static void speak_next(void) {
  say_allophone(queue[qr]);
  qr = (qr + 1) & (QUEUE_SIZE - 1);
}

/* Give the chip the next allophone if it can take it. */
static int pump(void) {
  if (qr != qw && say_ready())
    speak_next();
  return 0;
}

/* speak all the allophones that are waiting */
static void drain(void) {
  while (qr != qw)
    speak_next();
}

/* end the line of printed allophones */
static void newline(void) {
  if (column > 0) {
    cputs("\r\n");
    column = 0;
  }
}

/* an allophone of the text */
static void emit(byte a) {
  char *s;
  int n;

  if (out >= 0) {
    outblock[outlen++] = a;
    if (outlen == OUT_SIZE) {
      write(out, outblock, OUT_SIZE);
      outlen = 0;
    }
  }

  if (print) {
    s = say_name(a);
    if (column > 72)
      newline();
    cputs(s);
    cputs(" ");
    column += strlen(s) + 1;
  }

  if (speak) {
    n = (qw + 1) & (QUEUE_SIZE - 1);
    if (n == qr)
      speak_next();      /* the queue is full, wait for the chip */
    queue[qw] = a;
    qw = n;
    pump();
  }
}

/* make a pause at least as long as p before the next word */
static void add_pause(int p) {
  if (pause < p)
    pause = p;
}

/* make the pause before a word, if a word was said before it */
static void begin_word(void) {
  if (spoken && pause != P_NONE) {
    emit(pauses[pause]);
    if (pause == P_STOP)
      emit(PA5);
  }
  pause = P_NONE;
  spoken = true;
}

/*
 * Say a word from the dictionary, with an S after it if s is true.  False
 * if it is not there.
 */
static bool say_dict(char *w, bool s) {
  int i, n;
  byte a;

  n = dict_lookup(w, allo);
  if (n == 0)
    return false;
  begin_word();
  for (i = 0; i < n; i++)
    emit(allo[i]);
  if (s) {
    a = allo[n - 1];
    if (strchr(hissing, a) != NULL) {
      emit(IH);
      emit(ZZ);
    }
    else if (strchr(voiceless, a) != NULL)
      emit(SS);
    else
      emit(ZZ);
  }
  return true;
}

/*
 * Say a word with the rules of the CTS256A-AL2.  The text of the rules
 * ends with a CR, which is a PA3 that is left out.
 */
static void say_by_rules(char *w) {
  int i, n, held;

  if (unknown) {
    newline();
    cputs(w);
    cputs("\r\n");
  }
  begin_word();
  cts_init(NULL);
  while (*w)
    cts_feed(*w++);
  cts_finish();
  held = -1;
  while ((n = cts_step(allo)) >= 0) {
    for (i = 0; i < n; i++) {
      if (held >= 0)
        emit(held);
      held = allo[i];
    }
  }
  if (held != PA3)
    emit(held);
}

/*
 * Say a word of letters, digits and apostrophes, in upper case.  A word
 * with 'S that is not in the dictionary is the word before the 'S and an
 * S, and other apostrophes are left out if that finds the word.
 */
static void say_word(char *w) {
  int len;
  char *s, *d;

  if (say_dict(w, false))
    return;
  if (strchr(w, '\'') != NULL) {
    len = strlen(w);
    strcpy(other, w);
    if (len > 2 && w[len - 2] == '\'' && w[len - 1] == 'S') {
      other[len - 2] = 0;
      if (say_dict(other, true))
        return;
    }
    d = other;
    for (s = w; *s; s++)
      if (*s != '\'')
        *d++ = *s;
    *d = 0;
    if (other[0] != 0 && say_dict(other, false))
      return;
  }
  say_by_rules(w);
}

/* say a word of a number, with a space before it */
static void say_number_word(char *w) {
  add_pause(P_SPACE);
  say_word(w);
}

/* say a number from 1 to 999 */
static void say_group(int n) {
  if (n >= 100) {
    say_number_word(ones[n / 100]);
    say_number_word(scales[0]);
    n = n % 100;
  }
  if (n >= 20) {
    say_number_word(tens[n / 10 - 2]);
    n = n % 10;
  }
  if (n > 0)
    say_number_word(ones[n]);
}

/*
 * Say a number of digits as words, as "one million two hundred thousand
 * five", three digits at a time.  A number with too many digits, or that
 * starts with 0 and is not 0, or that is after a decimal point, is said a
 * digit at a time.
 */
static void say_number(char *w) {
  int len, n, k, d;
  bool zero;

  len = strlen(w);
  if (spell || len > NUMBER_DIGITS || (len > 1 && w[0] == '0')) {
    while (*w) {
      d = *w++ - '0';
      say_number_word(ones[d]);
    }
    return;
  }
  zero = true;
  while (len > 0) {
    /* the digits of the millions, the thousands or the rest */
    k = (len - 1) / 3;
    n = 0;
    do {
      d = *w++ - '0';
      n = n * 10 + d;
      len--;
    } while (len % 3 != 0);
    if (n > 0) {
      say_group(n);
      if (k > 0)
        say_number_word(scales[k]);
      zero = false;
    }
  }
  if (zero)
    say_number_word(ones[0]);
}

/* say the word that has been read, if there is one */
static void flush_word(void) {
  char *w;
  int len;

  word[wlen] = 0;
  /* leave out the apostrophes of quotes around it */
  w = word;
  while (*w == '\'')
    w++;
  len = strlen(w);
  while (len > 0 && w[len - 1] == '\'')
    w[--len] = 0;
  if (len > 0) {
    if (digits)
      say_number(w);
    else
      say_word(w);
  }
  wlen = 0;
  digits = true;
  spell = false;
}

/* the class of a character, see classes */
static int class_of(int c) {
  if (c >= ' ' && c < 127)
    return classes[c - ' '];
  if (c == '\t' || c == '\r' || c == '\n')
    return '2';
  return '3';
}

/* a character of a word */
static void add_char(int c) {
  if (wlen == DICT_WORD_MAX)
    flush_word();
  if (c >= 'a')
    c = c - 'a' + 'A';
  if (c > '9' || c < '0')
    digits = false;
  word[wlen++] = c;
}

/* a character between words */
static void separator(int k) {
  flush_word();
  if (k >= 'a') {
    add_pause(P_SPACE);
    say_word(symbols[k - 'a']);
    add_pause(P_SPACE);
  }
  else
    add_pause(k - '0');
}

/*
 * A character of the text.  A , or . after digits is kept until the next
 * character: before a digit, a , is in a number, as in 1,000, and a . is
 * a decimal point.
 */
static void feed(int c) {
  int k, p;

  k = class_of(c);
  if (pend) {
    p = pend;
    pend = 0;
    if (c >= '0' && c <= '9') {
      if (p == '.') {
        flush_word();
        say_number_word("POINT");
        spell = true;
      }
      add_char(c);
      return;
    }
    separator(class_of(p));
  }

  if (k == 'w')
    add_char(c);
  else if ((c == ',' || c == '.') && wlen > 0 && digits)
    pend = c;
  else
    separator(k);
}

/* the end of the text, with a pause after it */
static void finish(void) {
  if (pend) {
    separator(class_of(pend));
    pend = 0;
  }
  flush_word();
  if (spoken) {
    add_pause(P_OTHER);
    begin_word();
  }
  spoken = false;
  pause = P_NONE;
  newline();
}

/* speak a string */
static void feed_string(char *s) {
  while (*s)
    feed(*s++);
}

/*
 * Speak a file, false if it could not be opened.  It is read with read,
 * which takes a third of the time of fread.
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

/* Speak the lines typed at the console, until an empty one. */
static void feed_console(void) {
  char *line;

  line = (char *) block;
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

/* print a message about a file, and return 1 for main */
static int message(char *msg, char *name) {
  cputs(msg);
  cputs(name);
  cputs("\r\n");
  return 1;
}

/*
 * Print how tts is used.  main returns what it returns, as exit of the
 * small stdlib of -M does not link in Elf/OS programs.
 */
static int usage(void) {
  cputs("Usage: tts [-d dict] [-o file] [-p] [-q] [-u] [-f file | text]\r\n"
        "  -d dictionary, -f text file, -o phrase file to write,\r\n"
        "  -p print allophones, -q quiet, -u print unknown words\r\n");
  return 1;
}

static int run(int argc, char *argv[]) {
  int i, c, r;
  char *s, *text, *dname, *oname;

  text = NULL;
  dname = NULL;
  oname = NULL;
  out = -1;
  speak = true;
  digits = true;

  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    s = argv[i];
    if (s[1] == 0 || s[2] != 0)
      return usage();
    c = s[1] | 0x20;
    if (c == 'd' || c == 'f' || c == 'o') {
      if (++i == argc)
        return usage();
      if (c == 'd')
        dname = argv[i];
      else if (c == 'f')
        text = argv[i];
      else {
        oname = argv[i];
        speak = false;
      }
    }
    else if (c == 'p')
      print = true;
    else if (c == 'q')
      speak = false;
    else if (c == 'u')
      unknown = true;
    else
      return usage();
  }
  if (text != NULL && i < argc)
    return usage();

  if (dname == NULL) {
    dname = "sp0256.dic";
    r = dict_open(dname);
    if (r == DICT_NO_FILE) {
      dname = "/sp0256.dic";
      r = dict_open(dname);
    }
  }
  else
    r = dict_open(dname);
  if (r == DICT_NO_FILE)
    return message("Dictionary not found: ", dname);
  if (r != DICT_OK)
    return message("Not a dictionary: ", dname);

  if (speak) {
    cts_poll(pump);
    dict_poll(pump);
  }

  if (oname != NULL) {
    out = open(oname, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0)
      return message("Can not create ", oname);
  }

  if (text != NULL) {
    if (!feed_file(text))
      return message("File not found: ", text);
  }
  else if (i < argc) {
    for (; i < argc; i++) {
      feed_string(argv[i]);
      feed(' ');
    }
    finish();
  }
  else
    feed_console();

  drain();
  dict_close();

  if (out >= 0) {
    outblock[outlen++] = SAY_EOP;
    write(out, outblock, outlen);
    close(out);
  }
  return 0;
}

/* run tts on a stack of its own, see STACK_SIZE */
int main(int argc, char *argv[]) {
  int r;

  asm("          ldi   Cruntime_sp.1   ; save the stack pointer\n"
      "          phi   rf\n"
      "          ldi   Cruntime_sp.0\n"
      "          plo   rf\n"
      "          glo   r2\n"
      "          str   rf\n"
      "          inc   rf\n"
      "          ghi   r2\n"
      "          str   rf\n"
      "          ldi   (Cstack+255).1  ; and use the stack of tts\n"
      "          phi   r2\n"
      "          ldi   (Cstack+255).0\n"
      "          plo   r2\n");
  r = run(argc, argv);
  asm("          ldi   Cruntime_sp.1   ; back to the stack of the runtime\n"
      "          phi   rf\n"
      "          ldi   Cruntime_sp.0\n"
      "          plo   rf\n"
      "          lda   rf\n"
      "          plo   r2\n"
      "          ldn   rf\n"
      "          phi   r2\n");
  return r;
}
