#!/usr/bin/env python3
"""
dictupdate.py - change how tts says words

    tools/dictupdate.py file ...             replace the words of the files
    tools/dictupdate.py -r word ...          say the words as before again
    tools/dictupdate.py -n file ...          only show what would change

The files are words with their allophones, as sp0256_al2_dictionary.txt
is, one to a line:

    HELLO: \\HH1 \\EH \\LL \\AX \\OW
    TOMATO: \\TT2 \\AX \\MM \\EY \\PA3 \\TT2 \\OW

A word that is in the dictionary is said with the new allophones, and a
word that is not is added.  The words are kept in overrides.txt of the
dictionary folder, which is read after the other word files, so the word
files themselves are not changed.  -r takes words out of overrides.txt,
so that they are said as the other files say them.

Then the dictionary file is built again, sp0256.dic unless -o gives
another, as mkdict.py builds it.  Copy it to the disk of the Elf.
"""

import argparse
import os
import sys

import mkdict

HEADER = """\
# Words that are said as they are here, and not as the other word files
# say them.  dictupdate.py adds words to this file and takes them out, and
# keeps it sorted.  It is read last by mkdict.py.
"""


def format_line(word, names):
    return word + ': ' + ' '.join('\\' + n for n in names)


def main():
    ap = argparse.ArgumentParser(
        description='Change how tts says words, and build the dictionary '
                    'again.')
    ap.add_argument('files', nargs='*',
                    help='files of words with their allophones')
    ap.add_argument('-r', '--remove', nargs='+', metavar='WORD', default=[],
                    help='take the words out of overrides.txt')
    ap.add_argument('-d', '--dict', default=mkdict.DICT_DIR,
                    help='the folder of the word files')
    ap.add_argument('-o', '--output', default='sp0256.dic',
                    help='the dictionary file to build')
    ap.add_argument('-n', '--dry-run', action='store_true',
                    help='show what would change, and change nothing')
    args = ap.parse_args()
    if not args.files and not args.remove:
        ap.error('give files of words, or -r and words')

    path = os.path.join(args.dict, 'overrides.txt')
    try:
        before, _ = mkdict.load(args.dict)
        overrides = mkdict.read_words(path) if os.path.exists(path) else {}
        new = {}
        for f in args.files:
            words = mkdict.read_words(f)
            mkdict.check_names(f, words)
            new.update(words)
    except (OSError, ValueError) as e:
        sys.exit(f'dictupdate: {e}')

    changed = 0
    for word, names in sorted(new.items()):
        old = before.get(word)
        if old == names:
            print(f'{word}: the same')
            continue
        changed += 1
        if old is None:
            print(f'{word}: added: {" ".join(names)}')
        else:
            print(f'{word}: {" ".join(old)}  ->  {" ".join(names)}')
        overrides[word] = names

    for word in args.remove:
        word = word.upper()
        if word not in overrides:
            print(f'{word}: not in overrides.txt')
            continue
        changed += 1
        del overrides[word]
        print(f'{word}: taken out of overrides.txt')

    if args.dry_run or changed == 0:
        print('Nothing was changed.')
        return

    with open(path, 'w') as f:
        f.write(HEADER)
        for word in sorted(overrides):
            f.write(format_line(word, overrides[word]) + '\n')
    print(f'{path}: {len(overrides)} words')

    words, _ = mkdict.load(args.dict)
    common = [w for w in mkdict.read_common(args.dict) if w in words]
    dic, sizes = mkdict.build(words, common[:mkdict.COMMON])
    for word in list(new) + [w.upper() for w in args.remove]:
        if word in words and mkdict.lookup(dic, word) != \
                [mkdict.ALLOPHONES[a] for a in words[word]]:
            sys.exit(f'dictupdate: {word} is wrong in the file built')
    with open(args.output, 'wb') as f:
        f.write(dic)
    print(f'{args.output}: {len(dic)} bytes, {len(words)} words')


if __name__ == '__main__':
    main()
