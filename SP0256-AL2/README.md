# SP0256-AL2

Software for the General Instrument SP0256-AL2 speech synthesizer on a
parallel port of a CDP1802 Elf, written for the
[ElfC](https://github.com/arhefner/ELFC) C compiler. The same sources build
programs for both Elf/OS and ELF-DOS.

It is an ElfC version of the assembly language `say` library and the `say`,
`os-time` and `os-speak` programs, and it has:

| | |
|-|-|
| `say.lib` | The speech library: speaks allophones and phrases, and looks allophones up by name. |
| `say` | Speaks phrase files. |
| `saytime` | Speaks the time of day. |
| `speak` | Speaks allophones typed at the console, with line editing and recall. |
| `calculate` | A talking calculator for 32 bit numbers. |
| `face.lib` | A talking face on a 128 by 64 OLED display, whose mouth moves with the speech. |
| `sayface` | Speaks phrase files with the talking face. |

## Hardware

The address lines A1 to A6 of the chip are wired to the low six bits of an
output port, and its LRQ line to an EF line. The library writes the number
of an allophone to the port once the EF line is no longer asserted.

The default is the first parallel port of the 1802/MAX, port 2 of port
group 1, with LRQ on EF4. That is the same as the assembly version built
with `TARGET = 1802MAX` and `FLAG_LINE = EF4`. To build for other wiring:

```
make clean
make SAY_GROUP=0 SAY_INDEX=4 SAY_EF=3
```

A program can also set the port and EF line when it runs, with `say_init`.

The port group is selected through the expander port that `gpio.lib` was
built with, which is port 5 by default. See the ELFC-GPIO README for other
systems.

## Building

You need:

* ElfC on the path, with the [ELFC-GPIO](https://github.com/arhefner/ELFC-GPIO)
  and [ELFC-conio](https://github.com/arhefner/ELFC-conio) libraries
  installed in it (`make install` in each). `speak` needs a conio that has
  `cgetline`. `calculate` uses the math32 library that comes with ElfC.
* The ssd1305 library of ELFC-GPIO, for `face.lib` and `sayface`, built
  with `make -C examples` there. The makefiles look for it in
  `ELFC-GPIO/examples/ssd1305` next to this repository; give
  `SSD1305_DIR=folder` if it is somewhere else.

```
make                     build the libraries and the programs for Elf/OS
make TARGET_OS=elfdos    build them for ELF-DOS
make install             copy the libraries and headers into ElfC (default /opt/elfc)
make clean               remove the generated files
```

The libraries go to `lib/`. The programs are in `programs/`: `say.elfos`,
`saytime.elfos`, `speak.elfos`, `calculate.elfos` and `sayface.elfos` for
Elf/OS, and the same names without `.elfos` for ELF-DOS. Use `make install ELFC_DIR=folder` if ElfC is installed
somewhere else.

## The programs

### say

```
say file [file ...]
```

Speaks phrase files, one after the other. A phrase file is allophone
numbers, one byte each, ended by a byte with bit 7 set or by the end of the
file. `phrases/hello` and `phrases/intro` are examples.

Unlike the assembly version, `say` reads the file as it speaks it, so a file
can be any size, and it takes more than one file.

### saytime

```
saytime            speak the time of the real time clock
saytime hh:mm      speak a time, hh is 0 to 23
```

Speaks the time on a twelve hour clock, as "the time is three fifteen p m"
or "the time is twelve o'clock a m". The words are the same as in
`os-time`.

On Elf/OS without a real time clock, `saytime` speaks the time the kernel
keeps. ELF-DOS keeps no time of its own, so on ELF-DOS without a clock
`saytime` gives a message and speaks only a time given as `hh:mm`.

### speak

```
speak
```

Type the names of allophones separated by spaces, in upper or lower case,
and press Enter to hear them:

```
SP0256-AL2 speech synthesizer
Type allophones, ? for a list, Q to quit
say: hh1 eh ll ax ow pa2
```

* An empty line says the last phrase again.
* `?` lists the allophones and their numbers.
* `Q` or Ctrl-C quits.
* A name that is not an allophone is reported, and nothing is spoken.
* A line can have up to 64 allophones.
* A `PA1` pause is added to the end of each line, so that the last allophone
  is spoken in full.

The line can be edited as it is typed, and the up and down arrows recall the
lines typed before, with `cgetline` of the conio library, see its README for
the keys. The last 512 bytes of lines are kept, which is about 20 lines. A
line with a typo is kept too, so it can be recalled and fixed.

### calculate

```
calculate [-q] number op number
```

Calculates with 32 bit numbers, from -2147483648 to 2147483647, prints the
sum and speaks it:

```
> calculate -5 x 435
-5 x 435 = -2175
```

says "negative five times four hundred thirty five equals negative two
thousand one hundred seventy five".

| Operator | | Example |
|-|-|-|
| `+` | plus | |
| `-` | minus | |
| `x` or `*` | times | |
| `/` | divided by | `-7 / 2` is -3, whole numbers as in C |
| `%` | modulo | `-7 % 2` is -1, the remainder as in C |
| `?` | compares | `5 ? 7` prints `5 < 7` and says "five is less than seven", or "is greater than", "is equal to" |

* A result that does not fit in 32 bits is said as "overflow", and a
  division by zero as "error", in place of "equals" and the result.
* `-q` prints the sum, but does not speak it.

The words are spoken with `saynumber.h`, below.

### sayface

```
sayface file [file ...]
```

Speaks phrase files as `say` does, with a talking face on an SSD1305 OLED
display on the SPI adapter: its mouth moves with the allophones, and its
eyes close for the PA5 pause at the end of a sentence. The face is drawn
when it starts, and stays with its mouth closed at the end. `cts256face` of
[CTS256A-AL2](../CTS256A-AL2) shows the face as it speaks text.

The ports of the adapter are those of the 1802/MAX, port 2 and 3 of group
0. Build with other ones with `make SPI_GROUP=0 SPI_CTL=2 SPI_DATA=3`.

## The say library

Include `say.h` in a program. It tells the linker to search `say.lib` and
`gpio.lib`. `allophones.h` names the allophones as in the data sheet:

```c
#include <say.h>
#include <allophones.h>

static byte hello[] = { HH1, EH, LL, AX, OW, PA2, EOP };

int main(void) {
  say(hello);
  return 0;
}
```

| Function | Description |
|----------|-------------|
| `void say_init(port_t port, byte ef)` | Set the port of the chip, as `PORT(group, index)`, and the EF line of LRQ, 1 to 4. Optional. |
| `void say_allophone(byte allophone)` | Speak one allophone, 0 to 63, once the chip can take it. |
| `void say(const byte *phrase)` | Speak allophones up to the byte with bit 7 set that ends the phrase. |
| `bool say_ready(void)` | True when the chip can take an allophone, so that `say_allophone` would not wait. |
| `int say_lookup(const char *name)` | The number of an allophone from its name, in any case, or -1. |
| `char *say_name(byte allophone)` | The name of an allophone. |

The functions return as soon as the chip has the last allophone, while it is
still speaking it. A program can do other work while the chip speaks, and
give it the next allophone when `say_ready` is true, as `cts256` of
[CTS256A-AL2](../CTS256A-AL2) does.

The library calls only `gpio.lib`, which makes no BIOS or operating system
calls, so it works the same under Elf/OS and ELF-DOS.

### Numbers

`saynumber.h` speaks 32 bit numbers, the `int32_t` of the math32 library of
ElfC, and the words of a calculator. A number is turned into a list of
numbered words, which can be spoken one by one or printed:

```c
#include <saynumber.h>

byte words[SAY_NUMBER_MAX];
int i, n;

n = say_number_words(atoi32("-2175"), words);
for (i = 0; i < n; i++)
  printf("%s ", say_word_name(words[i]));    /* negative two thousand ... */

say_number(atoi32("-2175"));
say_word(SAY_EQUALS);
```

| Function | Description |
|----------|-------------|
| `int say_number_words(int32_t n, byte *words)` | Put the words of a number in an array of `SAY_NUMBER_MAX` bytes, and return how many. |
| `void say_number(int32_t n)` | Speak a number. |
| `void say_word(int word)` | Speak a word, followed by a PA2 pause. |
| `char *say_word_name(int word)` | The name of a word, "zero" to "modulo". |

The words are `SAY_ZERO` to `SAY_NINETY`, `SAY_HUNDRED`, `SAY_THOUSAND`,
`SAY_MILLION`, `SAY_BILLION`, `SAY_NEGATIVE`, `SAY_MINUS`, `SAY_PLUS`,
`SAY_TIMES`, `SAY_DIVIDED`, `SAY_BY`, `SAY_EQUALS`, `SAY_ERROR`,
`SAY_OVERFLOW`, `SAY_EQUAL`, `SAY_TO`, `SAY_LESS`, `SAY_GREATER`,
`SAY_THAN`, `SAY_IS` and `SAY_MODULO`. The word of a number below 20 is the number, and the tens
are `SAY_TWENTY + n / 10 - 2`. A number is said as "negative", then the
billions, millions, thousands and the rest, each as "seven hundred forty
two", and 0 as "zero".

The allophones of the words are from `sp0256_dict/NUMBERS.txt`, except
"error", which is from `sp0256_al2_dictionary.txt`, and "overflow", which
is as the CTS256A-AL2 says it.

### The talking face

`face.h` draws the face, and moves its mouth with the speech:

```c
#include <face.h>

face_init(PORT(0, 2), PORT(0, 3));   /* the ports of the SPI adapter */
face_say(phrase);
face_done();
```

| Function | Description |
|----------|-------------|
| `void face_init(port_t ctrl, port_t data)` | Set the ports of the SPI adapter, reset the display and draw the face. |
| `void face_allophone(byte allophone)` | Speak an allophone, as `say_allophone`, and show the mouth of the one the chip has begun. |
| `void face_say(const byte *phrase)` | Speak a phrase. |
| `void face_poll(void)` | Show the mouth of an allophone as soon as it begins, for a program that does other work between allophones. |
| `void face_done(void)` | After the last allophone: show it, wait until it is spoken, and close the mouth. |
| `void face_off(void)` | Clear the display and turn it off. |

The SP0256-AL2 holds the allophone it speaks and one more. When it begins
the one it holds, it can take the next, and LRQ drops. So `face_allophone`
waits for LRQ, gives the chip the next allophone, and then shows the mouth
of the one that has begun. After the last allophone, LRQ tells nothing
more, so `face_done` waits as long as the data sheet says that allophone
is spoken.

There are ten mouths, each for the allophones that look alike:

| Mouth | Allophones |
|-------|------------|
| Closed, a smile | PA1 to PA5 |
| Lips pressed | MM PP BB1 BB2 |
| Teeth on the lower lip | FF VV |
| Tongue between the teeth | TH DH1 DH2 |
| Wide open, with the tongue | AA AE AY AW AR |
| Open, with the teeth | EH EY AX HH1 HH2 YR |
| Wide and flat | IY IH YY1 YY2 |
| Round | AO OW OY OR UH |
| Small and round | UW1 UW2 WW WH RR1 RR2 ER1 ER2 XR SH ZH CH JH |
| A little open, with the teeth | the other consonants |

Only the rectangle of the mouth, or of the eyes, is sent to the display,
144 bytes, and only when it changes. At 4 MHz a mouth is on the display
about 4 ms after its allophone begins, and the speech is never held up.

`tools/mkface.py` draws the face and the mouths, and writes
`libface/face_images.h`, with which mouth each allophone has. Change it
to draw another face, `python3 tools/mkface.py --png folder` writes the
images as PNG files to look at. It needs only Python.

## Notes

* ElfC can not initialize an array of pointers with the addresses of arrays,
  only with strings, so `saytime` fills in its tables of words when it
  starts.
* The programs were tested in the Run/02 emulator, under its Elf/OS kernel
  calls and under the ELF-DOS kernel booted from a disk image, by checking
  the bytes written to the port. The output of `saytime` is the same as that
  of `os-time` for every minute of the day. `calculate` was checked against
  the same calculator in Python, for 450 sums with the numbers at the ends
  of the ranges and random ones, for what it prints and the allophones it
  sends.
* The face was tested in a Run/02 that sets EF4 as the LRQ of an
  SP0256-AL2 would, from the lengths of the allophones in the data sheet,
  with a build that writes the mouths it shows to a port, to time each
  mouth against its allophone. The images were checked as PNG files, as
  Run/02 does not emulate the DMA of the SPI adapter.
