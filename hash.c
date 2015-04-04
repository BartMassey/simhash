/*
 * Copyright © 2005-2009 Bart Massey
 * ALL RIGHTS RESERVED
 * [This program is licensed under the "3-clause ('new') BSD License"]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 */

/*
 * Simple hash table stop list ala corman-leiserson-rivest.
 * Bart Massey 2005/03
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "hash.h"

/* occupancy is out-of-band.  sigh */
#define EMPTY 0
#define FULL 1
#define DELETED 2
static char *occ;

static int *hash;
static int nhash;

/* for n > 0 */
static int next_pow2(int n) {
    int m = 1;
    while (n > 0) {
	n >>= 1;
	m <<= 1;
    }
    return m;
}

static void hash_alloc(void) {
    int i;
    hash = malloc(nhash * sizeof(int));
    assert(hash);
    occ = malloc(nhash);
    assert(occ);
    for (i = 0; i < nhash; i++)
	occ[i] = EMPTY;
}

/* The occupancy shouldn't be bad, since we only keep small crcs in
   the stop list */

void hash_reset(int size) {
    nhash = 7 * size;
    nhash = next_pow2(nhash);
    hash_alloc();
}

/* Since the input values are crc's, we don't
   try to hash them at all!  they're plenty random
   coming in, in principle. */

static int do_hash_insert(uint32_t crc) {
    int count;
    uint32_t h = crc;
    for (count = 0; count < nhash; count++) {
	int i = h & (nhash - 1);
	if (occ[i] != FULL) {
	    occ[i] = FULL;
	    hash[i] = crc;
	    return 1;
	}
	if (hash[i] == crc)
	    return 1;
	h += 2 * (nhash / 4) + 1;
    }
    return 0;
}

/* idiot stop-and-copy for deleted references */
static void gc(void) {
    int i;
    int *oldhash = hash;
    char *oldocc = occ;
    hash_alloc();
    for (i = 0; i < nhash; i++) {
	if (oldocc[i] == FULL) {
	    if(!do_hash_insert(oldhash[i])) {
		fprintf(stderr, "internal error: gc failed, table full\n");
		exit(1);
	    }
	}
    }
    free(oldhash);
    free(oldocc);
}

void hash_insert(uint32_t crc) {
    if (do_hash_insert(crc))
	return;
    gc();
    if (do_hash_insert(crc))
	return;
    fprintf(stderr, "internal error: insert failed, table full\n");
    abort();
    /*NOTREACHED*/
}

static int do_hash_contains(uint32_t crc) {
    int count;
    uint32_t h = crc;
    for (count = 0; count < nhash; count++) {
	int i = h & (nhash - 1);
	if (occ[i] == EMPTY)
	    return 0;
	if (occ[i] == FULL && hash[i] == crc)
	    return 1;
	h += 2 * (nhash / 4) + 1;
    }
    return -1;
}

int hash_contains(uint32_t crc) {
    int result = do_hash_contains(crc);
    if (result >= 0)
	return result;
    gc();
    result = do_hash_contains(crc);
    if (result >= 0)
	return result;
    fprintf(stderr, "internal error: can't find value, table full\n");
    abort();
    /*NOTREACHED*/
}

static int do_hash_delete(uint32_t crc) {
    int count;
    uint32_t h = crc;
    for (count = 0; count < nhash; count++) {
	int i = h & (nhash - 1);
	if (occ[i] == FULL && hash[i] == crc) {
	    occ[i] = DELETED;
	    return 1;
	}
	if (occ[i] == EMPTY)
	    return 0;
	h += 2 * (nhash / 4) + 1;
    }
    return -1;
}

int hash_delete(uint32_t crc) {
    int result = do_hash_delete(crc);
    if (result >= 0)
	return result;
    gc();
    result = do_hash_delete(crc);
    if (result >= 0)
	return result;
    fprintf(stderr, "internal error: delete failed, table full\n");
    abort();
    /*NOTREACHED*/
}
