#include "say_int.h"
#include "../include/allophones.h"
#include "../include/saynumber.h"

#pragma extrn Csay
#pragma extrn Csay_allophone

/*
 * The words, in the order of their numbers in saynumber.h, each ended by
 * EOP.  They are from sp0256_dict/NUMBERS.txt, but for error, which is
 * from sp0256_al2_dictionary.txt, and overflow, which is as the
 * CTS256A-AL2 says it.
 */
static byte _say_words[] = {
  ZZ, YR, OW, EOP,                                          /* zero */
  WW, AX, AX, NN1, EOP,                                     /* one */
  TT2, UW2, EOP,                                            /* two */
  TH, RR1, IY, EOP,                                         /* three */
  FF, FF, OR, EOP,                                          /* four */
  FF, FF, AY, VV, EOP,                                      /* five */
  SS, SS, IH, IH, PA3, KK2, SS, EOP,                        /* six */
  SS, SS, EH, EH, VV, IH, NN1, EOP,                         /* seven */
  EY, PA3, TT2, EOP,                                        /* eight */
  NN1, AA, AY, NN1, EOP,                                    /* nine */
  TT2, EH, EH, NN1, EOP,                                    /* ten */
  IH, LL, EH, EH, VV, IH, NN1, EOP,                         /* eleven */
  TT2, WH, EH, EH, LL, VV, EOP,                             /* twelve */
  TH, ER1, PA2, PA3, TT2, IY, NN1, EOP,                     /* thirteen */
  FF, OR, PA2, PA3, TT2, IY, NN1, EOP,                      /* fourteen */
  FF, IH, FF, PA2, PA3, TT2, IY, NN1, EOP,                  /* fifteen */
  SS, SS, IH, PA3, KK2, SS, PA2, PA3, TT2, IY, NN1, EOP,    /* sixteen */
  SS, SS, EH, VV, IH, NN1, PA2, PA3, TT2, IY, NN1, EOP,     /* seventeen */
  EY, PA2, PA3, TT2, IY, NN1, EOP,                          /* eighteen */
  NN1, AY, NN1, PA2, PA3, TT2, IY, NN1, EOP,                /* nineteen */
  TT2, WH, EH, EH, NN1, PA2, PA3, TT2, IY, EOP,             /* twenty */
  TH, ER2, PA2, PA3, TT2, IY, EOP,                          /* thirty */
  FF, OR, PA3, TT2, IY, EOP,                                /* forty */
  FF, FF, IH, FF, FF, PA2, PA3, TT2, IY, EOP,               /* fifty */
  SS, SS, IH, PA3, KK2, SS, PA2, PA3, TT2, IY, EOP,         /* sixty */
  SS, SS, EH, VV, IH, NN1, PA2, PA3, TT2, IY, EOP,          /* seventy */
  EY, PA3, TT2, IY, EOP,                                    /* eighty */
  NN1, AY, NN1, PA3, TT2, IY, EOP,                          /* ninety */
  HH2, AX, AX, NN1, PA2, DD2, RR2, IH, IH, PA1, DD1, EOP,   /* hundred */
  TH, AA, AW, ZZ, PA1, PA1, NN1, DD1, EOP,                  /* thousand */
  MM, IH, IH, LL, YY1, AX, NN1, EOP,                        /* million */
  BB2, IH, IH, LL, YY1, AX, NN1, EOP,                       /* billion */
  NN1, EH, EH, PA1, GG2, AX, PA3, TT2, IH, VV, EOP,         /* negative */
  MM, AY, NN2, AX, PA2, SS, SS, EOP,                        /* minus */
  PP, LL, AX, AX, SS, SS, EOP,                              /* plus */
  TT2, AA, AY, MM, ZZ, EOP,                                 /* times */
  DD2, IH, VV, AY, PA2, DD2, IH, PA2, DD1, EOP,             /* divided */
  BB2, AA, AY, EOP,                                         /* by */
  IY, PA2, PA3, KK3, WH, AX, EL, ZZ, EOP,                   /* equals */
  EH, XR, OR, EOP,                                          /* error */
  OW, VV, ER1, FF, LL, OW, EOP,                            /* overflow */
  IY, PA2, PA3, KK3, WH, AX, EL, EOP,                       /* equal */
  TT2, UW2, EOP,                                            /* to */
  LL, EH, EH, SS, SS, EOP,                                  /* less */
  GG1, RR2, EY, TT2, ER1, EOP,                              /* greater */
  DH1, AE, AE, NN1, EOP,                                    /* than */
  IH, IH, ZZ, EOP,                                          /* is */
  MM, AA, AA, JH, UW1, LL, OW, EOP                          /* modulo */
};

static char *_say_word_names[SAY_WORDS] = {
  "zero", "one", "two", "three", "four", "five", "six", "seven",
  "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
  "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
  "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty",
  "ninety", "hundred", "thousand", "million", "billion",
  "negative", "minus", "plus", "times", "divided", "by", "equals",
  "error", "overflow", "equal", "to", "less", "greater", "than", "is",
  "modulo"
};

/*
 * Speak a word, followed by a PA2.  The words are found by counting the
 * EOPs before them, as ElfC can not make a table of their addresses.
 */
void say_word(int word) {
  byte *p;

  p = _say_words;
  while (word > 0) {
    if (*p++ == EOP)
      word--;
  }
  say(p);
  say_allophone(PA2);
}

/* The name of a word. */
char *say_word_name(int word) {
  return _say_word_names[word];
}
