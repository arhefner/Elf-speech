/*
 * speak.c - speak allophones typed at the console with the SP0256-AL2
 *
 *   speak
 *
 * Type the names of allophones separated by spaces, in upper or lower
 * case, and press Enter to hear them:
 *
 *   say: hh1 eh ll ax ow pa2
 *
 * The line can be edited, and the up and down arrows recall the lines
 * typed before, see cgetline in conio.  An empty line says the last
 * phrase again, ? lists the allophones, and Q or Ctrl-C quits.  A PA1
 * pause is added to the end of each phrase, so that the last allophone
 * is spoken in full.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <conio.h>
#include <say.h>
#include <allophones.h>

#define LINE_SIZE     128
#define PHRASE_SIZE   64
#define HISTORY_SIZE  512

static char line[LINE_SIZE];
static char history[HISTORY_SIZE];

/*
 * The phrase that was spoken last, and the one being read, with room
 * for the PA1 that ends a phrase and the SAY_EOP after it.
 */
static byte phrase[PHRASE_SIZE + 2];
static byte next[PHRASE_SIZE + 2];

/* list the allophones and their numbers */
static void list(void) {
  int i;

  for (i = 0; i < SAY_COUNT; i++) {
    printf("%02x %-4s", i, say_name(i));
    if (i % 8 == 7)
      printf("\n");
    else
      printf("  ");
  }
}

/* is the line the one word w, in upper or lower case? */
static bool is_word(char *s, char *w) {
  int c;

  while (*s == ' ')
    s++;
  while (*w) {
    c = *s++;
    if (c >= 'a' && c <= 'z')
      c -= 'a' - 'A';
    if (c != *w++)
      return false;
  }
  while (*s == ' ')
    s++;
  return *s == 0;
}

/*
 * Read the allophones on the line into next, followed by a PA1 so the
 * chip finishes the last one.  Returns true if they are all known,
 * otherwise tells which one is not and returns false.
 */
static bool read_phrase(char *s) {
  char *word;
  int n, a;

  n = 0;
  for (;;) {
    while (*s == ' ')
      s++;
    if (*s == 0)
      break;

    word = s;
    while (*s != ' ' && *s != 0)
      s++;
    if (*s == ' ')
      *s++ = 0;

    a = say_lookup(word);
    if (a < 0) {
      printf("Unknown allophone: %s\n", word);
      return false;
    }
    if (n == PHRASE_SIZE) {
      printf("Too many allophones, the most is %d\n", PHRASE_SIZE);
      return false;
    }
    next[n++] = a;
  }

  next[n++] = PA1;
  next[n] = SAY_EOP;
  return true;
}

int main(int argc, char *argv[]) {
  int i;

  if (argc > 1) {
    printf("Usage: speak\n");
    return 1;
  }

  printf("SP0256-AL2 speech synthesizer\n");
  printf("Type allophones, ? for a list, Q to quit\n");

  cgethistory(history, HISTORY_SIZE);
  phrase[0] = SAY_EOP;

  for (;;) {
    cputs("say: ");
    if (cgetline(line, LINE_SIZE) == CGETLINE_CANCEL)
      break;

    if (is_word(line, "Q"))
      break;

    if (is_word(line, "?")) {
      list();
      continue;
    }

    /* an empty line says the last phrase again */
    if (!is_word(line, "")) {
      if (!read_phrase(line))
        continue;
      for (i = 0; i <= PHRASE_SIZE + 1; i++)
        phrase[i] = next[i];
    }

    say(phrase);
  }

  return 0;
}
