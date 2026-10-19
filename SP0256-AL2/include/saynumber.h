#ifndef _SAYNUMBER_
#define _SAYNUMBER_

/*
 * saynumber.h - speak numbers and the words of a calculator with the
 * SP0256-AL2, for ElfC
 *
 * The words are numbered, and a number is turned into the list of its
 * words, which can be spoken or printed:
 *
 *   byte words[SAY_NUMBER_MAX];
 *   int i, n;
 *
 *   n = say_number_words(atoi32("-2175"), words);
 *   for (i = 0; i < n; i++)
 *     printf("%s ", say_word_name(words[i]));     negative two thousand
 *                                                  one hundred seventy five
 *   say_number(atoi32("-2175"));
 *
 * The words are in say.lib, with the other functions of say.h.  The
 * numbers are the int32_t of the math32 library of ElfC.
 */
#ifndef _SAY_
#include <say.h>
#endif
#include <math32.h>

/*
 * say_number_words and say_word_name are in the procedures of
 * say_number and say_word, which the linker finds only by those names.
 */
#ifndef _ELFCLIB_
#pragma .link .requires Csay_number
#pragma .link .requires Csay_word
#endif

/*
 * The words.  0 to 19 are the numbers zero to nineteen, and twenty to
 * ninety are SAY_TWENTY + 0 to 7, so the word of a number below 20 is
 * the number, and the word of the tens of n is SAY_TWENTY + n / 10 - 2.
 * Thousand, million and billion follow each other, the word of the
 * thousands to the power p, 1 to 3, is SAY_HUNDRED + p.
 */
#define SAY_ZERO       0
#define SAY_TEN       10
#define SAY_TWENTY    20
#define SAY_NINETY    27
#define SAY_HUNDRED   28
#define SAY_THOUSAND  29
#define SAY_MILLION   30
#define SAY_BILLION   31
#define SAY_NEGATIVE  32
#define SAY_MINUS     33
#define SAY_PLUS      34
#define SAY_TIMES     35
#define SAY_DIVIDED   36
#define SAY_BY        37
#define SAY_EQUALS    38
#define SAY_ERROR     39
#define SAY_OVERFLOW  40
#define SAY_EQUAL     41
#define SAY_TO        42
#define SAY_LESS      43
#define SAY_GREATER   44
#define SAY_THAN      45
#define SAY_IS        46
#define SAY_MODULO    47

/* the number of words */
#define SAY_WORDS     48

/* the most words of a number, -2147483648 has 17 */
#define SAY_NUMBER_MAX  20

/*
 * Speak a word, followed by a PA2 pause, as between the words of a
 * sentence.  The function returns as soon as the chip has the pause.
 */
void say_word(int word);

/* the name of a word in lower case, "zero" to "modulo" */
char *say_word_name(int word);

/*
 * Put the words of a number in an array of at least SAY_NUMBER_MAX
 * bytes, and return how many there are: "negative" for a number below
 * 0, then the billions, millions, thousands and the rest, each as
 * "seven hundred forty two".  0 is "zero".
 */
int say_number_words(int32_t n, byte *words);

/* speak a number, as say_number_words gives its words */
void say_number(int32_t n);

#endif
