/* generate and compare simple shingleprints
 * Bart Massey 2005/03
 * Copyright (c) 2005 Bart Massey.
 * ALL RIGHTS RESERVED.
 */

/* Bibliography
 *
 *   Mark Manasse
 *   Microsoft Research Silicon Valley
 *   Finding similar things quickly in large collections
 *   http://research.microsoft.com/research/sv/PageTurner/similarity.htm
 *
 *   Andrei Z. Broder
 *   On the resemblance and containment of documents
 *   In Compression and Complexity of Sequences (SEQUENCES'97),
 *   pages 21-29. IEEE Computer Society, 1998
 *   ftp://ftp.digital.com/pub/DEC/SRC/publications/broder/
 *     positano-final-wpnums.pdf
 *
 *   Andrei Z. Broder
 *   Some applications of Rabin's fingerprinting method
 *   Published in R. Capocelli, A. De Santis, U. Vaccaro eds.
 *   Sequences II: Methods in Communications, Security, and
 *   Computer Science, Springer-Verlag, 1993.
 *   http://athos.rutgers.edu/~muthu/broder.ps
 */

#include <stdlib.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "crc.h"
#include "heap.h"
#include "hash.h"

#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>

/* size of a shingle in bytes.  should be
   at least 4 to make CRC work */
int nshingle = 8;
int nfeature = 64;
/* were the defaults changed? */
int pset = 0;
/* do a debugging trace? */
int debug_trace = 0;

static struct option long_options[] = {
    {"write-hashfile", 0, 0, 'w'},
    {"compare-hashfile", 0, 0, 'c'},
    {"shingle-size", 1, 0, 's'},
    {"feature-set-size", 1, 0, 'f'},
    {"debug-trace", 1, 0, 'd'},
    {0,0,0,0}
};

/* HASH FILE VERSION */
#define FILE_VERSION 0xcb01

/* SUFFIX for hash outputs */
#define SUFFIX ".sim"

/* if crc is less than top of heap, extract
   top-of-heap, then insert crc.  don't worry
   about sign bits---doesn't matter here. */
static void crc_insert(unsigned crc) {
    if (debug_trace)
	fprintf(stderr, ">got %x\n", crc);
    if(nheap == nfeature && crc >= heap[0])
	return;
    if (hash_contains(crc)) {
	if (debug_trace)
	    fprintf(stderr, ">dup\n");
	return;
    }
    if(nheap == nfeature) {
	unsigned m = heap_extract_max();
	assert(hash_delete(m));
	if (debug_trace)
	    fprintf(stderr, ">pop %x\n", m);
    }
    if (debug_trace)
	fprintf(stderr, ">push\n");
    hash_insert(crc);
    heap_insert(crc);
}


char *buf = 0;

/* return true if the file had at least enough bytes
   for a single shingle. */
static int running_crc(FILE *f) {
    int i;
    if (buf == 0) {
	buf = malloc(nshingle);
	assert(buf);
    }
    for (i = 0; i < nshingle; i++) {
	int ch = fgetc(f);
	if (ch == EOF)
	    return 0;
	buf[i] = ch;
    }
    i = 0;
    while(1) {
	int ch;
	crc_insert((unsigned)hash_crc32(buf, i, nshingle));
	ch = fgetc(f);
	if (ch == EOF)
	    return 1;
	buf[i] = ch;
	i = (i + 1) % nshingle;
    }
    assert(0);
    /*NOTREACHED*/
}


static void write_hash(FILE *f) {
    short s = htons(FILE_VERSION);  /* file/CRC version */
    fwrite(&s, sizeof(short), 1, f);
    s = htons(nshingle);
    fwrite(&s, sizeof(short), 1, f);
    while(nheap > 0) {
	unsigned hv = htonl(heap_extract_max());
	fwrite(&hv, sizeof(unsigned), 1, f);
    }
}


static char nambuf[MAXPATHLEN];

void write_hashes(int argc, char **argv) {
    int i;
    for(i = 0; i < argc; i++) {
	FILE *f = fopen(argv[i], "r");
	FILE *of;
	if (!f) {
	    perror(argv[i]);
	    exit(1);
	}
	heap_reset(nfeature);
	hash_reset(nfeature);
	running_crc(f);
	fclose(f);
	strncpy(nambuf, argv[i],
		MAXPATHLEN - sizeof(SUFFIX));
	nambuf[MAXPATHLEN - sizeof(SUFFIX) - 1] = '\0';
	strcat(nambuf, SUFFIX);
	of = fopen(nambuf, "w");
	if (!f) {
	    perror(argv[i]);
	    exit(1);
	}
	write_hash(of);
	fclose(of);
    }
}

typedef struct hashinfo {
    unsigned short version;
    unsigned short nshingle;
    unsigned int nfeature;
    int *feature;
} hashinfo;
    

/* fills features with the features from f, and returns
   a pointer to info.  The info pointer is static data,
   the results are overwritten on each call.  A null pointer
   is returned on error. */
static hashinfo *read_hash(FILE *f) {
    hashinfo *h = malloc(sizeof(hashinfo));
    short s;
    int i;
    assert(h);
    fread(&s, sizeof(short), 1, f);
    h->version = ntohs(s);
    if (h->version != FILE_VERSION) {
	fprintf(stderr, "bad file version\n");
	return 0;
    }
    fread(&s, sizeof(short), 1, f);
    h->nshingle = ntohs(s);
    h->nfeature = 16;
    h->feature = malloc(h->nfeature * sizeof(int));
    assert(h->feature);
    i = 0;
    while(1) {
	int fe;
	int nread = fread(&fe, sizeof(int), 1, f);
	if (nread <= 0) {
	    if (ferror(f)) {
		perror("fread");
		return 0;
	    }
	    h->nfeature = i;
	    h->feature = realloc(h->feature, h->nfeature * sizeof(int));
	    assert(h->feature);
	    return h;
	}
	if (i >= h->nfeature) {
	    h->nfeature *= 2;
	    h->feature = realloc(h->feature, h->nfeature * sizeof(int));
	    assert(h->feature);
	}
	h->feature[i++] = ntohl(fe);
    }
    abort();
    /*NOTREACHED*/
}


static int read_hashfile(char *name, hashinfo **hi) {
    FILE *f = fopen(name, "r");
    if (!f) {
	perror(name);
	exit(1);
    }
    *hi = read_hash(f);
    fclose(f);
    return !!hi;
}

hashinfo *hi1, *hi2;

/* walk backward until one set runs out, counting the
   number of elements in the union of the sets.  the
   backward walk is necessary because the common subsets
   are at the end of the file by construction.  bleah.
   should probably reformat so that it's the other way
   around, which would mean that one could shorten a
   shingleprint by truncation. */
static double score(void) {
    double unionsize;
    double intersectsize;
    int i1 = hi1->nfeature - 1;
    int i2 = hi2->nfeature - 1;
    int count = 0;
    int matchcount = 0;
    while(i1 >= 0 && i2 >= 0) {
	if ((unsigned)(hi1->feature[i1]) < (unsigned)(hi2->feature[i2])) {
	    --i1;
	    continue;
	}
	if((unsigned)(hi1->feature[i1]) > (unsigned)(hi2->feature[i2])) {
	    --i2;
	    continue;
	}
	matchcount++;
	--i1;
	--i2;
    }
    count = hi1->nfeature;
    if (count > hi2->nfeature)
	count = hi2->nfeature;
    intersectsize = matchcount;
    unionsize = 2 * count - matchcount;
    return intersectsize / unionsize;
}

static void compare_hashes(char *name1, char *name2) {
    if (!read_hashfile(name1, &hi1))
	exit(1);
    if (!read_hashfile(name2, &hi2))
	exit(1);
    if (hi1->nshingle != hi2->nshingle) {
	fprintf(stderr, "shingle size mismatch\n");
	exit(1);
    }
    if (hi1->nfeature != hi2->nfeature)
	fprintf(stderr, "warning: feature set size mismatch %d %d\n",
		hi1->nfeature, hi2->nfeature);
    printf("%4.2f\n", score());
}


void usage(void) {
    fprintf(stderr, "simhash: usage:\n"
	    "\tsimhash [-s nshingles] [-f nfeatures] [file]\n"
	    "\tsimhash [-s nshingles] [-f nfeatures] -w [file] ...\n"
	    "\tsimhash -c hashfile hashfile\n");
    exit(1);
}

int main(int argc, char **argv) {
    char mode = '?';
    FILE *fin = stdin;
    /* parse initial arguments */
    while(1) {
	switch(getopt_long(argc, argv, "wcs:f:d",
			   long_options, 0)) {
	case 'w':
	    mode = 'w';
	    continue;
	case 'c':
	    mode = 'c';
	    continue;
	case 's':
	    nshingle = atoi(optarg);
	    if (nshingle < 4) {
		fprintf(stderr, "simhash: shingle size must be at least 4\n");
		exit(1);
	    }
	    pset = 1;
	    continue;
	case 'f':
	    nfeature = atoi(optarg);
	    if (nfeature < 1) {
		fprintf(stderr, "simhash: feature set size must be at least 1\n");
		exit(1);
	    }
	    pset = 1;
	    continue;
	case 'd':
	    debug_trace = 1;
	}
	break;
    }
    /* actually process */
    switch(mode) {
    case '?':
	switch (argc - optind) {
	case 1:
	    fin = fopen(argv[optind], "r");
	    if (!fin) {
		perror(argv[optind]);
		exit(1);
	    }
	    /* fall through */
	case 0:
	    heap_reset(nfeature);
	    hash_reset(nfeature);
	    running_crc(fin);
	    write_hash(stdout);
	    return 0;
	}
	usage();
	abort();
	/*NOTREACHED*/
    case 'w':
	write_hashes(argc - optind, argv + optind);
	return 0;
    case 'c':
	if (pset)
	    usage();
	if (optind != argc - 2)
	    usage();
	compare_hashes(argv[optind], argv[optind + 1]);
	return 0;
    }
    abort();
    /*NOTREACHED*/
}

