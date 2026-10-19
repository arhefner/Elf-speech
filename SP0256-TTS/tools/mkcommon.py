#!/usr/bin/env python3
"""
mkcommon.py - make the list of common words of the dictionary of tts

    tools/mkcommon.py [-n count] [-o file] text ...

Counts the words of English text files, and writes the most common ones,
the most common first, one to a line, to common.txt of the dictionary
folder unless -o gives another file.  Each file counts the same, however
long it is.  A Project Gutenberg text is counted from its START line to
its END line, without the licence around it.

mkdict.py puts the common words that are in the dictionary in a part of
the dictionary file of their own, at the start of the file, which tts
looks in first.  Elf/OS and ELF-DOS find a place in a file by following
its chain of clusters from the start, so a word near the start of the
file is found faster.  The common.txt that comes with tts was made from
29 books of Project Gutenberg, where the 5000 most common words of the
dictionary are 90% of the words.
"""

import argparse
import collections
import os
import re

import mkdict


def main():
    ap = argparse.ArgumentParser(
        description='Make the list of common words of the dictionary.')
    ap.add_argument('texts', nargs='+', help='English text files')
    ap.add_argument('-n', '--count', type=int, default=10000,
                    help='how many words to list (10000)')
    ap.add_argument('-o', '--output',
                    default=os.path.join(mkdict.DICT_DIR, 'common.txt'),
                    help='the file to write')
    args = ap.parse_args()

    freq = collections.Counter()
    for path in args.texts:
        with open(path, encoding='utf-8', errors='replace') as f:
            text = f.read()
        start = re.search(r'\*\*\* ?START OF.*?\*\*\*', text)
        end = re.search(r'\*\*\* ?END OF', text)
        if start and end:
            text = text[start.end():end.start()]
        text = text.replace('’', "'").replace('‘', "'")
        words = [w.strip("'").upper()
                 for w in re.findall(r"[A-Za-z']+", text)]
        counts = collections.Counter(w for w in words if w)
        total = sum(counts.values())
        for w, n in counts.items():
            freq[w] += n / total

    with open(args.output, 'w') as f:
        f.write(f'# The most common words of {len(args.texts)} texts, the most '
                f'common first, made by\n# mkcommon.py.  mkdict.py puts the '
                f'first ones in the part of the\n# dictionary file that tts '
                f'looks in first.\n')
        for w, _ in freq.most_common(args.count):
            f.write(w + '\n')
    print(f'{args.output}: {min(args.count, len(freq))} words')


if __name__ == '__main__':
    main()
