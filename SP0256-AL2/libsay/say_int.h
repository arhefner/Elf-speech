#ifndef _SAY_INT_
#define _SAY_INT_

/*
 * Definitions shared by the functions of the say library.
 *
 * A function of an ElfC library has to name the functions and the
 * variables of other procedures that it uses, as _ELFCLIB_ keeps the
 * header files from doing so.  Each source file has a #pragma extrn
 * for the ones it uses.
 */
#define _ELFCLIB_
#include "../include/say.h"

/*
 * The port and the EF line a program uses if it does not call
 * say_init.  The makefile sets them:
 *
 *   make SAY_GROUP=1 SAY_INDEX=2 SAY_EF=4
 */
#ifndef SAY_GROUP
#define SAY_GROUP  1
#endif

#ifndef SAY_INDEX
#define SAY_INDEX  2
#endif

#ifndef SAY_EF
#define SAY_EF     4
#endif

#endif
