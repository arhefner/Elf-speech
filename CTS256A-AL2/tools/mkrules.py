#!/usr/bin/env python3
"""
mkrules.py - make rules.inc, the rules of the CTS256A-AL2, from its ROM

    tools/mkrules.py ROM > libcts/rules.inc

ROM is a 4K image of the ROM of the chip (F000-FFFF), or CTS256A_AL2.cpp of
the CTS256A-AL2 emulator, which has the image as a C array.  rules.inc has
the parts of the ROM that the conversion uses, for cts.asm: the classes of
the letters, where the list of rules of each character starts, and the
rules.
"""
import re
import sys

RULES = 0xF78C      # the first rule, of the punctuation
RULES_END = 0xFFBE  # the end of the rules, where the list of letters is
LETTERS = 0xFFBE    # the address of the rules of each letter, A to Z
DIGITS = 0xFF8E     # the rules of the digits
CLASSES = 0xF526    # the class of each letter, A to Z


def read_rom(name):
    data = open(name, 'rb').read()
    if len(data) == 0x1000:
        return data
    src = data.decode('latin1')
    i = src.index('CTS256A_AL2_ROM[0x1000]')
    body = src[src.index('{', i):src.index('};', i)]
    rom = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body))
    if len(rom) != 0x1000:
        sys.exit('mkrules: the ROM in %s is not 4K' % name)
    return rom


def asm_table(name, op, values, per_line):
    lines = []
    for i in range(0, len(values), per_line):
        label = (name + ':') if i == 0 else ''
        lines.append('%-12s%-6s%s' % (label, op, ','.join(values[i:i + per_line])))
    return '\n'.join(lines) + '\n'


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: mkrules.py ROM > rules.inc')
    rom = read_rom(sys.argv[1])

    def byte(a):
        return rom[a - 0xF000]

    def word(a):
        return (byte(a) << 8) | byte(a + 1)

    lists = [word(LETTERS + 2 * i) - RULES for i in range(26)]
    lists += [DIGITS - RULES, 0]

    print('; rules.inc - the rules of the CTS256A-AL2, made from its ROM by')
    print('; tools/mkrules.py, do not edit.')
    print(';')
    print('; classes is the class of each letter, A to Z, and lists is where the')
    print('; rules of each character start: A to Z, the digits and the others.')
    print('; They have to be on the page of the variables, see cts.asm.  rules')
    print('; is the ROM from %04X to %04X.' % (RULES, RULES_END - 1))
    print()
    print(asm_table('classes', 'db', ['0%02xh' % byte(CLASSES + i) for i in range(26)], 13))
    print(asm_table('lists', 'dw', ['rules+%d' % x for x in lists], 7))
    print('; The byte before the first rule ends its left context, as in the ROM.')
    print()
    print('            db    0%02xh' % byte(RULES - 1))
    print(asm_table('rules', 'db', ['0%02xh' % byte(a) for a in range(RULES, RULES_END)], 12), end='')


if __name__ == '__main__':
    main()
