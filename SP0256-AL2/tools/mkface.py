#!/usr/bin/env python3
"""
mkface.py - make the images of the talking face for the SSD1305 display

    tools/mkface.py > libface/face_images.h
    tools/mkface.py --png folder        also write the images as PNG files

Draws a face on the 128 by 64 display, and the shapes of its mouth for
the allophones of the SP0256-AL2, and writes them as C arrays for
face.c: the whole face, with its mouth closed, and for each mouth the
rectangle of the display it is in, and the eyes closed for a blink.  It
also writes which mouth each allophone has, and how long it is spoken,
from the data sheet, which face.c waits for after the last one.

An image is the eight pages of eight pixel rows from the top down, and
each page is a byte for each column from left to right, with the top
pixel of the eight in bit 0, as the display takes it.  A rectangle is
the same, for its pages and its columns, so it can be sent to a window
of the display as it is.

It needs only Python.  The PNG files are four times the size of the
display, to look at.
"""
import math
import os
import struct
import sys
import zlib

WIDTH, HEIGHT = 128, 64

# The face
HEAD_X, HEAD_Y = 64, 32             # the middle of the head
HEAD_RX, HEAD_RY = 34, 31           # half its width and height
EYE_Y = 22
EYES_X = (51, 77)
EYE_R = 6                           # the white of an eye
BROW_Y = 12
MOUTH_X, MOUTH_Y = 64, 50           # the middle of the mouth

# The rectangles that change: pages and columns, the end not included
MOUTH_PAGES, MOUTH_COLS = (5, 8), (40, 88)
EYES_PAGES, EYES_COLS = (1, 4), (40, 88)

# The mouths, in the order of their numbers in face.h
MOUTHS = ['rest', 'mbp', 'fv', 'th', 'a', 'e', 'i', 'o', 'u', 'cons']

# The mouth of each allophone of the SP0256-AL2
ALLOPHONES = (
    "PA1 PA2 PA3 PA4 PA5 OY AY EH KK3 PP JH NN1 IH TT2 RR1 AX MM TT1 DH1 IY "
    "EY DD1 UW1 AO AA YY2 AE HH1 BB1 TH UH UW2 AW DD2 GG3 VV GG1 SH ZH RR2 "
    "FF KK2 KK1 ZZ NG LL WW XR WH YY1 CH ER1 ER2 OW DH2 SS NN2 HH2 OR AR YR "
    "GG2 EL BB2").split()
# How long each allophone is spoken, in ms, from the data sheet
DURATIONS = [10, 30, 50, 100, 200, 290, 170, 50, 80, 150, 400, 170, 60, 100,
             130, 50, 180, 80, 140, 170, 200, 50, 60, 70, 60, 130, 80, 90, 40,
             130, 70, 170, 250, 80, 120, 130, 80, 120, 130, 80, 110, 140, 120,
             150, 200, 80, 140, 250, 150, 90, 150, 110, 210, 170, 180, 60, 140,
             130, 240, 200, 250, 80, 140, 60]

GROUPS = {
    'rest': 'PA1 PA2 PA3 PA4 PA5',
    'mbp': 'MM PP BB1 BB2',
    'fv': 'FF VV',
    'th': 'TH DH1 DH2',
    'a': 'AA AE AY AW AR',
    'e': 'EH EY AX HH1 HH2 YR',
    'i': 'IY IH YY1 YY2',
    'o': 'AO OW OY OR UH',
    'u': 'UW1 UW2 WW WH RR1 RR2 ER1 ER2 XR SH ZH CH JH',
    'cons': 'TT1 TT2 DD1 DD2 NN1 NN2 NG KK1 KK2 KK3 GG1 GG2 GG3 SS ZZ LL EL',
}


class Image:
    def __init__(self):
        self.px = [[0] * WIDTH for _ in range(HEIGHT)]

    def set(self, x, y, on=1):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            self.px[y][x] = on

    def ellipse(self, cx, cy, rx, ry, fill=False, on=1, a0=0, a1=360):
        """the outline of an ellipse, or the part from a0 to a1 degrees,
        clockwise from the right as y goes down, or all of it filled"""
        if fill:
            for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
                for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                    if ((x - cx) / (rx + 0.5)) ** 2 + ((y - cy) / (ry + 0.5)) ** 2 <= 1.0:
                        self.set(x, y, on)
            return
        steps = int(4 * (rx + ry) * (a1 - a0) / 360) + 8
        for i in range(steps + 1):
            t = math.radians(a0 + (a1 - a0) * i / steps)
            self.set(cx + rx * math.cos(t), cy + ry * math.sin(t), on)

    def line(self, x0, y0, x1, y1, on=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            self.set(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, on)

    def rect(self, x0, y0, x1, y1, on=1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, on)

    def to_bytes(self, pages=(0, 8), cols=(0, WIDTH)):
        out = bytearray()
        for page in range(*pages):
            for x in range(*cols):
                b = 0
                for bit in range(8):
                    if self.px[page * 8 + bit][x]:
                        b |= 1 << bit
                out.append(b)
        return bytes(out)

    def png(self, name, scale=4):
        rows = b''
        for y in range(HEIGHT * scale):
            row = bytes(255 if self.px[y // scale][x // scale] else 0
                        for x in range(WIDTH * scale))
            rows += b'\0' + row

        def chunk(kind, data):
            c = struct.pack('>I', len(data)) + kind + data
            return c + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
        with open(name, 'wb') as f:
            f.write(b'\x89PNG\r\n\x1a\n')
            f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH * scale, HEIGHT * scale, 8, 0, 0, 0, 0)))
            f.write(chunk(b'IDAT', zlib.compress(rows, 9)))
            f.write(chunk(b'IEND', b''))


def draw_head(img):
    img.ellipse(HEAD_X, HEAD_Y, HEAD_RX, HEAD_RY)
    img.ellipse(HEAD_X, HEAD_Y, HEAD_RX - 0.6, HEAD_RY - 0.6)
    # ears
    for side in (-1, 1):
        img.ellipse(HEAD_X + side * (HEAD_RX + 3), HEAD_Y + 1, 4, 7,
                    a0=-90 if side > 0 else 90, a1=90 if side > 0 else 270)
    # nose
    img.line(64, 27, 62, 37)
    img.line(62, 37, 66, 37)


def draw_eyes(img, closed=False):
    for ex in EYES_X:
        img.line(ex - 7, BROW_Y + 1, ex + 6, BROW_Y - 1 if ex < 64 else BROW_Y + 1)
        if closed:
            img.ellipse(ex, EYE_Y - 2, EYE_R, 4, a0=20, a1=160)
        else:
            img.ellipse(ex, EYE_Y, EYE_R, EYE_R)
            img.rect(ex - 1, EYE_Y - 1, ex + 2, EYE_Y + 2)
            img.rect(ex - 2, EYE_Y, ex + 3, EYE_Y + 1)
            img.rect(ex, EYE_Y - 2, ex + 1, EYE_Y + 3)


def open_mouth(img, rx, ry, teeth=0, tongue=0):
    """an open mouth: lips around a dark opening, with a band of upper
    teeth teeth pixels high, and the tongue tongue pixels high at the
    bottom, both inside the opening"""
    x, y = MOUTH_X, MOUTH_Y
    img.ellipse(x, y, rx + 1, ry + 1, fill=True)
    img.ellipse(x, y, rx - 1, ry - 1, fill=True, on=0)
    # the teeth follow the upper lip, a pixel below it, and the tongue
    # lies on the lower lip in the middle: in each column, between the
    # lips is the dark opening
    for px in range(int(x - rx), int(x + rx) + 1):
        col = [img.px[py][px] for py in range(HEIGHT)]
        py = int(y - ry - 2)
        while py < y and not col[py]:           # down to the upper lip
            py += 1
        while py < y and col[py]:               # and through it
            py += 1
        dark = []
        while py < HEIGHT and not col[py]:      # the opening
            dark.append(py)
            py += 1
        if len(dark) < 3 or py > y + ry + 2:
            continue
        for py in dark[1:1 + teeth]:
            if py < dark[-1] - 1:
                img.set(px, py)
        if tongue and abs(px - x) < rx * 0.55:
            for py in dark[len(dark) - tongue:]:
                img.set(px, py)


def draw_mouth(img, mouth):
    x, y = MOUTH_X, MOUTH_Y
    if mouth == 'rest':
        img.ellipse(x, y - 6, 12, 7, a0=30, a1=150)
    elif mouth == 'mbp':
        img.line(x - 10, y, x + 10, y)
        img.line(x - 9, y + 1, x + 9, y + 1)
        img.ellipse(x, y - 1, 9, 2, a0=200, a1=340)
    elif mouth == 'fv':
        img.line(x - 11, y - 1, x + 11, y - 1)
        img.rect(x - 7, y, x + 7, y + 1)
        img.ellipse(x, y + 1, 10, 3, a0=0, a1=180)
    elif mouth == 'th':
        open_mouth(img, 10, 5, teeth=2)
        img.ellipse(x, y + 2, 4, 2, fill=True)
    elif mouth == 'a':
        open_mouth(img, 11, 8, teeth=2, tongue=3)
    elif mouth == 'e':
        open_mouth(img, 12, 7, teeth=2)
    elif mouth == 'i':
        open_mouth(img, 14, 5, teeth=2)
    elif mouth == 'o':
        open_mouth(img, 8, 8, tongue=2)
    elif mouth == 'u':
        img.ellipse(x, y, 6, 6, fill=True)
        img.ellipse(x, y, 3, 3, fill=True, on=0)
    elif mouth == 'cons':
        open_mouth(img, 10, 5, teeth=3)


def face(mouth='rest', closed=False):
    img = Image()
    draw_head(img)
    draw_eyes(img, closed)
    draw_mouth(img, mouth)
    return img


def check_rects():
    """everything that changes is inside its rectangle"""
    base = face()
    for m in MOUTHS:
        img = face(m)
        for y in range(HEIGHT):
            for x in range(WIDTH):
                if img.px[y][x] != base.px[y][x]:
                    assert MOUTH_PAGES[0] * 8 <= y < MOUTH_PAGES[1] * 8, (m, x, y)
                    assert MOUTH_COLS[0] <= x < MOUTH_COLS[1], (m, x, y)
    img = face(closed=True)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if img.px[y][x] != base.px[y][x]:
                assert EYES_PAGES[0] * 8 <= y < EYES_PAGES[1] * 8, ('eyes', x, y)
                assert EYES_COLS[0] <= x < EYES_COLS[1], ('eyes', x, y)


def c_array(name, data, comment):
    lines = ['/* %s */' % comment, 'static byte %s[%d] = {' % (name, len(data))]
    for i in range(0, len(data), 16):
        lines.append('  ' + ', '.join('0x%02x' % b for b in data[i:i + 16]) + ',')
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')
    return '\n'.join(lines) + '\n'


def main():
    check_rects()
    mouth_of = {}
    for m, names in GROUPS.items():
        for a in names.split():
            assert a not in mouth_of, a
            mouth_of[a] = MOUTHS.index(m)
    assert sorted(mouth_of) == sorted(ALLOPHONES), set(ALLOPHONES) ^ set(mouth_of)

    if len(sys.argv) == 3 and sys.argv[1] == '--png':
        os.makedirs(sys.argv[2], exist_ok=True)
        for m in MOUTHS:
            face(m).png(os.path.join(sys.argv[2], 'mouth_%s.png' % m))
        face(closed=True).png(os.path.join(sys.argv[2], 'blink.png'))

    mouth_bytes = (MOUTH_PAGES[1] - MOUTH_PAGES[0]) * (MOUTH_COLS[1] - MOUTH_COLS[0])
    eyes_bytes = (EYES_PAGES[1] - EYES_PAGES[0]) * (EYES_COLS[1] - EYES_COLS[0])
    out = ['/*',
           ' * face_images.h - the images of the talking face, made by',
           ' * tools/mkface.py, do not edit.',
           ' */',
           '',
           '/* the rectangles of the mouth and of the eyes, pages and columns */',
           '#define MOUTH_PAGE   %d' % MOUTH_PAGES[0],
           '#define MOUTH_PAGES  %d' % (MOUTH_PAGES[1] - MOUTH_PAGES[0]),
           '#define MOUTH_COL    %d' % MOUTH_COLS[0],
           '#define MOUTH_COLS   %d' % (MOUTH_COLS[1] - MOUTH_COLS[0]),
           '#define MOUTH_SIZE   %d' % mouth_bytes,
           '#define EYES_PAGE    %d' % EYES_PAGES[0],
           '#define EYES_PAGES   %d' % (EYES_PAGES[1] - EYES_PAGES[0]),
           '#define EYES_COL     %d' % EYES_COLS[0],
           '#define EYES_COLS    %d' % (EYES_COLS[1] - EYES_COLS[0]),
           '#define EYES_SIZE    %d' % eyes_bytes,
           '#define MOUTHS       %d' % len(MOUTHS),
           '']
    out.append(c_array('face_image', face().to_bytes(), 'the face, with its mouth closed and its eyes open'))
    data = b''.join(face(m).to_bytes(MOUTH_PAGES, MOUTH_COLS) for m in MOUTHS)
    out.append(c_array('face_mouths', data, 'the mouths: ' + ', '.join(MOUTHS)))
    data = face().to_bytes(EYES_PAGES, EYES_COLS) + face(closed=True).to_bytes(EYES_PAGES, EYES_COLS)
    out.append(c_array('face_eyes', data, 'the eyes, open and closed'))
    out.append(c_array('face_mouth_of', bytes(mouth_of[a] for a in ALLOPHONES),
                       'the mouth of each allophone'))
    assert len(DURATIONS) == 64 and all(d % 10 == 0 for d in DURATIONS)
    out.append(c_array('face_tens', bytes(d // 10 for d in DURATIONS),
                       'how long each allophone is spoken, in tens of ms'))
    print('\n'.join(out), end='')


if __name__ == '__main__':
    main()
