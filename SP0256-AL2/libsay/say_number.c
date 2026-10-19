#include "say_int.h"
#include "../include/saynumber.h"

#pragma extrn Csay_word
#pragma extrn Cdivu32
#pragma extrn Cnegi32
#pragma extrn Cu32_from_i32
#pragma extrn Cu32_from_uint

/*
 * Put the words of n, 1 to 999, at words[count], as "seven hundred forty
 * two", and return the new count.
 */
static int say_hundreds(int n, byte *words, int count) {
  if (n >= 100) {
    words[count++] = n / 100;
    words[count++] = SAY_HUNDRED;
    n %= 100;
  }
  if (n >= 20) {
    words[count++] = SAY_TWENTY + n / 10 - 2;
    n %= 10;
  }
  if (n > 0)
    words[count++] = n;
  return count;
}

/*
 * Put the words of a number in the array, and return how many there are.
 * The number is taken apart into thousands with 32 bit division, three
 * times at most, and each of them is said with 16 bits.
 */
int say_number_words(int32_t n, byte *words) {
  uint32_t m, rem, thousand;
  int group[4];
  int i, count;

  count = 0;
  if (n.high & 0x8000) {
    /* -2147483648 has no negative, but as unsigned it is its own */
    words[count++] = SAY_NEGATIVE;
    n = negi32(n);
  }
  m = u32_from_i32(n);
  if (m.high == 0 && m.low == 0) {
    words[count++] = SAY_ZERO;
    return count;
  }

  thousand = u32_from_uint(1000);
  for (i = 0; i < 4; i++) {
    m = divu32(m, thousand, &rem);
    group[i] = rem.low;
  }

  for (i = 3; i >= 0; i--) {
    if (group[i] != 0) {
      count = say_hundreds(group[i], words, count);
      if (i > 0)
        words[count++] = SAY_HUNDRED + i;
    }
  }
  return count;
}

/* Speak a number. */
void say_number(int32_t n) {
  byte words[SAY_NUMBER_MAX];
  int i, count;

  count = say_number_words(n, words);
  for (i = 0; i < count; i++)
    say_word(words[i]);
}
