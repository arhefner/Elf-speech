#!/usr/bin/env python3
"""
compare_elf.py - check cts256 against the model, in the Run/02 emulator

    tools/compare_elf.py -r ROM [-x EPROM] file ...

Runs programs/cts256.elfos in Run/02, with its Elf/OS kernel calls, on
each text file, writing the allophones to a phrase file, and checks that
they are the same as those of cts.py.  ROM is as for cts.py.  The files
are copied to a temporary folder, as cts256 takes names of up to 19
characters.

The model sees all of the text, and cts256 the next word, so the two
differ only where a rule looks further ahead than that, which no rule of
the ROM does.  cts256 cuts a word of more than 200 characters, and so
does the text given to the model, see cut().
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
from cts import CTS, LABELS, mark  # noqa: E402
from mkrules import read_rom  # noqa: E402


def cut(text):
    """the text as cts256 has it, with a CR where it cuts a long word: when
    the text it holds, the last word and the one before it, which it keeps
    for the right contexts, comes to 200 characters"""
    out = bytearray()
    held = prev = 0
    for c in text:
        out.append(c)
        if c & 0x80:
            held, prev = prev, len(out)
        elif len(out) - held >= 200:
            out.append(0x8D)
            held = prev = len(out)
    return out


def run_cts256(run02, program, folder, args):
    """run cts256 with args in folder, return the instructions it took"""
    cmd = "%s -4 -nd '-a=cts256 %s' -b=%s" % (run02, args, program)
    r = subprocess.run(['script', '-qfec', cmd, '/dev/null'], cwd=folder,
                       capture_output=True, timeout=3600)
    for line in r.stdout.decode('latin1').splitlines():
        if line.startswith('Instructions executed'):
            return int(line.split(':')[1])
    return None


def main():
    ap = argparse.ArgumentParser(description='check cts256 against the model in Run/02')
    ap.add_argument('-r', '--rom', default=os.environ.get('CTS_ROM'))
    ap.add_argument('-x', '--eprom')
    ap.add_argument('--run02', default='/opt/elfc/run02')
    ap.add_argument('--program', default=os.path.join(HERE, '..', 'programs', 'cts256.elfos'))
    ap.add_argument('files', nargs='+')
    o = ap.parse_args()
    if not o.rom:
        ap.error('give the ROM with -r or CTS_ROM')
    rom = read_rom(o.rom)
    eprom = open(o.eprom, 'rb').read() if o.eprom else None

    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        program = os.path.join(tmp, 'cts256.elfos')
        shutil.copy(o.program, program)
        args = '-o out.say -f text.txt'
        if eprom:
            shutil.copy(o.eprom, os.path.join(tmp, 'exc.bin'))
            args += ' -x exc.bin'
        for name in o.files:
            text = open(name, 'rb').read()
            shutil.copy(name, os.path.join(tmp, 'text.txt'))
            out = os.path.join(tmp, 'out.say')
            if os.path.exists(out):
                os.remove(out)
            n = run_cts256(o.run02, program, tmp, args)

            model = CTS(rom, eprom).convert(cut(mark(text + b'\r')))
            elf = list(open(out, 'rb').read()) if os.path.exists(out) else []
            if elf and elf[-1] == 0xFF:
                elf.pop()
            per = ', %d instructions a character' % (n // max(len(text), 1)) if n else ''
            if elf == model:
                print('%s: the same, %d allophones%s' % (name, len(model), per))
                continue

            bad += 1
            print('%s: DIFFERENT' % name)
            a = [LABELS[x] for x in model]
            b = [LABELS[x & 0x3F] for x in elf]
            shown = 0
            for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
                if op[0] != 'equal' and shown < 10:
                    print('  model: %s' % ' '.join(a[max(0, op[1] - 8):op[2] + 3]))
                    print('  elf:   %s' % ' '.join(b[max(0, op[3] - 8):op[4] + 3]))
                    shown += 1
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
