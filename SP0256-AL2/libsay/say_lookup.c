#include "say_int.h"

/* the names of the allophones 0 to 63, from the SP0256-AL2 data sheet */
static char *_say_names[SAY_COUNT] = {
  "PA1", "PA2", "PA3", "PA4", "PA5", "OY",  "AY",  "EH",
  "KK3", "PP",  "JH",  "NN1", "IH",  "TT2", "RR1", "AX",
  "MM",  "TT1", "DH1", "IY",  "EY",  "DD1", "UW1", "AO",
  "AA",  "YY2", "AE",  "HH1", "BB1", "TH",  "UH",  "UW2",
  "AW",  "DD2", "GG3", "VV",  "GG1", "SH",  "ZH",  "RR2",
  "FF",  "KK2", "KK1", "ZZ",  "NG",  "LL",  "WW",  "XR",
  "WH",  "YY1", "CH",  "ER1", "ER2", "OW",  "DH2", "SS",
  "NN2", "HH2", "OR",  "AR",  "YR",  "GG2", "EL",  "BB2"
};

/*
 * The number of the allophone with a name, in upper or lower case, or
 * -1 if there is none with that name.
 */
int say_lookup(const char *name) {
  int i;
  char *s, *t;
  int c;

  for (i = 0; i < SAY_COUNT; i++) {
    s = _say_names[i];
    t = name;
    for (;;) {
      c = *t++;
      if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
      if (c != *s)
        break;
      if (c == 0)
        return i;
      s++;
    }
  }
  return -1;
}

/* The name of an allophone, 0 to 63. */
char *say_name(byte allophone) {
  return _say_names[allophone & 0x3f];
}
