#!/usr/bin/env python3
"""
cts.py - a model of the CTS256A-AL2 in Python, to test cts.lib against

    tools/cts.py -r ROM [-x EPROM] [text ...]      the allophones of the text
    tools/cts.py -r ROM [-x EPROM] -f file         ... of a file
    tools/cts.py -r ROM [-x EPROM] --rules         list the rules

ROM is the ROM image of the chip, or CTS256A_AL2.cpp of the emulator, as
for mkrules.py, or the CTS_ROM environment variable names it.  EPROM is
an exception EPROM image.  The text ends with a CR, as the emulator gives
the chip at the end of its input.

The model takes the same steps as the ROM, from its disassembly, with the
names of the registers of the chip.  It was checked against the emulator
of the chip, see compare_emulator.py, and cts.lib is checked against it,
see compare_elf.py.  The model sees all of the text at once, where the
chip sees what it has been sent so far, see the README.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkrules import read_rom  # noqa: E402

LABELS = ('PA1 PA2 PA3 PA4 PA5 OY AY EH KK3 PP JH NN1 IH TT2 RR1 AX MM TT1 '
          'DH1 IY EY DD1 UW1 AO AA YY2 AE HH1 BB1 TH UH UW2 AW DD2 GG3 VV '
          'GG1 SH ZH RR2 FF KK2 KK1 ZZ NG LL WW XR WH YY1 CH ER1 ER2 OW DH2 '
          'SS NN2 HH2 OR AR YR GG2 EL BB2').split()

PATTERNS = {0x07: "'", 0x09: '#', 0x0A: '.', 0x0B: '%', 0x0C: '&', 0x0D: '@',
            0x0E: '^', 0x0F: '+', 0x10: ':', 0x11: '*', 0x12: '>', 0x13: '<',
            0x14: '?'}

PUNCT = 0xF78C      # the rules of the characters that are not letters or digits
DIGITS = 0xFF8E     # the rules of the digits
LETTERS = 0xFFBE    # the addresses of the rules of A to Z
CLASSES = 0xF526    # the classes of A to Z
EXCEPTIONS = 0x0A3  # the addresses of the lists of an exception EPROM


class Fail(Exception):
    pass


def mark(text):
    """the text as the chip keeps it, in upper case, with bit 7 set on the
    characters that end a word"""
    out = bytearray()
    for c in text:
        if 0x61 <= c <= 0x7A:
            c -= 0x20
        if c == 0x27 or 0x30 <= c <= 0x39 or 0x41 <= c <= 0x7B:
            out.append(c)
        else:
            out.append(c | 0x80)
    return out


class CTS:
    def __init__(self, rom, eprom=None):
        self.rom = rom
        self.ep = eprom
        if eprom:
            self.epbase = ((eprom[EXCEPTIONS] << 8) | eprom[EXCEPTIONS + 1]) & 0xF000

    def mem(self, a):
        """a byte of the address space of the chip"""
        if a >= 0xF000:
            return self.rom[a - 0xF000]
        if self.ep and self.epbase <= a < self.epbase + len(self.ep):
            return self.ep[a - self.epbase]
        return 0xFF

    def word(self, a):
        return (self.mem(a) << 8) | self.mem(a + 1)

    def ch(self, i):
        """the character at i, without bit 7, and whether it ends a word"""
        b = self.buf[i] if 0 <= i < len(self.buf) else 0
        return b & 0x7F, (b & 0x80) != 0

    @staticmethod
    def code(c):
        if c >= 0x61:
            c -= 0x20
        return (c - 0x20) & 0xFF

    def cls(self, code):
        """the class of the character with the code, 0 if not a letter"""
        if 0x21 <= code <= 0x3A:
            return self.rom[CLASSES - 0xF000 + code - 0x21]
        return 0

    @staticmethod
    def is_letter(c):
        return 0x41 <= c <= 0x5A or 0x61 <= c <= 0x7A

    def find(self, p, n):
        """the nth byte from p with bit 6 set"""
        while True:
            if self.mem(p) & 0x40:
                n -= 1
                if n == 0:
                    return p
            p += 1

    def next(self, left):
        """the code of the next character of a context"""
        if left:
            c, _ = self.ch(self.r17)
            self.r17 -= 1
        else:
            c, _ = self.ch(self.r3)
            self.r3 += 1
        self.r22 = self.code(c)
        return self.cls(self.r22)

    def unread(self, left):
        if left:
            self.r17 += 1
        else:
            self.r3 -= 1

    def skip(self, left, mask):
        while self.next(left) & mask:
            pass
        self.unread(left)

    def raw(self):
        c, _ = self.ch(self.r3)
        self.r3 += 1
        return c

    def context(self, left):
        """match the context of the rule at r21, raise Fail if it does not"""
        while True:
            if left:
                self.r21 -= 1
                if self.mem(self.r21) & 0x80:
                    self.r21 += 1
                    return
            else:
                self.r21 += 1
                if self.mem(self.r21) & 0x40:
                    self.r21 -= 1
                    return
            c = self.next(left)
            a = self.mem(self.r21)
            if a >= 0x15 or a == 0x07:
                if self.r22 != a:
                    raise Fail
            elif a == 0x13:                                     # <
                if c:
                    raise Fail
            elif a == 0x11:                                     # *
                if not c & 0x08:
                    raise Fail
                self.skip(left, 0x08)
            elif a == 0x10:                                     # :
                if c & 0x08:
                    self.skip(left, 0x08)
                else:
                    self.unread(left)
            elif a == 0x09:                                     # #
                if not c & 0x80:
                    raise Fail
                self.skip(left, 0x80)
            elif a == 0x14:                                     # ?
                if not c & 0x80 or not self.next(left) & 0x80:
                    raise Fail
                self.skip(left, 0x80)
            elif a in (0x0A, 0x0E, 0x0F, 0x12):                 # . ^ + >
                if not c & {0x0A: 0x40, 0x0E: 0x08, 0x0F: 0x04, 0x12: 0x02}[a]:
                    raise Fail
            elif a in (0x0C, 0x0D):                             # & @
                if c & (0x20 if a == 0x0C else 0x10):
                    continue
                self.next(left)
                if not (self.r22 in (0x23, 0x33) or (a == 0x0D and self.r22 == 0x34)):
                    raise Fail
                # the H two characters to the right of r17, even in a
                # right context, where it is before the brackets
                self.r3 = self.r17 + 2
                if self.raw() != 0x48:
                    raise Fail
            elif a == 0x0B:                                     # %
                self.suffix(c)
            else:
                raise Fail

    def suffix(self, c):
        if c & 0x01:                                # E ER ERS ES ED ELY
            a = self.raw()
            if a == 0x52:
                if self.raw() != 0x53:
                    self.r3 -= 1
            elif a == 0x4C:
                if self.raw() != 0x59:
                    raise Fail
            elif a not in (0x53, 0x44):
                self.r3 -= 1
        else:                                       # ING MENT OR
            rest = {0x29: b'NG', 0x2D: b'ENT', 0x2F: b'R'}.get(self.r22)
            if rest is None:
                raise Fail
            for x in rest:
                if self.raw() != x:
                    raise Fail
        a = self.raw()
        self.r3 -= 1
        if self.is_letter(a):
            raise Fail

    def brackets(self, letter):
        """match the brackets of the rule at r21 with the text at r3"""
        self.r19 = self.r3
        if self.mem(self.r21) == 0xFF:
            self.r15 = self.r3
            return True
        if not letter:
            self.r3 -= 1
        while True:
            c, _ = self.ch(self.r3)
            self.r3 += 1
            a = self.mem(self.r21)
            if (a & 0x3F) != self.code(c):
                self.r3 = self.r19
                return False
            if a & 0x80:
                self.r15 = self.r3
                return True
            self.r21 += 1

    def apply(self, rules, letter, exception=False):
        """apply the first rule of the list that matches the character at r3 - 1"""
        self.r21 = self.find(rules, 1)
        while True:
            if not self.brackets(letter):
                self.r21 = self.find(self.r21 + 1, 2)
                continue
            if exception and self.mem(self.r21) == 0xFF:
                return False
            self.r29 = self.r17
            try:
                self.context(False)
            except Fail:
                self.r3, self.r17 = self.r19, self.r29
                self.r21 = self.find(self.r21 + 1, 2)
                continue
            while not self.mem(self.r21) & 0x40:
                self.r21 -= 1
            try:
                self.context(True)
            except Fail:
                self.r3, self.r17 = self.r19, self.r29
                self.r21 = self.find(self.r21, 3)
                continue
            self.r21 = self.find(self.r21, 2)
            while True:
                a = self.mem(self.r21)
                if a == 0xFF:
                    break
                self.out.append(a & 0x3F)
                self.r21 += 1
                if a & 0x80:
                    break
            self.r3 = self.r15
            return True

    def first(self):
        """read the character to convert: its list of rules, its list of
        exceptions, whether it is a letter and whether it ends a word"""
        self.r17 = self.r3 - 1
        c, ends = self.ch(self.r3)
        self.r3 += 1
        if self.is_letter(c):
            n = (c & 0xDF) - 0x41
            return self.word(LETTERS + 2 * n), n, True, ends
        if 0x30 <= c <= 0x39:
            return DIGITS, 26, False, ends
        return PUNCT, 26, False, ends

    def convert(self, text):
        """the allophones of text, as mark() gives it"""
        self.buf = bytearray(b' ') + text   # the chip starts with a space
        self.r3 = 1
        self.out = []
        word = True
        while self.r3 < len(self.buf):
            if word and self.ep:
                start = self.r3
                _, n, letter, ends = self.first()
                a = EXCEPTIONS + 2 * n
                if self.apply((self.ep[a] << 8) | self.ep[a + 1], letter, True):
                    word = ends
                    continue
                self.r3 = start
            rules, _, letter, word = self.first()
            self.apply(rules, letter)
        return self.out

    def list_rules(self):
        for n in range(26):
            print('%c:' % (0x41 + n))
            for r in self.rules_of(self.word(LETTERS + 2 * n), chr(0x41 + n)):
                print('    ' + r)
        for name, start in (('digits', DIGITS), ('others', PUNCT)):
            print(name + ':')
            for r in self.rules_of(start, ''):
                print('    ' + r)

    def rules_of(self, start, letter):
        """the rules of a list of the ROM, which ends where the next starts"""
        ends = sorted([self.word(LETTERS + 2 * n) for n in range(26)] + [DIGITS, PUNCT, LETTERS])
        end = ends[ends.index(start) + 1]
        p = self.find(start, 1)
        while p < end:
            yield self.rule_at(p, start, letter)
            p = self.find(self.find(p, 2) + 1, 1)

    def rule_at(self, p, start, letter, mem=None):
        mem = mem or self.mem
        q = p
        while q > start and not mem(q - 1) & 0x80:
            q -= 1
        left = [mem(a) for a in range(q, p)]
        inside = []
        if mem(p) != 0xFF:
            while True:
                inside.append(mem(p) & 0x3F)
                if mem(p) & 0x80:
                    break
                p += 1
        p += 1
        right = []
        while not mem(p) & 0x40:
            right.append(mem(p))
            p += 1
        allo = []
        if mem(p) != 0xFF:
            while True:
                allo.append(LABELS[mem(p) & 0x3F])
                if mem(p) & 0x80:
                    break
                p += 1

        def ctx(xs):
            return ''.join(chr(x + 0x20) if x >= 0x15 else PATTERNS.get(x, '{%02X}' % x) for x in xs)
        return '%s[%s%s]%s = %s' % (ctx(left), letter, ''.join(chr(x + 0x20) for x in inside),
                                    ctx(right), ' '.join(allo))

    def list_exceptions(self):
        for n in range(27):
            a = EXCEPTIONS + 2 * n
            p = start = (self.ep[a] << 8) | self.ep[a + 1]
            letter = chr(0x41 + n) if n < 26 else ''
            print((letter or 'others') + ':')
            p = self.find(p, 1)
            while self.mem(p) != 0xFF:
                print('    ' + self.rule_at(p, start, letter))
                p = self.find(self.find(p, 2) + 1, 1)


def main():
    ap = argparse.ArgumentParser(description='the allophones of text, as the CTS256A-AL2 makes them')
    ap.add_argument('-r', '--rom', default=os.environ.get('CTS_ROM'),
                    help='the ROM image, or CTS256A_AL2.cpp of the emulator')
    ap.add_argument('-x', '--eprom', help='an exception EPROM image')
    ap.add_argument('-f', '--file', help='convert the text of a file')
    ap.add_argument('-b', '--binary', action='store_true',
                    help='write the allophones as bytes, as a phrase file for say')
    ap.add_argument('--rules', action='store_true', help='list the rules, and the exceptions')
    ap.add_argument('text', nargs='*')
    o = ap.parse_args()
    if not o.rom:
        ap.error('give the ROM with -r or CTS_ROM')
    cts = CTS(read_rom(o.rom), open(o.eprom, 'rb').read() if o.eprom else None)

    if o.rules:
        cts.list_rules()
        if o.eprom:
            print('\nexceptions:')
            cts.list_exceptions()
        return

    if o.file:
        text = open(o.file, 'rb').read()
    elif o.text:
        text = ' '.join(o.text).encode('latin1')
    else:
        text = sys.stdin.buffer.read()
    allophones = cts.convert(mark(text + b'\r'))
    if o.binary:
        sys.stdout.buffer.write(bytes(allophones) + b'\xff')
    else:
        print(' '.join(LABELS[a] for a in allophones))


if __name__ == '__main__':
    main()
