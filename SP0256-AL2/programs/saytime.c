/*
 * saytime.c - speak the time of day with the SP0256-AL2
 *
 *   saytime            speak the time of the real time clock
 *   saytime hh:mm      speak a time, hh is 0 to 23
 *
 * The time is spoken on a twelve hour clock, as "the time is three
 * fifteen p m" or "the time is twelve o'clock a m".
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <time.h>
#include <say.h>
#include <allophones.h>

static byte one[]       = { WW, AX, PA3, PA2, NN1, EOP };
static byte two[]       = { TT2, UW2, EOP };
static byte three[]     = { TH, RR1, IY, EOP };
static byte four[]      = { FF, OR, EOP };
static byte five[]      = { FF, AY, VV, EOP };
static byte six[]       = { SS, SS, IH, IH, PA3, KK2, SS, EOP };
static byte seven[]     = { SS, SS, EH, EH, VV, IH, NN1, EOP };
static byte eight[]     = { EY, PA3, TT2, EOP };
static byte nine[]      = { NN2, AA, AY, NN1, EOP };
static byte ten[]       = { TT2, EH, EH, NN1, EOP };
static byte eleven[]    = { IH, LL, EH, EH, VV, IH, NN1, EOP };
static byte twelve[]    = { TT2, WH, EH, EH, LL, VV, EOP };
static byte thirteen[]  = { TH, ER1, PA2, PA3, TT2, IY, NN1, EOP };
static byte fourteen[]  = { FF, OR, PA2, PA3, TT2, IY, NN1, EOP };
static byte fifteen[]   = { FF, IH, FF, PA2, PA3, TT2, IY, NN1, EOP };
static byte sixteen[]   = { SS, SS, IH, PA3, KK2, SS, PA2, PA3, TT2, IY, NN1,
                            EOP };
static byte seventeen[] = { SS, SS, EH, VV, IH, NN1, PA2, PA3, TT2, IY, NN1,
                            EOP };
static byte eighteen[]  = { EY, PA2, PA3, TT2, IY, NN1, EOP };
static byte nineteen[]  = { NN1, AY, NN1, PA2, PA3, TT2, IY, NN1, EOP };
static byte twenty[]    = { TT2, WH, EH, EH, NN1, PA2, PA3, TT2, IY, EOP };
static byte thirty[]    = { TH, ER2, PA2, PA3, TT2, IY, EOP };
static byte forty[]     = { FF, OR, PA3, TT2, IY, EOP };
static byte fifty[]     = { FF, IH, FF, PA2, PA3, TT2, IY, EOP };

static byte time_is[]   = { DH1, AX, PA2, PA3, TT2, AY, MM, PA2, IH, ZZ, PA2,
                            PA3, EOP };
static byte oclock[]    = { OW, PA3, KK1, LL, AA, PA3, KK2, EOP };
static byte am[]        = { EY, EH, EH, MM, EOP };
static byte pm[]        = { PP, IY, EH, EH, MM, EOP };

/*
 * The words for 0 to 19, where 0 is said as twelve for the hour, and
 * for 20, 30, 40 and 50.  ElfC can not initialize an array of pointers
 * with the addresses of arrays, so make_tables fills them in.
 */
static byte *numbers[20];
static byte *tens[4];

static void make_tables(void) {
  numbers[0] = twelve;
  numbers[1] = one;
  numbers[2] = two;
  numbers[3] = three;
  numbers[4] = four;
  numbers[5] = five;
  numbers[6] = six;
  numbers[7] = seven;
  numbers[8] = eight;
  numbers[9] = nine;
  numbers[10] = ten;
  numbers[11] = eleven;
  numbers[12] = twelve;
  numbers[13] = thirteen;
  numbers[14] = fourteen;
  numbers[15] = fifteen;
  numbers[16] = sixteen;
  numbers[17] = seventeen;
  numbers[18] = eighteen;
  numbers[19] = nineteen;

  tens[0] = twenty;
  tens[1] = thirty;
  tens[2] = forty;
  tens[3] = fifty;
}

/* the longest phrase is about 50 allophones */
#define PHRASE_SIZE  80

static byte phrase[PHRASE_SIZE];
static int length;

static struct tm now;

/* add a word to the phrase */
static void add_word(byte *word) {
  while ((*word & 0x80) == 0)
    phrase[length++] = *word++;
}

/* add one allophone to the phrase */
static void add(byte allophone) {
  phrase[length++] = allophone;
}

/* make the phrase for a time, hour 0 to 23 and minute 0 to 59 */
static void make_phrase(int hour, int minute) {
  length = 0;

  add_word(time_is);
  add_word(numbers[hour % 12]);
  add(PA2);

  if (minute == 0)
    add_word(oclock);
  else if (minute < 10) {
    add(OW);
    add_word(numbers[minute]);
  } else if (minute < 20)
    add_word(numbers[minute]);
  else {
    add_word(tens[minute / 10 - 2]);
    if (minute % 10 != 0)
      add_word(numbers[minute % 10]);
  }

  add(PA4);
  add_word(hour < 12 ? am : pm);
  add(PA2);
  add(EOP);
}

/* read hh:mm into now, false if it is not a time */
static bool parse_time(char *s) {
  int hour, minute, digits;

  hour = 0;
  digits = 0;
  while (*s >= '0' && *s <= '9' && digits < 2) {
    hour = hour * 10 + *s++ - '0';
    digits++;
  }
  if (digits == 0 || *s++ != ':')
    return false;

  minute = 0;
  digits = 0;
  while (*s >= '0' && *s <= '9' && digits < 2) {
    minute = minute * 10 + *s++ - '0';
    digits++;
  }
  if (digits != 2 || *s != 0 || hour > 23 || minute > 59)
    return false;

  now.tm_hour = hour;
  now.tm_min = minute;
  return true;
}

int main(int argc, char *argv[]) {
  if (argc > 2 || (argc == 2 && !parse_time(argv[1]))) {
    printf("Usage: saytime [hh:mm]\n");
    return 1;
  }

  /*
   * Without a real time clock, Elf/OS gives the time the kernel keeps,
   * but ELF-DOS keeps none and gives midnight, which is not worth
   * saying.  systime is false when there is no clock.
   */
  if (argc == 1 && !systime(&now)) {
#ifdef __ELFDOS__
    printf("No real time clock, give the time as saytime hh:mm\n");
    return 1;
#endif
  }

  make_tables();
  make_phrase(now.tm_hour, now.tm_min);
  say(phrase);

  return 0;
}
