#!/usr/bin/env python3
"""
compare_elf.py - check tts against a model of it, in the Run/02 emulator

    tools/compare_elf.py -r ROM [-d sp0256.dic] file ...

Runs programs/tts.elfos in Run/02, with its Elf/OS kernel calls, on each
text file, writing the allophones to a phrase file, and checks that they
are the same as those of the model below, which does what tts.c does in
Python.  It looks words up in the dictionary file with mkdict.py, and
converts the others with cts.py of ../CTS256A-AL2/tools, which needs the
ROM of the CTS256A-AL2 as cts.py does, CTS_ROM or -r.

It prints how many instructions tts took for each word, which leaves out
the time the kernel takes to read the file, as Run/02 does the kernel
calls itself.
"""
import argparse
import difflib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'CTS256A-AL2', 'tools'))
import mkdict  # noqa: E402
from cts import CTS, LABELS, mark  # noqa: E402
from mkrules import read_rom  # noqa: E402

A = mkdict.ALLOPHONES
CLASSES = ("262dcbaw223e4163wwwwwwwwww543f36"
           "gwwwwwwwwwwwwwwwwwwwwwwwwww23233"
           "3wwwwwwwwwwwwwwwwwwwwwwwwww2323")
SYMBOLS = ['AND', 'PERCENT', 'DOLLARS', 'NUMBER', 'PLUS', 'EQUALS', 'AT']
PAUSES = [A['PA1'], A['PA1'], A['PA2'], A['PA3'], A['PA4'], A['PA5'],
          A['PA5']]
HISSING = {A[n] for n in ('SS', 'ZZ', 'SH', 'ZH', 'CH', 'JH')}
VOICELESS = {A[n] for n in ('PP', 'TT1', 'TT2', 'KK1', 'KK2', 'KK3', 'FF',
                            'TH')}
ONES = ['ZERO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN',
        'EIGHT', 'NINE', 'TEN', 'ELEVEN', 'TWELVE', 'THIRTEEN', 'FOURTEEN',
        'FIFTEEN', 'SIXTEEN', 'SEVENTEEN', 'EIGHTEEN', 'NINETEEN']
TENS = ['TWENTY', 'THIRTY', 'FORTY', 'FIFTY', 'SIXTY', 'SEVENTY', 'EIGHTY',
        'NINETY']
SCALES = ['HUNDRED', 'THOUSAND', 'MILLION']
WORD_MAX = 48


class TTS:
    """tts.c in Python"""

    def __init__(self, dic, cts):
        self.dic = dic
        self.cts = cts
        self.out = []
        self.word = []
        self.digits = True
        self.spell = False
        self.pend = 0
        self.pause = 0
        self.spoken = False
        self.unknown = []

    def add_pause(self, p):
        self.pause = max(self.pause, p)

    def begin_word(self):
        if self.spoken and self.pause:
            self.out.append(PAUSES[self.pause])
            if self.pause == 6:
                self.out.append(A['PA5'])
        self.pause = 0
        self.spoken = True

    def say_dict(self, w, s):
        allo = mkdict.lookup(self.dic, w)
        if not allo:
            return False
        self.begin_word()
        self.out += allo
        if s:
            a = allo[-1]
            if a in HISSING:
                self.out += [A['IH'], A['ZZ']]
            elif a in VOICELESS:
                self.out.append(A['SS'])
            else:
                self.out.append(A['ZZ'])
        return True

    def say_by_rules(self, w):
        self.unknown.append(w)
        self.begin_word()
        allo = self.cts.convert(mark(w.encode('latin-1') + b'\r'))
        if allo and allo[-1] == A['PA3']:
            allo = allo[:-1]
        self.out += allo

    def say_word(self, w):
        if self.say_dict(w, False):
            return
        if "'" in w:
            if len(w) > 2 and w.endswith("'S"):
                if self.say_dict(w[:-2], True):
                    return
            other = w.replace("'", '')
            if other and self.say_dict(other, False):
                return
        self.say_by_rules(w)

    def say_number_word(self, w):
        self.add_pause(2)
        self.say_word(w)

    def say_group(self, n):
        if n >= 100:
            self.say_number_word(ONES[n // 100])
            self.say_number_word(SCALES[0])
            n %= 100
        if n >= 20:
            self.say_number_word(TENS[n // 10 - 2])
            n %= 10
        if n > 0:
            self.say_number_word(ONES[n])

    def say_number(self, w):
        if self.spell or len(w) > 9 or (len(w) > 1 and w[0] == '0'):
            for d in w:
                self.say_number_word(ONES[int(d)])
            return
        zero = True
        while w:
            k = (len(w) - 1) // 3
            g = len(w) - 3 * k
            n = int(w[:g])
            w = w[g:]
            if n > 0:
                self.say_group(n)
                if k > 0:
                    self.say_number_word(SCALES[k])
                zero = False
        if zero:
            self.say_number_word(ONES[0])

    def flush_word(self):
        w = ''.join(self.word).strip("'")
        if w:
            if self.digits:
                self.say_number(w)
            else:
                self.say_word(w)
        self.word = []
        self.digits = True
        self.spell = False

    @staticmethod
    def class_of(c):
        if 32 <= c < 127:
            return CLASSES[c - 32]
        if c in (9, 10, 13):
            return '2'
        return '3'

    def add_char(self, c):
        if len(self.word) == WORD_MAX:
            self.flush_word()
        if c >= ord('a'):
            c -= 32
        if not ord('0') <= c <= ord('9'):
            self.digits = False
        self.word.append(chr(c))

    def separator(self, k):
        self.flush_word()
        if k >= 'a':
            self.add_pause(2)
            self.say_word(SYMBOLS[ord(k) - ord('a')])
            self.add_pause(2)
        else:
            self.add_pause(int(k))

    def feed(self, c):
        k = self.class_of(c)
        if self.pend:
            p = self.pend
            self.pend = 0
            if ord('0') <= c <= ord('9'):
                if p == ord('.'):
                    self.flush_word()
                    self.say_number_word('POINT')
                    self.spell = True
                self.add_char(c)
                return
            self.separator(self.class_of(p))
        if k == 'w':
            self.add_char(c)
        elif c in (ord(','), ord('.')) and self.word and self.digits:
            self.pend = c
        else:
            self.separator(k)

    def finish(self):
        if self.pend:
            self.separator(self.class_of(self.pend))
            self.pend = 0
        self.flush_word()
        if self.spoken:
            self.add_pause(3)
            self.begin_word()
        self.spoken = False
        self.pause = 0

    def convert(self, text):
        for c in text:
            self.feed(c)
        self.finish()
        return self.out


def run_tts(run02, folder, args):
    """run tts with args in folder, return the instructions it took"""
    cmd = "%s -4 -nd '-a=tts %s' -b=tts.elfos" % (run02, args)
    r = subprocess.run(['script', '-qfec', cmd, '/dev/null'], cwd=folder,
                       capture_output=True, timeout=3600)
    for line in r.stdout.decode('latin1').splitlines():
        if line.startswith('Instructions executed'):
            return int(line.split(':')[1])
    return None


def main():
    ap = argparse.ArgumentParser(
        description='check tts against a model of it in Run/02')
    ap.add_argument('-r', '--rom', default=os.environ.get('CTS_ROM'))
    ap.add_argument('-d', '--dict',
                    default=os.path.join(HERE, '..', 'sp0256.dic'))
    ap.add_argument('--run02', default='/opt/elfc/run02')
    ap.add_argument('--program', default=os.path.join(
        HERE, '..', 'programs', 'tts.elfos'))
    ap.add_argument('files', nargs='+')
    o = ap.parse_args()
    if not o.rom:
        ap.error('give the ROM with -r or CTS_ROM')
    cts = CTS(read_rom(o.rom))
    dic = open(o.dict, 'rb').read()

    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        shutil.copy(o.program, os.path.join(tmp, 'tts.elfos'))
        shutil.copy(o.dict, os.path.join(tmp, 'sp0256.dic'))
        for name in o.files:
            text = open(name, 'rb').read()
            shutil.copy(name, os.path.join(tmp, 'text.txt'))
            out = os.path.join(tmp, 'out.say')
            if os.path.exists(out):
                os.remove(out)
            n = run_tts(o.run02, tmp, '-o out.say -f text.txt')

            t = TTS(dic, cts)
            model = t.convert(text)
            elf = list(open(out, 'rb').read()) if os.path.exists(out) else []
            if elf and elf[-1] == 0xFF:
                elf.pop()
            words = len(text.split())
            per = (', %d instructions a word' % (n // max(words, 1))
                   if n else '')
            if elf == model:
                print('%s: the same, %d words, %d by the rules, %d '
                      'allophones%s' % (name, words, len(t.unknown),
                                        len(model), per))
                continue

            bad += 1
            print('%s: DIFFERENT' % name)
            a = [LABELS[x] for x in model]
            b = [LABELS[x & 0x3F] for x in elf]
            shown = 0
            for op in difflib.SequenceMatcher(None, a, b,
                                              autojunk=False).get_opcodes():
                if op[0] != 'equal' and shown < 10:
                    print('  model: %s' % ' '.join(a[max(0, op[1] - 8):
                                                     op[2] + 3]))
                    print('  elf:   %s' % ' '.join(b[max(0, op[3] - 8):
                                                     op[4] + 3]))
                    shown += 1
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
