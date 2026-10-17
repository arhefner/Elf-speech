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
  `cgetline`.

```
make                     build the libraries and the programs for Elf/OS
make TARGET_OS=elfdos    build them for ELF-DOS
make install             copy the libraries and headers into ElfC (default /opt/elfc)
make clean               remove the generated files
```

The libraries go to `lib/`. The programs are in `programs/`: `say.elfos`,
`saytime.elfos` and `speak.elfos` for Elf/OS, and `say`, `saytime` and
`speak` for ELF-DOS. Use `make install ELFC_DIR=folder` if ElfC is installed
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

## Notes

* ElfC can not initialize an array of pointers with the addresses of arrays,
  only with strings, so `saytime` fills in its tables of words when it
  starts.
* The programs were tested in the Run/02 emulator, under its Elf/OS kernel
  calls and under the ELF-DOS kernel booted from a disk image, by checking
  the bytes written to the port. The output of `saytime` is the same as that
  of `os-time` for every minute of the day.
