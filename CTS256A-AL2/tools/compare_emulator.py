#!/usr/bin/env python3
"""
compare_emulator.py - check the model against the emulator of the chip

    tools/compare_emulator.py -r ROM -e EMULATOR [-x EPROM] file ...

Gives the lines of each file, a few hundred at a time, to the
CTS256A-AL2 emulator, which runs the ROM of the chip in an emulated
TMS7000, and checks that cts.py gives the same allophones.  EMULATOR is
the cts256a-al2 program of the emulator.  An exception EPROM is given to
the emulator at E000, where cts_eprom makes them.

The lines are sent with CR LF, as the emulated chip converts the text
only after a CR, and buffers up to 1792 characters.  The emulator sends
the chip the next characters while it converts, as they would come in on
a serial line, so where a rule looks at the next word, as <[THE]<# does,
the emulator can see part of it, or what was in its buffer before.  The
model always sees all of it, so a line that ends with "the" can differ.
"""
import argparse
import difflib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from cts import CTS, LABELS, mark  # noqa: E402
from mkrules import read_rom  # noqa: E402

LINES = 200


def emulate(emulator, text, eprom):
    args = [emulator, '-n'] + (['-x' + eprom, '-aE000'] if eprom else [])
    r = subprocess.run(args, input=text, capture_output=True, timeout=120)
    out = r.stdout.decode('latin1')
    out = out.split('\n\n', 1)[1].rsplit('Conversion complete', 1)[0]
    return out.replace('\x1b[m', '').split()


def main():
    ap = argparse.ArgumentParser(description='check cts.py against the CTS256A-AL2 emulator')
    ap.add_argument('-r', '--rom', default=os.environ.get('CTS_ROM'))
    ap.add_argument('-e', '--emulator', required=True)
    ap.add_argument('-x', '--eprom')
    ap.add_argument('files', nargs='+')
    o = ap.parse_args()
    if not o.rom:
        ap.error('give the ROM with -r or CTS_ROM')
    rom = read_rom(o.rom)
    eprom = open(o.eprom, 'rb').read() if o.eprom else None

    bad = 0
    for name in o.files:
        lines = open(name, 'rb').read().splitlines()
        for i in range(0, len(lines), LINES):
            text = b'\r\n'.join(lines[i:i + LINES]) + b'\r\n'
            a = emulate(o.emulator, text, o.eprom)
            b = [LABELS[x] for x in CTS(rom, eprom).convert(mark(text + b'\r'))]
            if a == b:
                continue
            bad += 1
            print('%s, lines %d to %d:' % (name, i + 1, i + LINES))
            for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
                if op[0] != 'equal':
                    print('  emulator: %s' % ' '.join(a[max(0, op[1] - 8):op[2] + 3]))
                    print('  model:    %s' % ' '.join(b[max(0, op[3] - 8):op[4] + 3]))
        print('%s: %d lines, %s' % (name, len(lines), 'differences above' if bad else 'the same'))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
