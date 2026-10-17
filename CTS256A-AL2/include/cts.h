#ifndef _CTS_
#define _CTS_

/*
 * cts.h - the text to allophone conversion of the CTS256A-AL2 for ElfC
 *
 * The CTS256A-AL2 turns English text into the allophones of the
 * SP0256-AL2 with the letter to sound rules of the Naval Research
 * Laboratory.  cts.lib is the program of its ROM, written for the 1802,
 * with the rules of the ROM, so it gives the same allophones as the chip.
 *
 * The text is given a character at a time with cts_feed, and cts_step
 * converts it a rule at a time.  A word is converted once the word after
 * it has ended, as a rule can look at the next word, so there is always
 * a word that has not been converted until cts_finish ends the text:
 *
 *   unsigned char allo[CTS_MAX];
 *   int i, n;
 *
 *   cts_init(NULL);
 *   for each character c of the text {
 *     cts_feed(c);
 *     while ((n = cts_step(allo)) >= 0)
 *       for (i = 0; i < n; i++)
 *         say_allophone(allo[i]);
 *   }
 *   cts_finish();
 *   while ((n = cts_step(allo)) >= 0)
 *     ...
 *
 * Speaking the allophones as they come, as above, leaves the chip idle
 * while the next word is converted.  cts_poll lets a program queue them
 * and give them to the chip while it converts, which cts256.c does.
 *
 * The library makes no BIOS or operating system calls, so it works the
 * same under Elf/OS and ELF-DOS.
 */
#ifndef _ELFCLIB_
#pragma .link .library cts.lib
#pragma .link .requires Ccts
#endif

/* the most allophones cts_step puts in its array */
#define CTS_MAX  128

/*
 * Start the conversion.  exceptions is NULL, or the addresses of the 27
 * lists of exception words of an exception EPROM: the words that start
 * with A to Z, and then the others.  The lists are in the format of the
 * EPROM, see the README.
 */
void cts_init(unsigned char **exceptions);

/*
 * Add a character to the text.  Upper and lower case are the same, and
 * every character but the letters, the digits, ' and [\]^_`{ ends a
 * word.  A word of more than about 200 characters is cut, with a CR.
 */
void cts_feed(int c);

/* end the text, with a CR as the chip gets at the end */
void cts_finish(void);

/*
 * Convert the next characters of the text with a rule.  Puts the
 * allophones of the rule in the array, up to CTS_MAX of them, and
 * returns how many there are, which can be none.  Returns -1 when the
 * text that can be converted has been.
 */
int cts_step(unsigned char *allophones);

/*
 * Set a function that cts_step calls every few rules while it converts,
 * or NULL for none.  A rule can take longer than the allophones the
 * SP0256-AL2 holds, so a program that speaks while it converts can give
 * the chip the next allophone from this function, as soon as it can take
 * it, as the CTS256A-AL2 does with an interrupt.  The function must not
 * call the functions of this library.
 */
void cts_poll(int (*poll)());

#endif
