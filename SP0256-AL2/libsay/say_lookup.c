#include "say_int.h"

#pragma extrn Csay_name

/*
 * The number of the allophone with a name, in upper or lower case, or
 * -1 if there is none with that name.
 */
int say_lookup(const char *name) {
  int i;
  char *s, *t;
  int c;

  for (i = 0; i < SAY_COUNT; i++) {
    s = say_name(i);
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
