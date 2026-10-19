/*
 * dict_lookup.c - look words up in the pronunciation dictionary of tts
 *
 * The file is two B-trees of 512 byte blocks, see tools/mkdict.py: the
 * common words, and the others.  Block 0 is the header, with the top
 * level of each tree, which is kept in memory.  At each level, dict_scan
 * finds the last entry that is not after the word, which is the number of
 * the block of the next level, and at the bottom, the block of words, the
 * word with its allophones.
 */
#define _ELFCLIB_
#include <stdlib.h>
#include "../include/dict.h"

#pragma extrn Cdict_scan
#pragma extrn Cdict_hook
#pragma extrn Copen
#pragma extrn Cclose
#pragma extrn Cread
#pragma extrn Clseek32

#define BLOCK_SIZE  512

/*
 * The header: the signature, the number of trees, and for each tree the
 * number of its levels of index blocks and where its top level is.
 */
#define HEAD_TREES  4
#define HEAD_TREE   5

int dict_scan(unsigned char *block, char *word, unsigned char **where);
void dict_hook(int (*poll)());

static int _dict_fd = -1;
static unsigned char _dict_head[BLOCK_SIZE];
static unsigned char _dict_block[BLOCK_SIZE];
static int _dict_cached = -1;       /* the number of the block read */
static int (*_dict_poll)();

void dict_poll(int (*poll)()) {
  _dict_poll = poll;
  dict_hook(poll);
}

/* read a block of the file into buf, false if it can not be */
static int _dict_read(unsigned int n, unsigned char *buf) {
  off_t pos;

  if (_dict_poll != NULL)
    _dict_poll();
  pos.low = n << 9;
  pos.high = n >> 7;
  lseek32(_dict_fd, pos, SEEK_SET);
  return read(_dict_fd, buf, BLOCK_SIZE) == BLOCK_SIZE;
}

int dict_open(char *name) {
  dict_close();
  _dict_fd = open(name, O_RDONLY);
  if (_dict_fd < 0)
    return DICT_NO_FILE;
  if (!_dict_read(0, _dict_head) || _dict_head[0] != 'S' ||
      _dict_head[1] != 'P' || _dict_head[2] != 'D' || _dict_head[3] != '2') {
    dict_close();
    return DICT_BAD_FILE;
  }
  return DICT_OK;
}

void dict_close(void) {
  if (_dict_fd >= 0)
    close(_dict_fd);
  _dict_fd = -1;
  _dict_cached = -1;
}

/* look a word up in a tree, as dict_lookup */
static int _dict_tree(unsigned char *tree, char *word,
                      unsigned char *allophones) {
  unsigned char *block, *where;
  unsigned int n;
  int levels, count;
  unsigned char a;

  /* down the index to the block of words */
  block = _dict_head + ((tree[1] << 8) | tree[2]);
  for (levels = tree[0]; levels >= 0; levels--) {
    dict_scan(block, word, &where);
    if (where == NULL)
      return 0;
    n = (where[0] << 7) | (where[1] & 0x7f);
    if (n != _dict_cached) {
      _dict_cached = -1;
      if (!_dict_read(n, _dict_block))
        return 0;
      _dict_cached = n;
    }
    block = _dict_block;
  }

  /* the word, and its allophones */
  if (!dict_scan(block, word, &where))
    return 0;
  count = 0;
  do {
    a = *where++;
    if (count < DICT_MAX)
      allophones[count++] = a & 0x3f;
  } while ((a & 0x80) == 0);
  return count;
}

int dict_lookup(char *word, unsigned char *allophones) {
  int t, n;

  if (_dict_fd < 0)
    return 0;
  for (t = 0; t < _dict_head[HEAD_TREES]; t++) {
    n = _dict_tree(_dict_head + HEAD_TREE + 3 * t, word, allophones);
    if (n > 0)
      return n;
  }
  return 0;
}
