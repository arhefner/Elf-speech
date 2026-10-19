#!/usr/bin/env python3
"""
mkdict.py - build the pronunciation dictionary of tts for the SP0256-AL2

    tools/mkdict.py [-d folder] [-o file]       build the dictionary file
    tools/mkdict.py --show word ...             show how words are said
    tools/mkdict.py --compare                   score the rules

The words come from the files in the dictionary folder,
../SP0256-AL2/sp0256_dict unless -d gives another:

  A.txt to Z.txt     The big dictionary.  Its pronunciations are phonemes
                     written with the names of allophones, and are turned
                     into the allophones of the SP0256-AL2 by the rules
                     below.  The first pronunciation of a word is used.
  NUMBERS.txt        Words with their allophones as they are spoken, which
  sp0256_al2_dictionary.txt  replace those of the big dictionary.  The
  contractions.txt   files are read in this order, and a word in a later
  overrides.txt      file replaces the same word in an earlier one.
                     dictupdate.py adds words to overrides.txt.
  common.txt         The most common English words, the most common
                     first, which mkcommon.py makes.  The first 5000 of
                     them that are in the dictionary, or as many as
                     --common gives, are put in a tree of their own.

A line is a word, a colon and the allophones, each with a backslash:

    HELLO: \\HH1 \\EH \\LL \\AX \\OW

A word can be followed by a note in parentheses, "SUBJECT (NOUN)", which
is left out; the first line of a word in a file is used.

THE FILE

The dictionary is two B-trees of blocks of 512 bytes, which are sectors
on the disk, so that tts reads a block with one sector read.  The first
tree has the common words, and the second all the others.  Block 0 is
the header, with the top level of each tree:

    'S' 'P' 'D' '2'   the signature
    trees             the number of trees
    for each tree:
      levels          how many levels of index blocks are below the top
      offset          where its top level is in the header, two bytes,
                      high byte first
    the top level of each tree, entries ended by FF

An entry is a word, front coded, and its data:

    shared            how many letters it shares with the word before
    letters           the other letters, the last with bit 7 set
    data              bytes, the last with bit 7 set

The first word of a block shares nothing, and FF ends a block.  In the
data blocks, the bottom level, the data is the allophones of the word.
In the index blocks above them, and in the header, an entry is the first
word of a block of the level below, or as much of it as tells it from the
last word of the block before, and its data is the number of that block
in two bytes, the high seven bits and then the low seven bits with bit 7
set.  A word is looked for from the header down: at each level, the last
entry that is not after the word is taken.  It is looked for in the
common words first.

The words are sorted by their bytes, the ASCII of the letters A to Z, the
digits and the apostrophe, in upper case.

Elf/OS and ELF-DOS find a place in a file by following its chain of
clusters from the start, which takes longer the further into the file it
is.  So the index blocks of both trees are after the header, then the
words of the common tree, and the other words are last.  About nine words
of ten in English text are found within the first 100K of the file.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DICT_DIR = os.path.join(HERE, '..', '..', 'SP0256-AL2', 'sp0256_dict')
ALLOPHONES_H = os.path.join(HERE, '..', '..', 'SP0256-AL2', 'include',
                            'allophones.h')

BLOCK = 512

# the longest word and the most allophones of a word, DICT_WORD_MAX and
# DICT_MAX of dict.h
MAX_WORD = 48
MAX_ALLOPHONES = 64
SIGNATURE = b'SPD2'

# how many common words have a tree of their own
COMMON = 5000

# the room for the top level of a tree in the header, but the last one
TOP_ROOM = 128

# The override files, read in this order after the big dictionary
OVERRIDES = ['NUMBERS.txt', 'sp0256_al2_dictionary.txt', 'contractions.txt',
             'overrides.txt']


def allophone_numbers():
    """The numbers of the allophones by name, from allophones.h."""
    numbers = {}
    with open(ALLOPHONES_H) as f:
        for line in f:
            m = re.match(r'#define\s+([A-Z]+[0-9]?)\s+0x([0-9a-f]+)', line)
            if m and m.group(1) != 'EOP':
                numbers[m.group(1)] = int(m.group(2), 16)
    assert len(numbers) == 64
    return numbers


ALLOPHONES = allophone_numbers()
NAMES = {n: a for a, n in ALLOPHONES.items()}

WORD_CHARS = re.compile(r"^[A-Z0-9']+$")


def read_words(path, first=True):
    """
    The words of a file, as a dict of word -> list of names, in the order
    of the file.  A word that is there more than once has the first of its
    pronunciations.  Raises ValueError for a bad line.
    """
    words = {}
    with open(path, encoding='latin-1') as f:
        for number, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if ':' not in line:
                raise ValueError(f'{path}:{number}: no colon: {line}')
            word, allo = line.split(':', 1)
            word = re.sub(r'\s*\(.*\)\s*$', '', word).strip().upper()
            if not WORD_CHARS.match(word):
                raise ValueError(f'{path}:{number}: bad word: {word}')
            names = [a.lstrip('\\').upper() for a in allo.split()]
            if not names:
                raise ValueError(f'{path}:{number}: no allophones: {word}')
            if first and word in words:
                continue
            words[word] = names
    return words


def check_names(path, words):
    """Raise ValueError if a word has a name that is not an allophone."""
    for word, names in words.items():
        for n in names:
            if n not in ALLOPHONES:
                raise ValueError(f'{path}: {word}: not an allophone: {n}')


# THE RULES
#
# The big dictionary has the phonemes of English, one allophone for each,
# without stress: AX for the vowel of "but" and "about", TT1, KK1, DD1 and
# the others for every T, K and D, ER and YY, which are not allophones, for
# the R vowel and the Y.  The SP0256-AL2 has several allophones for many of
# them, which sound right in different places in a word, and needs pauses
# before the stops.  The rules choose them as the words of
# sp0256_al2_dictionary.txt do, which were tuned by ear.

VOWELS = {'AA', 'AE', 'AX', 'AO', 'AW', 'AY', 'EH', 'ER', 'EY', 'IH', 'IY',
          'OW', 'OY', 'UH', 'UW', 'OR', 'AR', 'YR', 'XR'}
FRONT = {'IY', 'IH', 'EY', 'EH', 'AE', 'ER', 'YY'}
SHORT = {'IH', 'EH', 'AE', 'AA', 'AX', 'UH', 'AO'}
VOICELESS_STOPS = {'PP', 'TT', 'KK', 'CH'}
VOICED_STOPS = {'BB', 'DD', 'GG', 'JH'}
STOPS = VOICELESS_STOPS | VOICED_STOPS
VOICELESS = {'PP', 'TT', 'KK', 'CH', 'FF', 'TH', 'SS', 'SH'}

# Words of one syllable that are not stressed in a sentence, whose vowel
# is not doubled
FUNCTION_WORDS = {'A', 'AN', 'AND', 'AS', 'AT', 'BUT', 'CAN', 'FOR', 'FROM',
                  'HAD', 'HAS', 'HAVE', 'HE', 'HER', 'HIM', 'HIS', 'IN',
                  'IT', 'ITS', 'OF', 'ON', 'OR', 'SHE', 'THAT', 'THE',
                  'THEM', 'THEN', 'TO', 'US', 'WAS', 'WE', 'WERE', 'WHAT',
                  'WITH', 'YOU'}

# The phonemes of the big dictionary, from its allophones
PHONEME = {'TT1': 'TT', 'KK1': 'KK', 'DD1': 'DD', 'BB1': 'BB', 'GG1': 'GG',
           'NN1': 'NN', 'RR1': 'RR', 'HH1': 'HH', 'UW1': 'UW', 'DH1': 'DH'}

# A vowel before an R that does not come before a vowel
R_VOWEL = {'AO': ['OR'], 'OW': ['OR'], 'AA': ['AR'], 'IH': ['YR'],
           'IY': ['YR'], 'EH': ['XR'], 'EY': ['XR'], 'AE': ['XR'],
           'AY': ['AY', 'ER'], 'AW': ['AW', 'ER']}


def to_phonemes(names):
    """The allophones of the big dictionary as phonemes."""
    return [PHONEME.get(n, n) for n in names]


def is_vowel(p):
    return p in VOWELS


def convert(names, word=None):
    """The allophones of a word of the big dictionary."""
    ph = to_phonemes(names)
    stressed = word not in FUNCTION_WORDS

    # A vowel and an R that is not before a vowel are an R vowel: OR, AR,
    # YR, XR.  ER after a vowel is ER.
    out = []
    i = 0
    while i < len(ph):
        p = ph[i]
        if (p in R_VOWEL and i + 1 < len(ph) and ph[i + 1] == 'RR'
                and (i + 2 == len(ph) or not is_vowel(ph[i + 2]))):
            out.extend(R_VOWEL[p])
            i += 2
            continue
        out.append(p)
        i += 1
    ph = out

    nvowels = sum(1 for p in ph if is_vowel(p))
    first_vowel = next((i for i, p in enumerate(ph) if is_vowel(p)), -1)

    def vowel_after(i):
        return any(is_vowel(p) for p in ph[i + 1:])

    def nxt(i):
        return ph[i + 1] if i + 1 < len(ph) else None

    def prv(i):
        return ph[i - 1] if i > 0 else None

    result = []
    for i, p in enumerate(ph):
        n = nxt(i)
        b = prv(i)
        last = i == len(ph) - 1

        # a pause before a stop, but not at the start of the word
        if p in STOPS and i > 0:
            if p in VOICELESS_STOPS:
                result.append('PA3')
            else:
                result.append('PA1' if is_vowel(b) else 'PA2')

        if p == 'TT':
            a = ['TT1'] if n == 'SS' else ['TT2']
        elif p == 'DD':
            a = ['DD1'] if last else ['DD2']
        elif p == 'BB':
            a = ['BB2'] if is_vowel(n) or n == 'YY' else ['BB1']
        elif p == 'KK':
            if n in FRONT or n in ('LL', 'RR', 'YY'):
                a = ['KK1']
            elif is_vowel(n) or n in ('WW', 'EL'):
                a = ['KK3']
            else:
                a = ['KK2']
        elif p == 'GG':
            if n in FRONT or n in ('LL', 'RR', 'YY'):
                a = ['GG1']
            elif is_vowel(n) or n == 'WW':
                a = ['GG2']
            else:
                a = ['GG3']
        elif p == 'HH':
            a = ['HH1'] if n in FRONT else ['HH2']
        elif p == 'NN':
            a = ['NN2'] if i == 0 and n in ('OW', 'AO', 'AA', 'UW', 'UH',
                                            'AX', 'OY', 'AW', 'OR', 'AR') \
                else ['NN1']
        elif p == 'RR':
            a = ['RR2'] if b in STOPS else ['RR1']
        elif p == 'DH':
            a = ['DH1'] if i == 0 else ['DH2']
        elif p == 'YY':
            a = ['YY2'] if i == 0 else ['YY1']
        elif p == 'UW':
            a = ['UW1'] if vowel_after(i) else ['UW2']
        elif p == 'ER':
            # the ER of an ending is ER1, a stressed one ER2
            a = ['ER1'] if not vowel_after(i) and i > first_vowel \
                else ['ER2']
        elif p == 'WW':
            a = ['WH'] if b in ('TT', 'KK') else ['WW']
        elif p == 'LL':
            # an L that ends a syllable
            if is_vowel(b) and b != 'ER' and not is_vowel(n):
                a = ['EL']
                # the AX of "little" and "uncle" is in the EL
                if b == 'AX' and prv(i - 1) in STOPS and len(result) > 0:
                    result.pop()
            else:
                a = ['LL']
        elif p == 'SS':
            # SS is short, so it is doubled at the start, before a vowel
            # and at the end after a vowel
            if i == 0 or is_vowel(n) or (last and is_vowel(b)):
                a = ['SS', 'SS']
            else:
                a = ['SS']
        elif p in SHORT and stressed and \
                ((nvowels == 1 and not (p == 'AX' and last))
                 or (i == first_vowel and p != 'AX')):
            # the short vowel of a word of one syllable is doubled, but not
            # the AX of "the", and the first one of a longer word, which is
            # most often stressed
            a = [p, p]
        elif p == 'AY' and nvowels == 1 and stressed:
            a = ['AA', 'AY']
        else:
            a = [p]
        result.extend(a)

    for a in result:
        assert a in ALLOPHONES, (names, result)
    return result


def load(folder, verbose=False):
    """
    All the words, as a dict of word -> list of allophone names, and the
    words of the override files.
    """
    words = {}
    for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
        path = os.path.join(folder, letter + '.txt')
        for word, names in read_words(path).items():
            words[word] = convert(names, word)
    big = len(words)

    overridden = {}
    for name in OVERRIDES:
        path = os.path.join(folder, name)
        if not os.path.exists(path):
            continue
        more = read_words(path)
        check_names(path, more)
        overridden.update(more)
        if verbose:
            new = sum(1 for w in more if w not in words)
            print(f'{name}: {len(more)} words, {new} new')
    words.update(overridden)
    if verbose:
        print(f'{big} words in the big dictionary, {len(words)} in all')
    return words, overridden


# THE B-TREE

def encode_number(n):
    assert n < 1 << 14
    return bytes([n >> 7, (n & 0x7f) | 0x80])


def encode_entry(word, prev, data):
    shared = 0
    if prev is not None:
        while (shared < len(word) and shared < len(prev)
               and word[shared] == prev[shared]):
            shared += 1
    rest = word[shared:].encode('ascii')
    assert len(rest) > 0, (word, prev)
    rest = rest[:-1] + bytes([rest[-1] | 0x80])
    data = data[:-1] + bytes([data[-1] | 0x80])
    return bytes([shared]) + rest + data


def pack(entries, room):
    """
    Pack (word, data) entries into blocks of room bytes, each ended by FF.
    Returns a list of (first word, last word, bytes of the block).
    """
    blocks = []
    block = bytearray()
    first = prev = None
    for word, data in entries:
        e = encode_entry(word, prev, data)
        if len(block) + len(e) + 1 > room:
            blocks.append((first, prev, bytes(block) + b'\xff'))
            block = bytearray()
            e = encode_entry(word, None, data)
        if not block:
            first = word
        block += e
        prev = word
    blocks.append((first, prev, bytes(block) + b'\xff'))
    return blocks


def separator(prev_last, first):
    """
    The shortest start of first that comes after prev_last, so that the
    index takes less room.  A word from prev_last up to it is looked for
    in the block before, and is not there either way.
    """
    if prev_last is None:
        return first[:1]
    for n in range(1, len(first) + 1):
        if first[:n] > prev_last:
            return first[:n]
    return first


def index(blocks, start, room):
    """
    Pack the entries of the level above blocks, which are numbered from
    start: the separator of each block and its number.
    """
    seps = []
    for k, (first, last, _) in enumerate(blocks):
        prev_last = blocks[k - 1][1] if k > 0 else None
        seps.append((separator(prev_last, first), encode_number(start + k)))
    upper = pack(seps, room)
    # a block of the index covers the words of the blocks below it
    out = []
    k = 0
    for first, last, block in upper:
        n = count_entries(block)
        out.append((blocks[k][0], blocks[k + n - 1][1], block))
        k += n
    return out


def build_tree(words, names, top_room):
    """
    The levels of a B-tree of words, the blocks of words first, as lists of
    (first word, last word, bytes) with no block numbers yet.  Levels of
    index are added until the top one fits in top_room.  A block number is
    always two bytes, so the blocks can be packed before they are
    numbered.
    """
    entries = [(w, bytes(ALLOPHONES[a] for a in names[w])) for w in words]
    levels = [pack(entries, BLOCK)]
    while len(index(levels[-1], 0, top_room)) > 1:
        levels.append(index(levels[-1], 0, BLOCK))
    return levels


def build(words, common=()):
    """
    The bytes of the dictionary file, and the number of words, of blocks of
    words and of index blocks of each tree.
    """
    for w, names in words.items():
        if len(w) > MAX_WORD or len(names) > MAX_ALLOPHONES:
            raise ValueError(f'{w}: longer than tts takes, {MAX_WORD} '
                             f'letters and {MAX_ALLOPHONES} allophones')
    first = set(w for w in common if w in words)
    sets = [sorted(first), sorted(w for w in words if w not in first)]
    sets = [ws for ws in sets if ws]

    room = BLOCK - len(SIGNATURE) - 1 - 3 * len(sets)
    rooms = []
    trees = []
    for k, ws in enumerate(sets):
        r = min(TOP_ROOM, room // 2) if k < len(sets) - 1 else room
        levels = build_tree(ws, words, r)
        top = index(levels[-1], 0, r)
        room -= len(top[0][2])
        rooms.append(r)
        trees.append(levels)

    # Lay the blocks out: the index blocks of each tree, the top level
    # first, then the words of each tree.
    starts = []
    n = 1
    for levels in trees:
        st = [0] * len(levels)
        for k in reversed(range(1, len(levels))):
            st[k] = n
            n += len(levels[k])
        starts.append(st)
    for t, levels in enumerate(trees):
        starts[t][0] = n
        n += len(levels[0])

    # pack the index again, with the numbers of the blocks
    tops = []
    for t, levels in enumerate(trees):
        for k in range(1, len(levels)):
            levels[k] = index(levels[k - 1], starts[t][k - 1], BLOCK)
        top = index(levels[-1], starts[t][-1], rooms[t])
        assert len(top) == 1
        tops.append(top[0][2])

    head = bytearray(SIGNATURE + bytes([len(trees)]))
    offset = len(head) + 3 * len(trees)
    for levels, top in zip(trees, tops):
        head += bytes([len(levels) - 1, offset >> 8, offset & 0xff])
        offset += len(top)
    for top in tops:
        head += top
    assert len(head) <= BLOCK

    blocks = [bytes(head)]
    for levels in trees:
        for k in reversed(range(1, len(levels))):
            blocks += [b for _, _, b in levels[k]]
    for levels in trees:
        blocks += [b for _, _, b in levels[0]]
    out = b''.join(b.ljust(BLOCK, b'\0') for b in blocks)
    sizes = [(len(ws), len(levels[0]), sum(len(lv) for lv in levels[1:]))
             for ws, levels in zip(sets, trees)]
    return out, sizes


def count_entries(block):
    n = 0
    i = 0
    while block[i] != 0xff:
        i += 1
        while not block[i] & 0x80:
            i += 1
        i += 1
        while not block[i] & 0x80:
            i += 1
        i += 1
        n += 1
    return n


# LOOKING UP, as tts does

def scan(buf, pos, word):
    """
    Look through the entries of a block from pos, as dict_scan of tts
    does.  Returns (found, where): whether the word is there, and the
    position of the data of the last entry that is not after the word, or
    None if there is none.

    m is how many letters the word has in common with the entry before,
    which is before the word.  An entry that shares more letters with the
    entry before is before the word too, and one that shares fewer is
    after it.  Only an entry that shares m letters is compared with the
    word, from letter m.
    """
    target = word.encode('ascii')
    m = 0
    where = None
    while buf[pos] != 0xff:
        shared = buf[pos]
        pos += 1
        if shared < m:
            return False, where
        if shared > m:
            while not buf[pos] & 0x80:
                pos += 1
            pos += 1
        else:
            while True:
                c = buf[pos]
                pos += 1
                if m == len(target) or (c & 0x7f) > target[m]:
                    return False, where
                if (c & 0x7f) < target[m]:
                    while not c & 0x80:
                        c = buf[pos]
                        pos += 1
                    break
                m += 1
                if c & 0x80:
                    if m == len(target):
                        return True, pos
                    break
        where = pos
        while not buf[pos] & 0x80:
            pos += 1
        pos += 1
    return False, where


def lookup(dic, word):
    """The allophone numbers of a word in the file, or None."""
    assert dic[:4] == SIGNATURE
    for t in range(dic[4]):
        levels = dic[5 + 3 * t]
        buf = dic[:BLOCK]
        pos = (dic[6 + 3 * t] << 8) | dic[7 + 3 * t]
        for _ in range(levels + 1):
            found, where = scan(buf, pos, word)
            if where is None:
                break
            n = (buf[where] << 7) | (buf[where + 1] & 0x7f)
            buf = dic[n * BLOCK:(n + 1) * BLOCK]
            pos = 0
        else:
            found, where = scan(buf, pos, word)
            if found:
                out = []
                while True:
                    out.append(buf[where] & 0x3f)
                    if buf[where] & 0x80:
                        return out
                    where += 1
    return None


def read_common(folder):
    """The words of common.txt, the most common first, or none."""
    path = os.path.join(folder, 'common.txt')
    if not os.path.exists(path):
        return []
    words = []
    with open(path, encoding='latin-1') as f:
        for line in f:
            w = line.strip().upper()
            if w and not w.startswith('#'):
                words.append(w)
    return words


def main():
    ap = argparse.ArgumentParser(
        description='Build the pronunciation dictionary of tts.')
    ap.add_argument('-d', '--dict', default=DICT_DIR,
                    help='the folder of the word files')
    ap.add_argument('-o', '--output', default='sp0256.dic',
                    help='the dictionary file to write')
    ap.add_argument('--show', nargs='+', metavar='WORD',
                    help='show how the words are said, and do not build')
    ap.add_argument('--compare', action='store_true',
                    help='compare the rules with the words that are tuned '
                         'by ear, and do not build')
    ap.add_argument('--common', type=int, default=COMMON,
                    help=f'how many words of common.txt to put in the '
                         f'tree of common words ({COMMON}), 0 for none')
    ap.add_argument('--no-check', action='store_true',
                    help='do not look every word up in the file built')
    args = ap.parse_args()

    try:
        if args.compare:
            return compare(args.dict)
        words, _ = load(args.dict, verbose=not args.show)
    except (OSError, ValueError) as e:
        sys.exit(f'mkdict: {e}')

    if args.show:
        for w in args.show:
            w = w.upper()
            names = words.get(w)
            print(f'{w}: ' + (' '.join('\\' + n for n in names)
                              if names else '(not in the dictionary)'))
        return

    try:
        common = [w for w in read_common(args.dict) if w in words]
        dic, sizes = build(words, common[:args.common])
    except (OSError, ValueError) as e:
        sys.exit(f'mkdict: {e}')
    if not args.no_check:
        for w, names in words.items():
            got = lookup(dic, w)
            want = [ALLOPHONES[a] for a in names]
            if got != want:
                sys.exit(f'mkdict: {w} is looked up as {got}, not {want}')
        # words that are not there, before, between and after them
        for w in ['', "'", 'A' * 40, 'ZZZZZZZZZZZZZZZZZZ', 'QQQQ', 'HELLOX',
                  'HELL0', 'ABCDEFG']:
            if w and w not in words and lookup(dic, w) is not None:
                sys.exit(f'mkdict: {w} is found, but it is not there')
    with open(args.output, 'wb') as f:
        f.write(dic)
    print(f'{args.output}: {len(dic)} bytes, {len(dic) // BLOCK} blocks')
    for name, (n, data, idx) in zip(['common words', 'other words']
                                    if len(sizes) == 2 else ['words'],
                                    sizes):
        print(f'  {name}: {n}, in {data} blocks of words and {idx} of '
              f'index')


def compare(folder):
    """Score the rules against the words that are tuned by ear."""
    big = {}
    for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
        big.update(read_words(os.path.join(folder, letter + '.txt')))
    same = 0
    total = 0
    dist = 0
    for name in OVERRIDES[:2]:
        tuned = read_words(os.path.join(folder, name))
        for w, names in tuned.items():
            if w not in big:
                continue
            got = convert(big[w], w)
            total += 1
            d = distance(got, names)
            dist += d
            if d == 0:
                same += 1
            else:
                print(f'{w:14} {" ".join(got):45} tuned: {" ".join(names)}')
    print(f'{same} of {total} the same, {dist / total:.2f} allophones '
          f'different on average')


def distance(a, b):
    """The edit distance between two lists."""
    d = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        prev, d[0] = d[0], i
        for j, y in enumerate(b, 1):
            prev, d[j] = d[j], min(d[j] + 1, d[j - 1] + 1, prev + (x != y))
    return d[len(b)]


if __name__ == '__main__':
    main()
