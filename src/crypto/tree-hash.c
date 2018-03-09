// Copyright (c) 2017-2018 The TycheCash developers  ; Originally forked from Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "hash-ops.h"

size_t tree_hash_cnt(size_t count) {
  assert(count >= 3); // cases for 0,1,2 are handled elsewhere
  assert(count <= 0x10000000); // sanity limit to 2^28, MSB=1 will cause an inf loop

  size_t pow = 2;
  while(pow < count) {
    pow <<= 1;
  }
  return pow >> 1;
}

void tree_hash(const char (*hashes)[HASH_SIZE], size_t count, char *root_hash) {
  assert(count > 0);
  if (count == 1) {
    memcpy(root_hash, hashes, HASH_SIZE);
  } else if (count == 2) {
    cn_fast_hash(hashes, 2 * HASH_SIZE, root_hash);
  } else {
    size_t i, j;
    size_t cnt = tree_hash_cnt(count);

    char (*ints)[HASH_SIZE];
    size_t ints_size = cnt * HASH_SIZE;
    ints = alloca(ints_size);

    memset(ints, 0, ints_size); // allocate, and zero out as extra protection for using uninitialized me
    memcpy(ints, hashes, (2 * cnt - count) * HASH_SIZE);

    for (i = 2 * cnt - count, j = 2 * cnt - count; j < cnt; i += 2, ++j) {
      cn_fast_hash(hashes[i], 64, ints[j]);
    }
    assert(i == count);

    while (cnt > 2) {
      cnt >>= 1;
      for (i = 0, j = 0; j < cnt; i += 2, ++j) {
        cn_fast_hash(ints[i], 64, ints[j]);
      }
    }

    cn_fast_hash(ints[0], 64, root_hash);
  }
}

void tree_branch(const char (*hashes)[HASH_SIZE], size_t count, char (*branch)[HASH_SIZE]) {
  size_t i, j;
  size_t cnt = 1;
  size_t depth = 0;
  char (*ints)[HASH_SIZE];
  assert(count > 0);
  for (i = sizeof(size_t) << 2; i > 0; i >>= 1) {
    if (cnt << i <= count) {
      cnt <<= i;
      depth += i;
    }
  }
  assert(cnt == 1ULL << depth);
  assert(depth == tree_depth(count));
  ints = alloca((cnt - 1) * HASH_SIZE);
  memcpy(ints, hashes + 1, (2 * cnt - count - 1) * HASH_SIZE);
  for (i = 2 * cnt - count, j = 2 * cnt - count - 1; j < cnt - 1; i += 2, ++j) {
    cn_fast_hash(hashes[i], 2 * HASH_SIZE, ints[j]);
  }
  assert(i == count);
  while (depth > 0) {
    assert(cnt == 1ULL << depth);
    cnt >>= 1;
    --depth;
    memcpy(branch[depth], ints[0], HASH_SIZE);
    for (i = 1, j = 0; j < cnt - 1; i += 2, ++j) {
      cn_fast_hash(ints[i], 2 * HASH_SIZE, ints[j]);
    }
  }
}

void tree_hash_from_branch(const char (*branch)[HASH_SIZE], size_t depth, const char *leaf, const void *path, char *root_hash) {
  if (depth == 0) {
    memcpy(root_hash, leaf, HASH_SIZE);
  } else {
    char buffer[2][HASH_SIZE];
    int from_leaf = 1;
    char *leaf_path, *branch_path;
    while (depth > 0) {
      --depth;
      if (path && (((const char *) path)[depth >> 3] & (1 << (depth & 7))) != 0) {
        leaf_path = buffer[1];
        branch_path = buffer[0];
      } else {
        leaf_path = buffer[0];
        branch_path = buffer[1];
      }
      if (from_leaf) {
        memcpy(leaf_path, leaf, HASH_SIZE);
        from_leaf = 0;
      } else {
        cn_fast_hash(buffer, 2 * HASH_SIZE, leaf_path);
      }
      memcpy(branch_path, branch[depth], HASH_SIZE);
    }
    cn_fast_hash(buffer, 2 * HASH_SIZE, root_hash);
  }
}