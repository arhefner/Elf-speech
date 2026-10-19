/*
 * calculate.c - a talking calculator for the SP0256-AL2
 *
 *   calculate [-q] number op number
 *
 * Calculates with 32 bit numbers, from -2147483648 to 2147483647, prints
 * the sum and speaks it:
 *
 *   calculate -5 x 435
 *   -5 x 435 = -2175
 *
 * says "negative five times four hundred thirty five equals negative two
 * thousand one hundred seventy five".
 *
 * op is + - x or * / and %, as in C: the division is of whole numbers,
 * -7 / 2 is -3, and % is the remainder, -7 % 2 is -1.  ? compares the
 * numbers, "five is less than seven".  A result that does not fit in 32
 * bits is an overflow, and division by zero an error.  -q prints, but
 * does not speak.
 *
 * Build for Elf/OS with:   make
 * Build for ELF-DOS with:  make TARGET_OS=elfdos
 */
#include <stdio.h>
#include <string.h>
#include <saynumber.h>

/*
 * The most words of a sentence: three numbers and three other words, or
 * two numbers and "is greater than".
 */
#define SENTENCE_MAX  (3 * SAY_NUMBER_MAX + 3)

/* the operator ?, which compares */
#define COMPARE  (-2)

static byte sentence[SENTENCE_MAX];
static int length;

/* true if a number is below zero */
static bool negative(int32_t n) {
  return (n.high & 0x8000) != 0;
}

/* true if a number is zero */
static bool zero(int32_t n) {
  return n.high == 0 && n.low == 0;
}

/* the size of a number, which is unsigned so that -2147483648 has one */
static uint32_t size(int32_t n) {
  if (negative(n))
    n = negi32(n);
  return u32_from_i32(n);
}

/*
 * Read a whole number, with a - or + in front of it or not, into n.
 * Returns false if it is not a number or does not fit in 32 bits.
 */
static bool read_number(char *s, int32_t *n) {
  uint32_t m;
  bool minus;

  minus = false;
  if (*s == '-' || *s == '+')
    minus = *s++ == '-';
  if (*s == 0)
    return false;

  m = u32_from_uint(0);
  for (; *s; s++) {
    if (*s < '0' || *s > '9')
      return false;
    /* above 214748364, ten times it does not fit */
    if (cmpu32(m, make_u32(0x0ccc, 0xcccc)) > 0)
      return false;
    m = addu32(mulu32x16(m, 10), u32_from_uint(*s - '0'));
  }

  /* up to 2147483647, or 2147483648 with a minus */
  if (cmpu32(m, make_u32(0x7fff, 0xffff)) > 0 &&
      (!minus || cmpu32(m, make_u32(0x8000, 0)) > 0))
    return false;

  *n = i32_from_u32(m);
  if (minus)
    *n = negi32(*n);
  return true;
}

/*
 * Calculate a op b into r.  Returns 0, or SAY_OVERFLOW if the result does
 * not fit in 32 bits, or SAY_ERROR for a division by zero.
 */
static int calculate(int32_t a, int op, int32_t b, int32_t *r) {
  uint32_t limit, most;

  switch (op) {
  case SAY_PLUS:
    *r = addi32(a, b);
    if (negative(a) == negative(b) && negative(*r) != negative(a))
      return SAY_OVERFLOW;
    break;

  case SAY_MINUS:
    *r = subi32(a, b);
    if (negative(a) != negative(b) && negative(*r) != negative(a))
      return SAY_OVERFLOW;
    break;

  case SAY_TIMES:
    /* the largest size of b for the size of a */
    if (!zero(a)) {
      limit = make_u32(0x7fff, 0xffff);
      if (negative(a) != negative(b))
        limit = make_u32(0x8000, 0);
      most = divu32(limit, size(a), NULL);
      if (cmpu32(size(b), most) > 0)
        return SAY_OVERFLOW;
    }
    *r = muli32(a, b);
    break;

  case SAY_DIVIDED:
    if (zero(b))
      return SAY_ERROR;
    /* -2147483648 / -1 is 2147483648 */
    if (a.high == 0x8000 && a.low == 0 && b.high == 0xffff && b.low == 0xffff)
      return SAY_OVERFLOW;
    *r = divi32(a, b, NULL);
    break;

  case SAY_MODULO:
    if (zero(b))
      return SAY_ERROR;
    /* anything % -1 is 0, which divi32 may not get for -2147483648 */
    if (b.high == 0xffff && b.low == 0xffff)
      *r = i32_from_int(0);
    else
      divi32(a, b, r);
    break;
  }
  return 0;
}

/* the word of an operator, or -1 */
static int operator(char *s) {
  if (s[1] != 0)
    return -1;
  switch (s[0]) {
  case '+':
    return SAY_PLUS;
  case '-':
    return SAY_MINUS;
  case 'x':
  case 'X':
  case '*':
    return SAY_TIMES;
  case '/':
    return SAY_DIVIDED;
  case '%':
    return SAY_MODULO;
  case '?':
    return COMPARE;
  }
  return -1;
}

/* add a word, or the words of a number, to the sentence */
static void add_word(int word) {
  sentence[length++] = word;
}

static void add_number(int32_t n) {
  length += say_number_words(n, sentence + length);
}

static void usage(void) {
  printf("Usage: calculate [-q] number op number\n");
  printf("  op is + - x * / %% or ? to compare\n");
  printf("  the numbers are -2147483648 to 2147483647\n");
  printf("  -q  do not speak\n");
}

int main(int argc, char *argv[]) {
  int32_t a, b, r;
  int op, result, first, i, c;
  bool speak;
  char abuf[12], bbuf[12], rbuf[12];

  speak = true;
  first = 1;
  if (argc > 1 && strcmp(argv[1], "-q") == 0) {
    speak = false;
    first = 2;
  }
  if (argc != first + 3) {
    usage();
    return 1;
  }

  if (!read_number(argv[first], &a)) {
    printf("Not a 32 bit number: %s\n", argv[first]);
    return 1;
  }
  op = operator(argv[first + 1]);
  if (op == -1) {
    printf("Not an operator: %s\n", argv[first + 1]);
    usage();
    return 1;
  }
  if (!read_number(argv[first + 2], &b)) {
    printf("Not a 32 bit number: %s\n", argv[first + 2]);
    return 1;
  }

  i32toa(a, abuf);
  i32toa(b, bbuf);
  add_number(a);

  if (op == COMPARE) {
    /* "five is less than seven", "five is equal to five" */
    c = cmpi32(a, b);
    add_word(SAY_IS);
    if (c < 0) {
      add_word(SAY_LESS);
      add_word(SAY_THAN);
    }
    else if (c > 0) {
      add_word(SAY_GREATER);
      add_word(SAY_THAN);
    }
    else {
      add_word(SAY_EQUAL);
      add_word(SAY_TO);
    }
    add_number(b);
    printf("%s %c %s\n", abuf, c < 0 ? '<' : c > 0 ? '>' : '=', bbuf);
  }
  else {
    result = calculate(a, op, b, &r);
    add_word(op);
    if (op == SAY_DIVIDED)
      add_word(SAY_BY);
    add_number(b);
    if (result == 0) {
      add_word(SAY_EQUALS);
      add_number(r);
      printf("%s %s %s = %s\n", abuf, argv[first + 1], bbuf, i32toa(r, rbuf));
    }
    else {
      add_word(result);
      printf("%s %s %s: %s\n", abuf, argv[first + 1], bbuf,
             result == SAY_ERROR ? "division by zero" : "overflow");
    }
  }

  if (speak)
    for (i = 0; i < length; i++)
      say_word(sentence[i]);
  return 0;
}
