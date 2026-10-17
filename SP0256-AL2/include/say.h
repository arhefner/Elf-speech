#ifndef _SAY_
#define _SAY_

/*
 * say.h - SP0256-AL2 speech library for ElfC
 *
 * The SP0256-AL2 speaks allophones, the 64 sounds numbered 0 to 63 that
 * are named in allophones.h.  The number of an allophone is written to
 * the address lines of the chip, which are wired to an output port, and
 * the chip asks for the next one with its LRQ line, which is wired to an
 * EF line.
 *
 * The library uses the gpio library for the port and the EF line, and
 * makes no BIOS or operating system calls, so a program that uses it
 * runs under both Elf/OS and ELF-DOS.
 *
 * Including this file in a program makes the linker search say.lib and
 * gpio.lib, which have to be in the lib folder of ElfC, in the current
 * folder, or in a folder given to ElfC with -l.
 */
#ifndef _ELFCLIB_
#pragma .link .library say.lib
#pragma .link .requires Csay
#endif

#include <gpio.h>

/*
 * A phrase is an array of allophone numbers, ended by a byte with bit 7
 * set, which is not spoken.  SAY_EOP is the usual end, and is the same
 * as EOP in allophones.h.
 */
#define SAY_EOP  0xff

/* the number of allophones, which are numbered 0 to SAY_COUNT - 1 */
#define SAY_COUNT  64

/*
 * Set the port the address lines of the chip are wired to and the EF
 * line, 1 to 4, its LRQ line is wired to.  The library waits while the
 * EF line is asserted (the pin is low) before it sends an allophone.
 *
 * A program that does not call say_init uses the port and the EF line
 * the library was built with, PORT(1, 2) and EF4 unless the makefile
 * was told otherwise.  This is the first parallel port of the 1802/MAX.
 *
 *   say_init(PORT(1, 2), 4);
 */
void say_init(port_t port, byte ef);

/*
 * Speak one allophone, waiting until the chip can take it.  Only the
 * low six bits are used.  The function returns as soon as the chip has
 * the allophone, while it is still speaking it.
 */
void say_allophone(byte allophone);

/* speak a phrase, up to the byte with bit 7 set that ends it */
void say(const byte *phrase);

/*
 * The names of the allophones, as in the SP0256-AL2 data sheet and in
 * allophones.h.  say_lookup is the number of an allophone from its
 * name, in upper or lower case, or -1 if there is none with that name.
 * say_name is the name of an allophone, 0 to 63.
 */
int say_lookup(const char *name);
char *say_name(byte allophone);

#endif
