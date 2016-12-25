/*
 * Copyright © 2005-2009 Bart Massey
 * ALL RIGHTS RESERVED
 * [This program is licensed under the "3-clause ('new') BSD License"]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 */

/*
 * Generate and compare simple shingleprints
 * Bart Massey 2005/03
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
#include <math.h>
#include <stdint.h>
#include "crc.h"
#include "heap.h"
#include "hash.h"

#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>

/* size of a shingle in bytes.  should be
   at least 4 to make CRC work */
int nshingle = 8;
int nfeature = 128;
/* were the defaults changed? */
int pset = 0;
/* do a debugging trace? */
int debug_trace = 0;

static struct option long_options[] = {
    {"write-hashfile", 0, 0, 'w'},
    {"match-files", 0, 0, 'm'},
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
static void crc_insert(uint32_t crc) {
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
	uint32_t m = heap_extract_max();
	assert(hash_delete(m));
	if (debug_trace)
	    fprintf(stderr, ">pop %x\n", m);
    }
    if (debug_trace)
	fprintf(stderr, ">push\n");
    hash_insert(crc);
    heap_insert(crc);
}

/* return true if the file had enough bytes
   for at least a single shingle. */
static int running_crc(FILE *f) {
    int i;
    static unsigned char *buf = 0;
    if (buf == 0) {
	buf = (unsigned char *) malloc(nshingle);
	assert(buf);
    }
    for (i = 0; i < nshingle; i++) {
	int ch = fgetc(f);
	if (ch == EOF) {
	    fclose(f);
	    return 0;
	}
	buf[i] = (unsigned char) ch;
    }
    i = 0;
    while(1) {
	int ch;
	crc_insert(hash_crc32(buf, i, nshingle));
	ch = fgetc(f);
	if (ch == EOF) {
	    fclose(f);
	    return 1;
	}
	buf[i] = (unsigned char) ch;
	i = (i + 1) % nshingle;
    }
    assert(0);
    /*NOTREACHED*/
}

typedef struct hashinfo {
    uint16_t nshingle;
    uint16_t nfeature;
    uint32_t *feature;
} hashinfo;

static void free_hashinfo(hashinfo *hi) {
    free(hi->feature);
    free(hi);
}

static hashinfo * get_hashinfo() {
    hashinfo *hi = malloc(sizeof *hi);
    uint32_t *crcs = malloc(nheap * sizeof crcs[0]);
    int i = 0;
    assert(hi);
    assert(crcs);
    hi->nshingle = nshingle;
    hi->nfeature = nheap;
    while (nheap > 0)
	crcs[i++] = heap_extract_max();
    hi->feature = crcs;
    return hi;
}

static hashinfo * hash_file(FILE *f) {
    heap_reset(nfeature);
    hash_reset(nfeature);
    if (!running_crc(f))
	return 0;
    return get_hashinfo();
}


static hashinfo * hash_filename(char *filename) {
    FILE *f = fopen(filename, "r");
    hashinfo *hi;
    if (!f) {
	perror(filename);
	exit(1);
    }
    hi = hash_file(f);
    return hi;
}


static void write_hash(hashinfo *hi, FILE *f) {
    uint16_t s = htons(FILE_VERSION);  /* file/CRC version */
    int i;
    fwrite(&s, sizeof(uint16_t), 1, f);
    s = htons(hi->nshingle);
    fwrite(&s, sizeof(uint16_t), 1, f);
    for(i = 0; i < hi->nfeature; i++) {
	uint32_t hv = htonl(hi->feature[i]);
	fwrite(&hv, sizeof(uint32_t), 1, f);
    }
}

static void write_hashes(int argc, char **argv) {
    int i;
    static char nambuf[MAXPATHLEN + 1];
    for(i = 0; i < argc; i++) {
	hashinfo *hi = hash_filename(argv[i]);
	FILE *of;
	if (hi == 0) {
	    fprintf(stderr, "%s: warning: not hashed\n", argv[i]);
	    continue;
	}
	strncpy(nambuf, argv[i],
		MAXPATHLEN - sizeof(SUFFIX));
	nambuf[MAXPATHLEN - sizeof(SUFFIX)] = '\0';
	strcat(nambuf, SUFFIX);
	of = fopen(nambuf, "w");
	if (!of) {
	    perror(argv[i]);
	    exit(1);
	}
	write_hash(hi, of);
	fclose(of);
	free_hashinfo(hi);
    }
}

/* fills features with the features from f, and returns a
   pointer to info.  A null pointer is returned on error. */
static hashinfo *read_hash(FILE *f) {
    hashinfo *h = malloc(sizeof(hashinfo));
    uint16_t s;
    int i;
    uint16_t version;
    assert(h);
    fread(&s, sizeof(uint16_t), 1, f);
    version = ntohs(s);
    if (version != FILE_VERSION) {
	fprintf(stderr, "bad file version\n");
	return 0;
    }
    fread(&s, sizeof(uint16_t), 1, f);
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
    h->nfeature = i;
    abort();
    /*NOTREACHED*/
}


static hashinfo *read_hashfile(char *name) {
    FILE *f = fopen(name, "r");
    hashinfo *hi;
    if (!f) {
	perror(name);
	exit(1);
    }
    hi = read_hash(f);
    return hi;
}

/* walk backward until one set runs out, counting the
   number of elements in the union of the sets.  the
   backward walk is necessary because the common subsets
   are at the end of the file by construction.  bleah.
   should probably reformat so that it's the other way
   around, which would mean that one could shorten a
   shingleprint by truncation. */
static double score(hashinfo *hi1, hashinfo *hi2) {
    double unionsize;
    double intersectsize;
    int i1 = hi1->nfeature - 1;
    int i2 = hi2->nfeature - 1;
    int count = 0;
    int matchcount = 0;
    while(i1 >= 0 && i2 >= 0) {
	if (hi1->feature[i1] < hi2->feature[i2]) {
	    --i1;
	    continue;
	}
	if(hi1->feature[i1] > hi2->feature[i2]) {
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

void print_score(int fieldwidth, double s) {
    int lead = fieldwidth - 3;
    int i;
    for (i = 0; i < lead; i++)
	printf(" ");
    if (s == -1) {
	printf(" ? ");
    } else if (s == 1.0) {
	printf("1.0");
    } else {
	printf(".%02d", (int)floor(s * 100));
    }
}

static void compare_hashes(char *name1, char *name2) {
    hashinfo *hi1, *hi2;
    hi1 = read_hashfile(name1);
    if (!hi1)
	exit(1);
    hi2 = read_hashfile(name2);
    if (!hi2)
	exit(1);
    if (hi1->nshingle != hi2->nshingle) {
	fprintf(stderr, "shingle size mismatch\n");
	exit(1);
    }
#if 0
    /* this isn't normally necessary when things are
       working properly */
    if (hi1->nfeature != hi2->nfeature)
	fprintf(stderr, "warning: feature set size mismatch %d %d\n",
		hi1->nfeature, hi2->nfeature);
#endif
    print_score(0, score(hi1, hi2));
    printf("\n");
    free_hashinfo(hi1);
    free_hashinfo(hi2);
}


static int width(int n) {
    int i = 0;
    int k = 1;
    while (k <= n) {
	k *= 10;
	i++;
    }
    return i;
}

static void print_index(int fieldwidth, int value) {
    int n = width(value);
    int lead = fieldwidth - n;
    int i;
    for (i = 0; i < lead; i++)
	printf(" ");
    printf("%d", value);
}

static void match_hashes(int argc, char **argv) {
    hashinfo **his = malloc(argc * sizeof *his);
    double **scores = malloc(argc * sizeof *scores);
    int nfilename = 0;
    int i, j;
    int fieldwidth;
    if (argc <= 0)
	return;
    assert(his);
    assert(scores);
    /* compute filename hashes */
    for (i = 0; i < argc; i++)
	his[i] = hash_filename(argv[i]);
    /* build score matrix */
    for (i = 0; i < argc; i++) {
	scores[i] = malloc(argc * sizeof **scores);
	assert(scores[i]);
	for (j = 0; j < i; j++)
	    if (his[i] && his[j])
		scores[i][j] = score(his[i], his[j]);
	    else
		scores[i][j] = -1;
    }
    /* find maximum filename length */
    for (i = 0; i < argc; i++) {
	int n = strlen(argv[i]);
	if (n > nfilename)
	    nfilename = n;
    }
    /* find the field width */
    fieldwidth = width(argc);
    if (fieldwidth < 3)
	fieldwidth = 3;
    /* print the first row of indices */
    for (i = 0; i <= nfilename + fieldwidth; i++)
	printf(" ");
    for (i = 1; i < argc - 1; i++) {
	print_index(fieldwidth, i);
	printf(" ");
    }
    print_index(fieldwidth, argc - 1);
    printf("\n");
    /* print the rows of the matrix */
    for (i = 0; i < argc; i++) {
	printf("%s", argv[i]);
	for (j = strlen(argv[i]); j <= nfilename; j++)
	    printf(" ");
	print_index(fieldwidth, i + 1);
	if (i > 0)
	    printf(" ");
	for (j = 0; j < i - 1; j++) {
	    print_score(fieldwidth, scores[i][j]);
	    printf(" ");
	}
	if (i > 0)
	    print_score(fieldwidth, scores[i][i - 1]);
	printf("\n");
    }
}


static void usage(void) {
    fprintf(stderr, "simhash: usage:\n"
	    "\tsimhash [-s nshingles] [-f nfeatures] [file]\n"
	    "\tsimhash [-s nshingles] [-f nfeatures] [-w|-m] file ...\n"
	    "\tsimhash -c hashfile hashfile\n");
    exit(1);
}

int main(int argc, char **argv) {
    char mode = '?';
    FILE *fin = stdin;
    /* parse initial arguments */
    while(1) {
	switch(getopt_long(argc, argv, "wmcs:f:d",
			   long_options, 0)) {
	case 'w':
	    mode = 'w';
	    continue;
	case 'm':
	    mode = 'm';
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
	    hashinfo *hi;
	case 1:
	    hi = hash_filename(argv[optind]);
	    if (!hi) {
		fprintf(stderr, "%s: not hashable\n", argv[optind]);
		return -1;
	    }
	    write_hash(hi, stdout);
	    free_hashinfo(hi);
	    return 0;
	case 0:
	    hi = hash_file(fin);
	    if (!hi) {
		fprintf(stderr, "stdin not hashable\n");
		return -1;
	    }
	    write_hash(hi, stdout);
	    free_hashinfo(hi);
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
    case 'm':
	match_hashes(argc - optind, argv + optind);
	return 0;
    }
    abort();
    /*NOTREACHED*/
}
