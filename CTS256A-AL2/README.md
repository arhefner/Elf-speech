# CTS256A-AL2

Text to speech with the General Instrument SP0256-AL2 on a CDP1802 Elf, as
the CTS256A-AL2 code-to-speech chip does it, for the
[ElfC](https://github.com/arhefner/ELFC) C compiler. The same sources build
a program for Elf/OS and for ELF-DOS.

The CTS256A-AL2 is a TMS7000 microcontroller with a program in ROM that
turns English text into the allophones of the SP0256-AL2, with the letter
to sound rules of the Naval Research Laboratory. This is that program,
written for the 1802 from a disassembly of the ROM. It uses the rules of
the ROM and takes the same steps as the ROM, so it gives the same
allophones as the chip, also where the ROM has a bug. It also uses the
exception words of an exception EPROM of the chip.

| | |
|-|-|
| `cts256` | Speaks text, a text file or lines typed at the console, or writes the allophones to a phrase file. |
| `cts.lib` | The conversion, in 1802 assembly language, for other programs. |
| `tools/` | Python tools that test the conversion against the emulator of the chip. |

## Hardware

The SP0256-AL2 is wired as for the [say library](../SP0256-AL2), which
`cts256` speaks with. The default is the first parallel port of the
1802/MAX with LRQ on EF4. Build for other wiring with the same settings as
there:

```
make clean
make SAY_GROUP=0 SAY_INDEX=4 SAY_EF=3
```

## Building

You need ElfC on the path, with the
[ELFC-GPIO](https://github.com/arhefner/ELFC-GPIO) and
[ELFC-conio](https://github.com/arhefner/ELFC-conio) libraries installed in
it, as for [SP0256-AL2](../SP0256-AL2), and Asm/02.

```
make                     build the say library, cts.lib and cts256 for Elf/OS
make TARGET_OS=elfdos    build them for ELF-DOS
make install             copy cts.lib and cts.h into ElfC (default /opt/elfc)
make clean               remove the generated files
```

The say library is built in `../SP0256-AL2`. `cts256` needs the one in this
repository, which has `say_ready`, so if an older `say.lib` is installed in
ElfC, install the new one with `make install` there, as ElfC looks in its
own folders first.

`cts256.elfos` is for Elf/OS and `cts256` for ELF-DOS, in `programs/`.
They are about 27K.

## cts256

```
cts256 [-x file] [-o file] [-p] [-q] [text ...]
cts256 [-x file] [-o file] [-p] [-q] -f file
```

Speaks the text on the command line, the text of a file, or, with
neither, the lines typed at the console.

| Option | |
|-|-|
| `-f file` | Speak the text of a file. |
| `-x file` | Use the exception words of an exception EPROM image, see below. |
| `-o file` | Write the allophones to a phrase file, and do not speak them. |
| `-p` | Print the names of the allophones. |
| `-q` | Do not speak. |

```
cts256 Hello, world.
cts256 -f story.txt
cts256 -p -q The quick brown fox.
cts256 -o hello.say Hello there.
say hello.say
```

The phrase file of `-o` is the allophones, one byte each, ended by FF,
which `say` of [SP0256-AL2](../SP0256-AL2) speaks. `-p -q` shows how a
text is pronounced without the chip:

```
> cts256 -p -q Hello world.
HH1 EH LL OW PA2 WW ER1 LL PA2 DD1 PA5 PA5 PA3
```

With no text, `cts256` speaks each line as it is typed. Enter on an empty
line says the last one again, and Ctrl-C quits. The lines can be edited,
and the up and down arrows recall the ones before, with `cgetline` of the
conio library.

The allophones are given to the chip while the text after them is being
converted, every few rules, as the chip does with an interrupt, so the
speech does not stop between words. At 4 MHz the SP0256-AL2 is never kept
waiting. At 1.79 MHz it waits now and then, for up to 30 ms, mostly in the
pauses: in the Run/02 emulator, 12 times in two minutes of speech.

## How text is converted

The program does what the chip does with the text it gets:

* Upper and lower case are the same.
* Every character but the letters, the digits, `'` and `` [\]^_`{ `` ends a
  word. Spaces, punctuation and line ends are pauses: `.` `!` and `?` are
  PA5 PA5, `:` PA5, `,` and `;` PA4, a space PA2, `-` PA1 and the others
  PA3. `%`, `$` and `#` are spoken as words, and digits one at a time.
* A word is looked for in the exception words first, if there are any,
  and then converted with the rules.
* The text ends with a CR, which the chip gets at the end of its input in
  the emulator, so the allophones end with a PA3.
* A word of more than about 200 characters is cut, with a CR, as the chip
  cuts a word that fills its input buffer.

The chip also takes Esc to clear its input, Backspace to delete the last
character and Ctrl-R to say the last phrase again. These are not special
here, they are pauses like the other control characters.

### Looking ahead

One rule looks at the next word: "the" before a vowel is `DH1 IY`, as in
"the apple", and otherwise `DH1 AX`. So a word is converted once the word
after it has ended. The chip converts a word as soon as it ends, with
whatever of the next word it has been sent by then, so it can say "the"
either way when the next word is still coming in. The emulator of the chip
does this too.

## Exception words

An exception EPROM of the CTS256A-AL2 holds words that the rules say
wrongly, with their allophones, such as names and abbreviations. `-x` takes
an image of one, in the binary format of the EPROM, as made by `cts_eprom`
of the [emulator of the chip](https://github.com/GmEsoft/SP0256_CTS256A-AL2)
from a list of words:

```
<[FRIDAY]< = [FF RR2 AY PA2 DD2 EY]
<[CAP]A = [KK1 EY PP]                 ; capable, capability
[&] = [AE NN1 DD1 PA1]
```

The words are written as rules, see the `exceptions.txt` that comes with
`cts_eprom`: `<` is anything but a letter, so `<[FRIDAY]<` is the word on
its own and `<[CAP]A` the start of a word before an A. When an exception
matches up to the end of a word, it is the whole word, otherwise the rules
carry on with the rest of the word.

The exceptions are looked for only where a word starts, as on the chip,
so `[&]` is "and" in "up & down", but not in "up&down", where the `&`
ends the word "up". The notes in `exceptions.txt` say otherwise, but the
emulator of the chip agrees.

The image is read into memory as it is, so it can be up to 8K. The chip
looks for the EPROM at a 4K boundary, from 1000 to E000, and runs code in
it, so the image has to be laid out as `cts_eprom` lays it out:

| Address | |
|-|-|
| 000 | 80 48 28 58 85, which the chip looks for |
| 0A3 | The addresses of the 27 lists of exception words, high byte first: the words that start with A to Z, then the others |

Each list is the exceptions, as the rules are encoded in the ROM, see
`libcts/cts.asm`, ended by FF. The base address of the EPROM is taken from
the first address. Because FF ends a list, a single letter can not be an
exception, on the chip either.

## The cts library

`cts.lib` and `cts.h` can be used by other programs. The library makes no
BIOS or operating system calls, so it works under Elf/OS and ELF-DOS.

```c
#include <cts.h>

unsigned char allo[CTS_MAX];
int i, n;

cts_init(NULL);
for each character c of the text {
  cts_feed(c);
  while ((n = cts_step(allo)) >= 0)
    for (i = 0; i < n; i++)
      say_allophone(allo[i]);
}
cts_finish();
while ((n = cts_step(allo)) >= 0)
  ...
```

| Function | Description |
|----------|-------------|
| `void cts_init(unsigned char **exceptions)` | Start, with the addresses of the 27 lists of exception words, or with NULL for none. |
| `void cts_feed(int c)` | Add a character to the text. |
| `void cts_finish(void)` | End the text, with a CR. |
| `int cts_step(unsigned char *allophones)` | Convert the next characters with a rule, put its allophones in the array and return how many, or -1 if there is nothing to convert yet. |
| `void cts_poll(int (*poll)())` | A function that `cts_step` calls every eight rules, to give the chip the next allophone, as in `cts256.c`. |

The conversion takes about 1800 instructions a character, about 20 ms at
1.79 MHz, and the speech about 100 ms a character, so there is time to
spare. The library is 4K: the 2K of rules, the code and a 256 byte ring
for the text, which keeps what the rules look at before and after a word.

## How it was made, and tested

The ROM is in the C++ source of the
[CTS256A-AL2 emulator](https://github.com/GmEsoft/SP0256_CTS256A-AL2) by
Michel Bernard. `tools/dis7000.py` disassembles it, and the rules engine at
F3E7 to F78B and the exception EPROM code that `cts_eprom` writes were
ported from it. `tools/mkrules.py` takes the rules out of the ROM into
`libcts/rules.inc`.

`tools/cts.py` is a model of the conversion in Python, written first, from
the disassembly. With `--rules` it lists the rules, and the exceptions of
an EPROM, in the notation of the NRL rules.

```
export CTS_ROM=path/to/CTS256A-AL2/CTS256A_AL2.cpp
tools/cts.py Hello world.
tools/cts.py --rules -x exceptions.bin
tools/compare_emulator.py -e path/to/cts256a-al2 file ...
tools/compare_elf.py file ...
```

* `compare_emulator.py` checks the model against the emulator, which runs
  the ROM. They were the same for 21,170 different words from the Vim help
  files, with and without the sample exception EPROM of `cts_eprom`, and
  for 3,000 lines of its prose but for some "the"s, which the emulator
  says either way, see Looking ahead.
* `compare_elf.py` runs `cts256` in the Run/02 emulator and checks it
  against the model. They were the same for the words, the prose and
  210K of random text with punctuation, digits and control characters,
  with and without the exceptions, over a million allophones in all.

`cts256` was also run under the ELF-DOS kernel booted in Run/02, with the
allophones taken from the port, and in a Run/02 that sets EF4 as the LRQ
of an SP0256-AL2 would, with the lengths of the allophones from the data
sheet, to check that the allophones go to the chip in order and to time
how long it waits.

## Notes

* Link/02 checks a short branch as if its target were on the first page of
  its procedure, so it rejects good short branches after the first 256
  bytes. `cts.asm` uses only long branches because of this.
* The procedure of `cts.lib` is aligned to a page, for the ring of the
  text, so a program has up to 255 bytes of padding before it.
* ElfC does not take a char as an array index, so the ring is in assembly
  language, where the low byte of a pointer wraps around by itself.
