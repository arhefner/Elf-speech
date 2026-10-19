#ifndef _DICT_
#define _DICT_

/*
 * dict.h - look words up in the pronunciation dictionary of tts, for ElfC
 *
 * The dictionary is a file made by tools/mkdict.py, sp0256.dic, of more
 * than 120,000 English words with the allophones of the SP0256-AL2 that
 * say them.  It is two B-trees of 512 byte blocks, one of the 5000 most
 * common words, near the start of the file, where Elf/OS and ELF-DOS find
 * a block faster, and one of the others.  A word is found with two reads
 * of a sector in each tree it is looked for in: the block of the index it
 * is in, and the block of words.  The header of the file is kept in
 * memory, and the block last read, so the library takes about 1.1K.
 *
 *   byte allo[DICT_MAX];
 *   int n;
 *
 *   if (dict_open("sp0256.dic") != DICT_OK)
 *     ...
 *   n = dict_lookup("HELLO", allo);      5: HH1 EH LL AX OW
 *   dict_close();
 *
 * The words are in upper case, with the letters A to Z, the digits and
 * the apostrophe, as in "DON'T".
 */
#ifndef _ELFCLIB_
#pragma .link .library dict.lib
#pragma .link .requires Cdict_lookup
#endif

/* the most allophones of a word, and the longest word */
#define DICT_MAX       64
#define DICT_WORD_MAX  48

/* what dict_open returns: opened, the file can not be opened, and it is
   not a dictionary */
#define DICT_OK        0
#define DICT_NO_FILE   -1
#define DICT_BAD_FILE  -2

/* open a dictionary file, and read its header */
int dict_open(char *name);

/*
 * Look a word up.  Puts its allophones, 0 to 63, in the array, which has
 * room for DICT_MAX of them, and returns how many there are, or 0 if the
 * word is not in the dictionary or can not be read.
 */
int dict_lookup(char *word, unsigned char *allophones);

/* close the dictionary file */
void dict_close(void);

/*
 * Set a function that dict_lookup calls now and then while it looks for
 * a word, before it reads a block and every 16 words of a block, or NULL
 * for none.  A program that speaks while it looks words up can give the
 * SP0256-AL2 the next allophone from it, as soon as the chip can take
 * one.  The function must not call the functions of this library.
 */
void dict_poll(int (*poll)());

#endif
