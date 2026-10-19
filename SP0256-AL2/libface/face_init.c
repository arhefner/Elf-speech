/*
 * face_init.c - the talking face, see face.h
 *
 * All the functions are in this one procedure, which face.h makes the
 * linker take, as they share the state of the face.  The images are
 * made by tools/mkface.py.
 */
#define _ELFCLIB_
#include <gpio.h>
#include "../include/say.h"
#include "../include/allophones.h"
#include "../include/face.h"
#include "ssd1305.h"
#include "face_images.h"

#pragma extrn Cspi_init
#pragma extrn Cspi_control
#pragma extrn Cspi_write
#pragma extrn Cspi_write_dma
#pragma extrn Cdelay_ms
#pragma extrn Cssd1305_init
#pragma extrn Cssd1305_display
#pragma extrn Cssd1305_clear
#pragma extrn Cssd1305_sleep
#pragma extrn Csay_ready
#pragma extrn Csay_allophone

#ifdef FACE_TRACE
#pragma extrn Cout_port
#endif

/* the lines of the SPI adapter for the display, as in the ssd1305 library */
#define OLED_IDLE     SPI_OUT1
#define OLED_COMMAND  (SPI_CS0 | SPI_OUT1)
#define OLED_DATA     (SPI_CS0 | SPI_OUT1 | SPI_OUT0)

/* the commands of the SSD1305 that set the window the bytes go to */
#define SET_COL_ADDR   0x21
#define SET_PAGE_ADDR  0x22

/* the windows of the mouth and of the eyes on the display */
#define WINDOW_SIZE  6

static byte _face_mouth_window[WINDOW_SIZE] = {
  SET_PAGE_ADDR, MOUTH_PAGE, MOUTH_PAGE + MOUTH_PAGES - 1,
  SET_COL_ADDR, MOUTH_COL, MOUTH_COL + MOUTH_COLS - 1
};

static byte _face_eyes_window[WINDOW_SIZE] = {
  SET_PAGE_ADDR, EYES_PAGE, EYES_PAGE + EYES_PAGES - 1,
  SET_COL_ADDR, EYES_COL, EYES_COL + EYES_COLS - 1
};

static int _face_pending;   /* the allophone the chip holds, or -1 */
static int _face_speaking;  /* the allophone the chip speaks, or -1 */
static int _face_mouth;     /* the mouth that is shown */
static bool _face_closed;   /* the eyes are closed */

/* send a rectangle of an image to its window on the display */
static void _face_show(const byte *window, const byte *rect, unsigned int size) {
  spi_control(OLED_COMMAND);
  spi_write(window, WINDOW_SIZE);
  spi_control(OLED_DATA);
  spi_write_dma(rect, size);
  spi_control(OLED_IDLE);
}

static void _face_mouth_show(int mouth) {
  if (mouth == _face_mouth)
    return;
  _face_mouth = mouth;
#ifdef FACE_TRACE
  out_port(PORT(0, 6), mouth);
#endif
  _face_show(_face_mouth_window, face_mouths + mouth * MOUTH_SIZE, MOUTH_SIZE);
}

static void _face_eyes_show(bool closed) {
  if (closed == _face_closed)
    return;
  _face_closed = closed;
#ifdef FACE_TRACE
  out_port(PORT(0, 7), closed);
#endif
  _face_show(_face_eyes_window, face_eyes + (closed ? EYES_SIZE : 0), EYES_SIZE);
}

/* the chip has begun an allophone */
static void _face_begun(int allophone) {
  _face_speaking = allophone;
  _face_mouth_show(face_mouth_of[allophone]);
  _face_eyes_show(allophone == PA5);
}

/* Set the ports, reset the display and draw the face. */
void face_init(port_t ctrl, port_t data) {
  spi_init(ctrl, data);
  ssd1305_init();
  ssd1305_display(face_image);
  _face_pending = -1;
  _face_speaking = -1;
  _face_mouth = 0;
  _face_closed = false;
}

/*
 * Speak an allophone, then show the mouth of the one the chip has begun.
 * The chip is given the next one first, as the one it has begun can be
 * as short as 10 ms, less than it takes to send a mouth to the display.
 */
void face_allophone(byte allophone) {
  int begun;

  while (!say_ready())
    ;
  begun = _face_pending;
  say_allophone(allophone);
  _face_pending = allophone & 0x3f;
  if (begun >= 0)
    _face_begun(begun);
}

/*
 * Show the mouth of the allophone the chip holds as soon as it begins,
 * when there is no next one to give the chip yet.
 */
void face_poll(void) {
  if (_face_pending >= 0 && say_ready()) {
    _face_begun(_face_pending);
    _face_pending = -1;
  }
}

/* Speak a phrase, up to the byte with bit 7 set that ends it. */
void face_say(const byte *phrase) {
  while ((*phrase & 0x80) == 0)
    face_allophone(*phrase++);
}

/*
 * Show the last allophone, wait until it is spoken, and close the mouth.
 * If face_poll has shown it already, it is not known how much of it is
 * left, and it is waited for as long as it is.
 */
void face_done(void) {
  if (_face_pending >= 0) {
    while (!say_ready())
      ;
    _face_begun(_face_pending);
    _face_pending = -1;
  }
  if (_face_speaking < 0)
    return;
  delay_ms(face_tens[_face_speaking] * 10);
  _face_speaking = -1;
  _face_mouth_show(0);
  _face_eyes_show(false);
}

/* Clear the display and turn it off. */
void face_off(void) {
  ssd1305_clear();
  ssd1305_sleep();
}
