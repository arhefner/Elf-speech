#include "say_int.h"

#pragma extrn Cout_port
#pragma extrn Cef1_asserted
#pragma extrn Cef2_asserted
#pragma extrn Cef3_asserted
#pragma extrn Cef4_asserted

/*
 * The port and the EF line of the chip.  say.h makes the linker take
 * this procedure into every program that uses the library, so they are
 * set even if the program never calls say_init.
 */
static port_t _say_port = PORT(SAY_GROUP, SAY_INDEX);
static byte _say_ef = SAY_EF;

/* Set the port and the EF line of the chip. */
void say_init(port_t port, byte ef) {
  _say_port = port;
  _say_ef = ef;
}

/* True while the chip can not take an allophone, LRQ is high. */
static bool say_busy(void) {
  switch (_say_ef) {
  case 1:
    return ef1_asserted();
  case 2:
    return ef2_asserted();
  case 3:
    return ef3_asserted();
  }
  return ef4_asserted();
}

/* Speak one allophone, once the chip can take it. */
void say_allophone(byte allophone) {
  while (say_busy())
    ;
  out_port(_say_port, allophone & 0x3f);
}

/* Speak a phrase, up to the byte with bit 7 set that ends it. */
void say(const byte *phrase) {
  while ((*phrase & 0x80) == 0)
    say_allophone(*phrase++);
}
