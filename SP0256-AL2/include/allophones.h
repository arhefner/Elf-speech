#ifndef _ALLOPHONES_
#define _ALLOPHONES_

/*
 * allophones.h - the allophones of the SP0256-AL2, by their names in
 * the data sheet.  A phrase is an array of them ended by EOP:
 *
 *   static byte hello[] = { HH1, EH, LL, AX, OW, PA2, EOP };
 *
 * PA1 to PA5 are pauses of 10, 30, 50, 100 and 200 ms.
 */
#define PA1  0x00
#define PA2  0x01
#define PA3  0x02
#define PA4  0x03
#define PA5  0x04
#define OY   0x05
#define AY   0x06
#define EH   0x07
#define KK3  0x08
#define PP   0x09
#define JH   0x0a
#define NN1  0x0b
#define IH   0x0c
#define TT2  0x0d
#define RR1  0x0e
#define AX   0x0f
#define MM   0x10
#define TT1  0x11
#define DH1  0x12
#define IY   0x13
#define EY   0x14
#define DD1  0x15
#define UW1  0x16
#define AO   0x17
#define AA   0x18
#define YY2  0x19
#define AE   0x1a
#define HH1  0x1b
#define BB1  0x1c
#define TH   0x1d
#define UH   0x1e
#define UW2  0x1f
#define AW   0x20
#define DD2  0x21
#define GG3  0x22
#define VV   0x23
#define GG1  0x24
#define SH   0x25
#define ZH   0x26
#define RR2  0x27
#define FF   0x28
#define KK2  0x29
#define KK1  0x2a
#define ZZ   0x2b
#define NG   0x2c
#define LL   0x2d
#define WW   0x2e
#define XR   0x2f
#define WH   0x30
#define YY1  0x31
#define CH   0x32
#define ER1  0x33
#define ER2  0x34
#define OW   0x35
#define DH2  0x36
#define SS   0x37
#define NN2  0x38
#define HH2  0x39
#define OR   0x3a
#define AR   0x3b
#define YR   0x3c
#define GG2  0x3d
#define EL   0x3e
#define BB2  0x3f

/* the end of a phrase, any byte with bit 7 set will do */
#define EOP         0xff

#endif
