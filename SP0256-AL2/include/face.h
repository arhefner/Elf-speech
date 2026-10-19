#ifndef _FACE_
#define _FACE_

/*
 * face.h - a talking face on a 128 by 64 OLED display, for the SP0256-AL2
 *
 * The face is drawn on an SSD1305 display on the SPI adapter, with the
 * ssd1305 library of ELFC-GPIO, and its mouth moves with the allophones
 * as the SP0256-AL2 speaks them.  Its eyes close for the PA5 pause at the
 * end of a sentence.
 *
 *   face_init(PORT(0, 2), PORT(0, 3));
 *   face_say(phrase);
 *   face_done();
 *
 * The chip holds the allophone it speaks and one more.  When it takes
 * the next one, its LRQ line drops, and the one it held has just begun,
 * so face_allophone gives it the next allophone and then shows the mouth
 * of the one that has begun.  After the last one there is nothing to
 * wait for on the LRQ line, so face_done waits for as long as the last
 * allophone is spoken, from the data sheet, and closes the mouth.
 *
 * Only the rectangles of the mouth and the eyes are sent to the
 * display, 144 bytes each, and only when they change.
 *
 * Including this file in a program makes the linker search face.lib,
 * ssd1305.lib, say.lib and gpio.lib.
 */
#ifndef _ELFCLIB_
#pragma .link .library face.lib
#pragma .link .library ssd1305.lib
#pragma .link .requires Cface_init
#endif

#ifndef _SAY_
#include <say.h>
#endif

/*
 * Set the control port and the data port of the SPI adapter, as spi_init
 * of the gpio library does, reset the display and draw the face on it,
 * with the mouth closed.  PORT(0, 2) and PORT(0, 3) on the 1802/MAX.
 */
void face_init(port_t ctrl, port_t data);

/*
 * Speak an allophone, once the chip can take it, as say_allophone does,
 * and show the mouth of the one that the chip has begun.
 */
void face_allophone(byte allophone);

/* speak a phrase, up to the byte with bit 7 set that ends it */
void face_say(const byte *phrase);

/*
 * Show the mouth of the allophone the chip holds as soon as it begins.
 * A program that does other work between allophones, as cts256face does,
 * calls this now and then, so that the mouth does not wait for the
 * next face_allophone.
 */
void face_poll(void);

/*
 * After the last allophone: show its mouth, wait until it has been
 * spoken, and close the mouth.
 */
void face_done(void);

/* clear the display and turn it off */
void face_off(void);

#endif
