#!/usr/bin/env python3
"""
dis7000.py - disassemble the ROM of the CTS256A-AL2, or an exception EPROM

    tools/dis7000.py ROM [address ...] > rom.lst
    tools/dis7000.py --base E000 EPROM [address ...] > eprom.lst

A recursive disassembler for the TMS7000: it follows the code from the
reset and interrupt vectors of the ROM (F000-FFFF), and from the hex
addresses given, and shows the rest as data.  The rules engine was ported
from its output, with the code reached from these addresses, which are
reached through tables:

    F514 F69E F6B4 F601 F6D7 F6E2 F6BF F6C7 F5EA F5B9 F6CF F5B0 F5CC F71B F77F

An exception EPROM image made by cts_eprom is at E000, its code starts
at E005 and E007.  ROM is as for mkrules.py.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkrules import read_rom  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--base', default='F000', help='the address of the image, in hex')
ap.add_argument('image')
ap.add_argument('addresses', nargs='*')
o = ap.parse_args()
BASE = int(o.base, 16)
rom = read_rom(o.image) if BASE == 0xF000 else open(o.image, 'rb').read()
END = BASE + len(rom) - 1
extra = [int(x, 16) for x in o.addresses]


def rd(a):
    return rom[a - BASE] if BASE <= a <= END else None

def w(a):
    return (rd(a) << 8) | rd(a + 1)

ALU = ['?', '?', 'MOV', 'AND', 'OR', 'XOR', 'BTJO', 'BTJZ', 'ADD', 'ADC', 'SUB', 'SBB', 'MPY', 'CMP', 'DAC', 'DSB']
UN = ['?', 'TST', 'DEC', 'INC', 'INV', 'CLR', 'XCHB', 'SWAP', 'PUSH', 'POP', 'DJNZ', 'DECD', 'RR', 'RRC', 'RL', 'RLC']
JC = ['JMP', 'JN', 'JZ', 'JC', 'JP', 'JPZ', 'JNZ', 'JNC']

def rel(pc, off):
    return (pc + (off - 256 if off > 127 else off)) & 0xFFFF

def R(n):
    return 'R%d' % n

def dis(pc):
    """returns (text, length, targets, falls_through)"""
    op = rd(pc)
    hi, lo = op >> 4, op & 15
    b1 = rd(pc + 1); b2 = rd(pc + 2); b3 = rd(pc + 3)
    simple = {0: 'NOP', 1: 'IDLE', 5: 'EINT', 6: 'DINT', 7: 'SETC', 8: 'POP ST', 9: 'STSP',
              0x0A: 'RETS', 0x0B: 'RETI', 0x0D: 'LDSP', 0x0E: 'PUSH ST'}
    if op in simple:
        return simple[op], 1, [], op not in (0x0A, 0x0B)
    if hi in (1, 2, 3, 4, 5, 6, 7) and lo >= 2:
        m = ALU[lo]
        isb = m in ('BTJO', 'BTJZ')
        if hi == 1: s, d, n = R(b1), 'A', 2
        elif hi == 2: s, d, n = '%%>%02X' % b1, 'A', 2
        elif hi == 3: s, d, n = R(b1), 'B', 2
        elif hi == 4: s, d, n = R(b1), R(b2), 3
        elif hi == 5: s, d, n = '%%>%02X' % b1, 'B', 2
        elif hi == 6: s, d, n = 'B', 'A', 1
        elif hi == 7: s, d, n = '%%>%02X' % b1, R(b2), 3
        if isb:
            off = rd(pc + n)
            t = rel(pc + n + 1, off)
            return '%s %s,%s,>%04X' % (m, s, d, t), n + 1, [t], True
        return '%s %s,%s' % (m, s, d), n, [], True
    if hi in (8, 9, 0xA) and lo in (0, 2, 3, 4, 5, 6, 7) or (hi == 9 and lo == 1):
        PM = {0: 'MOVP', 1: 'MOVP', 2: 'MOVP', 3: 'ANDP', 4: 'ORP', 5: 'XORP', 6: 'BTJOP', 7: 'BTJZP'}
        m = PM[lo]
        if hi == 8:
            if lo == 0: return 'MOVP P%d,A' % b1, 2, [], True
            s, n = 'A', 2; p = b1
        elif hi == 9:
            if lo == 1: return 'MOVP P%d,B' % b1, 2, [], True
            s, n = 'B', 2; p = b1
        else:
            s, n = '%%>%02X' % b1, 3; p = b2
        if m.startswith('BTJ'):
            t = rel(pc + n + 1, rd(pc + n))
            return '%s %s,P%d,>%04X' % (m, s, p, t), n + 1, [t], True
        return '%s %s,P%d' % (m, s, p), n, [], True
    if hi in (8, 9, 0xA) and lo >= 8:
        MM = {8: 'MOVD', 0xA: 'LDA', 0xB: 'STA', 0xC: 'BR', 0xD: 'CMPA', 0xE: 'CALL'}
        if lo not in MM: return 'DB >%02X' % op, 1, [], False
        m = MM[lo]
        if hi == 8:
            a = (b1 << 8) | b2
            if m == 'MOVD': return 'MOVD %%>%04X,R%d' % (a, b3), 4, [], True
            ops = '@>%04X' % a; n = 3
            tg = [a] if m in ('BR', 'CALL') else []
        elif hi == 9:
            if m == 'MOVD': return 'MOVD R%d,R%d' % (b1, b2), 3, [], True
            ops = '*R%d' % b1; n = 2; tg = []
        else:
            a = (b1 << 8) | b2
            if m == 'MOVD': return 'MOVD %%>%04X(B),R%d' % (a, b3), 4, [], True
            ops = '@>%04X(B)' % a; n = 3; tg = []
        return '%s %s' % (m, ops), n, tg, m != 'BR'
    if hi in (0xB, 0xC, 0xD):
        if hi == 0xB and lo == 0: return 'CLRC/TSTA', 1, [], True
        if hi == 0xC and lo == 0: return 'MOV A,B', 1, [], True
        if hi == 0xC and lo == 1: return 'TSTB', 1, [], True
        if hi == 0xD and lo == 0: return 'MOV A,%s' % R(b1), 2, [], True
        if hi == 0xD and lo == 1: return 'MOV B,%s' % R(b1), 2, [], True
        reg = {0xB: 'A', 0xC: 'B'}.get(hi) or R(b1)
        n = 1 if hi != 0xD else 2
        m = UN[lo]
        if m == 'DJNZ':
            t = rel(pc + n + 1, rd(pc + n))
            return 'DJNZ %s,>%04X' % (reg, t), n + 1, [t], True
        return '%s %s' % (m, reg), n, [], True
    if hi == 0xE and lo < 8:
        t = rel(pc + 2, b1)
        return '%s >%04X' % (JC[lo], t), 2, [t], lo != 0
    if op >= 0xE8:
        n = 255 - op
        v = w(0xFFFE - 2 * n)
        return 'TRAP %d  ; ->%04X' % (n, v), 1, [v], True
    return 'DB >%02X' % op, 1, [], False

code = {}
labels = set()
todo = ([w(0xFFFE), w(0xFFFC), w(0xFFF8)] if BASE == 0xF000 else []) + extra
labels.update(todo)
while todo:
    pc = todo.pop()
    while BASE <= pc <= END and pc not in code:
        t, n, tg, ft = dis(pc)
        code[pc] = (t, n)
        for x in tg:
            if BASE <= x <= END:
                labels.add(x); todo.append(x)
        if not ft: break
        pc += n

pc = BASE
while pc <= END:
    lab = ('L%04X:' % pc) if pc in labels else ''
    if pc in code:
        t, n = code[pc]
        hx = ' '.join('%02X' % rd(pc + i) for i in range(n))
        print('%04X  %-12s %-7s %s' % (pc, hx, lab, t))
        pc += n
    else:
        # data run
        start = pc
        while pc <= END and pc not in code and (pc == start or pc not in labels) and pc - start < 16:
            pc += 1
        print('%04X  %-12s %-7s DB %s' % (start, '', lab, ' '.join('%02X' % rd(a) for a in range(start, pc))))
