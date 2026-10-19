# SP0256-TTS

Text to speech with the General Instrument SP0256-AL2 on a CDP1802 Elf,
from a dictionary of more than 120,000 English words with their
allophones, for the [ElfC](https://github.com/arhefner/ELFC) C compiler.
The same sources build a program for Elf/OS and for ELF-DOS.

| | |
|-|-|
| `tts` | Speaks text, a text file or lines typed at the console, or writes the allophones to a phrase file. It fits on an Elf with 32K of RAM. |
| `sp0256.dic` | The dictionary, a file of 1.4 MB, made by `tools/mkdict.py`. |
| `dict.lib` | Looks words up in the dictionary, for other programs. |
| `tools/` | Python tools that build the dictionary, change how words are said, and test `tts`. |

A word that is not in the dictionary is converted with the letter to sound
rules of the CTS256A-AL2, with `cts.lib` of [CTS256A-AL2](../CTS256A-AL2).

## Hardware

The SP0256-AL2 is wired as for the [say library](../SP0256-AL2), which
`tts` speaks with. The default is the first parallel port of the 1802/MAX
with LRQ on EF4. Build for other wiring with the same settings as there:

```
make clean
make SAY_GROUP=0 SAY_INDEX=4 SAY_EF=3
```

## Building

You need ElfC on the path, with the
[ELFC-GPIO](https://github.com/arhefner/ELFC-GPIO) and
[ELFC-conio](https://github.com/arhefner/ELFC-conio) libraries installed in
it, as for [SP0256-AL2](../SP0256-AL2), Asm/02, and Python 3 for the
dictionary.

```
make                     build the libraries, tts for Elf/OS and sp0256.dic
make TARGET_OS=elfdos    build them, tts for ELF-DOS
make dict                build only the dictionary
make install             copy dict.lib and dict.h into ElfC (default /opt/elfc)
make clean               remove the generated files
```

The say library is built in `../SP0256-AL2` and `cts.lib` in
`../CTS256A-AL2`. `tts.elfos` is for Elf/OS and `tts` for ELF-DOS, in
`programs/`. The dictionary is built from the word files in
`../SP0256-AL2/sp0256_dict`, which takes about 15 seconds.

Copy `tts` and `sp0256.dic` to the disk of the Elf. `tts` looks for the
dictionary in the current folder and then in the root folder, or where
`-d` says.

## tts

```
tts [-d file] [-o file] [-p] [-q] [-u] [text ...]
tts [-d file] [-o file] [-p] [-q] [-u] -f file
```

Speaks the text on the command line, the text of a file, or, with
neither, the lines typed at the console until an empty one.

| Option | |
|-|-|
| `-d file` | The dictionary, `sp0256.dic` if not given. |
| `-f file` | Speak the text of a file. |
| `-o file` | Write the allophones to a phrase file, and do not speak them. |
| `-p` | Print the names of the allophones. |
| `-q` | Do not speak. |
| `-u` | Print the words that are not in the dictionary, which are said by the rules. |

```
tts Hello, world.
tts -f story.txt
tts -p -q The quick brown fox.
tts -o hello.say Hello there.
say hello.say
```

The phrase file of `-o` is the allophones, one byte each, ended by FF,
which `say` of [SP0256-AL2](../SP0256-AL2) speaks. `-p -q` shows how a text
is said without the chip, and `-u -q` which of its words are not in the
dictionary:

```
> tts -p -q Hello world.
HH1 EH LL AX OW PA2 WW ER2 LL PA2 DD1 PA5 PA5
```

### How text is converted

* A word is letters, digits and apostrophes, in upper or lower case.
  Quotes around a word, as in `'hello'`, are left out.
* A word is looked for in the dictionary. A word with `'s` that is not
  there is the word before it with an S, which is `IH ZZ` after a hissing
  sound, `SS` after another voiceless one and `ZZ` after the others, so
  "John's" is said. Otherwise the apostrophes are left out if that finds
  the word, and then the word is converted by the rules.
* A number is said as words, as "one million two hundred thirty four
  thousand five hundred sixty seven" for `1,234,567` or `1234567`, up to
  nine digits. A longer number, or one that starts with 0 and is not 0, is
  said a digit at a time, as are the digits after a decimal point: `3.14`
  is "three point one four".
* `&` `%` `$` `#` `+` `=` and `@` are said as "and", "percent",
  "dollars", "number", "plus", "equals" and "at".
* The other characters between words are pauses, and the longest pause of
  the characters between two words is made: `.` `!` and `?` are PA5 PA5,
  `:` PA5, `,` and `;` PA4, spaces, line ends, quotes and brackets PA2,
  `-` PA1, and the others PA3. The text ends with at least a PA3.
* A word of more than 48 characters is cut.

### Speed

The allophones are spoken while the next words are looked up and
converted, and `tts` keeps up to 128 of them, about ten seconds of speech,
ahead of the chip. A word takes about 50,000 instructions under ELF-DOS
and about two reads of a sector, as most words are among the 5000 common
words near the start of the dictionary, below. Spoken, a word takes about
half a second.

At 4 MHz the speech does not stop: for 3,353 words of a book, which take
30 minutes to say, the chip never waited, as worked out from the
instructions `tts` took for each allophone in the Run/02 emulator, under
the ELF-DOS kernel, with 3,500 instructions for each sector read by the
BIOS, and the lengths of the allophones in the data sheet. At 1.79 MHz the
chip waits now and then between words, about a tenth of the time, so
there write a phrase file with `-o` and speak it with `say`.

Elf/OS and ELF-DOS find a place in a file by following its chain of
clusters from the start, which takes longer the further into the file it
is. This is why the common words are at the start. A word that is not one
of them takes longer, mostly under Elf/OS, which follows the chain of 4K
lumps one by one.

### Memory

`tts` is 26K for ELF-DOS and 24K for Elf/OS, built with `-M`, the smaller
libraries of ElfC. ELF-DOS leaves 29,312 bytes for a program on an Elf
with 32K of RAM, and each open file takes 577 bytes, so there is room for
the dictionary, the text file and the phrase file. `tts` prints with
`cputs`, reads typed lines with `gets`, and classes the characters of the
text with a table, to be small.

The runtime of `-M` has a system stack of 63 bytes, which a word converted
by the rules nearly fills, through the poll functions of `cts.lib` and
`dict.lib` and the say library, and a read of the dictionary under ELF-DOS
needs room for the kernel too. So `main` moves the stack to 256 bytes of
`tts`, of which about 80 are used, and moves it back before it returns.

## The dictionary

`tools/mkdict.py` builds `sp0256.dic` from the files in
`../SP0256-AL2/sp0256_dict`:

| File | |
|-|-|
| `A.txt` to `Z.txt` | The big dictionary, 120,173 words. Its pronunciations are phonemes, and are turned into allophones by the rules below. The first pronunciation of a word is used. |
| `NUMBERS.txt` | Words with the allophones they are said with, tuned by ear, which replace those of the big dictionary. |
| `sp0256_al2_dictionary.txt` | The same. |
| `contractions.txt` | Contractions such as "don't" and "you're", which the big dictionary does not have, made with the rules from their phonemes. |
| `overrides.txt` | The words changed with `dictupdate.py`, below. |
| `common.txt` | The most common English words, the most common first. |

The files are read in this order, and a word in a later file replaces it in
an earlier one. A line is a word, a colon and the allophones, each with a
backslash:

```
HELLO: \HH1 \EH \LL \AX \OW
```

### The rules

The big dictionary has the phonemes of English without stress, one
allophone name for each: AX for the vowel of "but" and "about", TT1, KK1,
DD1 and the others for every T, K and D, and ER and YY, which are not
allophones, for the R vowel and the Y. The SP0256-AL2 has several
allophones for many of them, which sound right in different places of a
word, and needs pauses before the stops. `mkdict.py` chooses them as the
words of `sp0256_al2_dictionary.txt` do:

* PA3 before PP, TT, KK and CH, and PA1 or PA2 before BB, DD, GG and JH,
  but not at the start of a word.
* TT2, but TT1 before SS. DD2, but DD1 at the end. BB2 before a vowel and
  BB1 elsewhere.
* KK1 and GG1 before the front vowels, LL, RR and YY, KK3 and GG2 before
  the other vowels, and KK2 and GG3 at the end and before the other
  consonants. HH1 before the front vowels and HH2 before the others.
* A vowel before an R that is not before a vowel is OR, AR, YR or XR, and
  ER is ER1 in an ending and ER2 elsewhere. RR2 after a stop and RR1
  elsewhere.
* UW1 before another vowel and UW2 at the end, YY2 and DH1 at the start
  and YY1 and DH2 elsewhere, WH after TT and KK, and EL for an L that ends
  a syllable.
* SS is doubled at the start, before a vowel, and at the end after a
  vowel. The short vowel of a word of one syllable is doubled, as is the
  first short vowel of a longer word, which is most often stressed, but
  not in words such as "the", "and" and "of", which are not stressed.

`tools/mkdict.py --compare` shows how close the rules come to the words
tuned by ear: 111 of 293 the same, and 1.3 allophones different on
average.

### Changing how words are said

`tools/dictupdate.py` takes files in the same format, and says the words
in them with the new allophones, or adds them. It puts them in
`overrides.txt` and builds the dictionary again, so the other word files
are not changed:

```
tools/mkdict.py --show tomato potato           how they are said now
tools/dictupdate.py better.txt                 change them
tools/dictupdate.py -n better.txt              only show what would change
tools/dictupdate.py -r tomato                  say it as before again
```

A file `better.txt` with the line

```
TOMATO: \TT2 \AX \MM \AA \AA \PA3 \TT2 \OW
```

says "tomato" the British way:

```
TOMATO: TT2 AX MM EY PA3 TT2 OW  ->  TT2 AX MM AA AA PA3 TT2 OW
overrides.txt: 1 words
sp0256.dic: 1425920 bytes, 120238 words
```

Run them in this folder, so that `sp0256.dic` is the one here, and copy it
to the Elf. `tts -u -q -f file` lists the words of a text that the
dictionary does not have, and `tts -p -q` how a text is said, to find the
words to change.

### The file

The dictionary is two B-trees of blocks of 512 bytes, which are sectors on
the disk, so that a block is read with one sector read. The first tree has
the 5000 most common words of `common.txt`, and the second all the
others. Block 0 is the header, with the top level of each tree, which
`dict.lib` keeps in memory. Below it are the blocks of the index, then the
blocks of the common words, and the other words are last, so that about
nine words in ten of English text are found within the first 100K of the
file. A word is found with two reads in each tree it is looked for in,
the block of the index it is in and the block of words.

A block is entries ended by FF. An entry is a word, front coded, and its
data:

| | |
|-|-|
| shared | How many letters it shares with the word before. |
| letters | The other letters, the last with bit 7 set. |
| data | Bytes, the last with bit 7 set. |

In a block of words the data is the allophones, so that they can be given
to the chip as they are. In the index, the entry is the start of the first
word of a block of the level below, and its data the number of the block.
See `tools/mkdict.py` for the header. The search through a block is in
assembly language, `libdict/dict_scan.asm`, and `mkdict.py` has the same
search in Python, with which it looks up every word in the file it built.

`tools/mkcommon.py` makes `common.txt` from English text files. The one
that comes with `tts` was made from 29 books of Project Gutenberg, where
the 5000 most common words that are in the dictionary are 90% of the
words, and 97% of the words are in the dictionary.

## The dict library

`dict.lib` and `dict.h` can be used by other programs. The library reads
the dictionary with `open`, `lseek32` and `read`, so it works under Elf/OS
and ELF-DOS.

```c
#include <dict.h>

unsigned char allo[DICT_MAX];
int n;

if (dict_open("sp0256.dic") != DICT_OK)
  ...
n = dict_lookup("HELLO", allo);       /* 5: HH1 EH LL AX OW */
dict_close();
```

| Function | Description |
|----------|-------------|
| `int dict_open(char *name)` | Open a dictionary and read its header. Returns `DICT_OK`, `DICT_NO_FILE` or `DICT_BAD_FILE`. |
| `int dict_lookup(char *word, unsigned char *allophones)` | Put the allophones of a word, in upper case, in an array of `DICT_MAX` bytes, and return how many, or 0 if it is not there. |
| `void dict_close(void)` | Close the dictionary. |
| `void dict_poll(int (*poll)())` | A function that `dict_lookup` calls before each read and every 16 words of a block, to give the chip the next allophone, as `cts_poll` of `cts.lib`. |

It takes 1.1K of memory for the header and a block, and one open file.

## How it was tested

`tools/compare_elf.py` runs `tts` in the Run/02 emulator, with its Elf/OS
kernel calls, writing a phrase file, and checks it against a model of
`tts` in Python, which looks words up with `mkdict.py` and converts the
others with `cts.py` of [CTS256A-AL2](../CTS256A-AL2), which needs the ROM
of the CTS256A-AL2 as `cts.py` does:

```
export CTS_ROM=path/to/CTS256A-AL2/CTS256A_AL2.cpp
tools/compare_elf.py file ...
```

They were the same for 80K of four books of Project Gutenberg, and for 26K
of random text with numbers, decimal points, apostrophes, symbols, control
characters, characters above 127 and words of more than 48 letters, 140,000
allophones in all. `tts` was also run under the ELF-DOS kernel booted in
Run/02, from a disk image, where it says the same, and where its speed was
measured, see Speed.

## Notes

* `-O` of ElfC, the branch optimization of Link/02 `-r`, is not used yet:
  Link/02 1.14 pairs each `v` record, the low byte of a local address as in
  `ldi v_wr.0`, with a `^` record next to it in the list, also one of
  another address, as `^0999 v09a3` in `cts.prg`, and so writes the low
  byte of the wrong address, which breaks `cts.lib` and `dict.lib`. With a
  Link/02 that has the fix, add `-O` to `CFLAGS` in `programs/Makefile`:
  `tts` is then 25,620 bytes for ELF-DOS and 23,834 for Elf/OS, and says
  the same.
* ElfC can not initialize an array of pointers with the addresses of
  arrays, but it can with strings, so the words of the numbers are tables
  of strings, which are looked up in the dictionary.
