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

/* HASH FILE VERSION */
#define FILE_VERSION 0xcb01

/* SUFFIX for hash outputs */
#define SUFFIX ".sim"

/* NFEATURES should be a power of 2 */
#define NFEATURES 64

/* size of a shingle in bytes.  should be
   at least 4 to make CRC32 work */
#define NSHINGLE 8

extern int hash_crc32(char *buf, int i0, int nbuf);

/* heap max priority queue.  should probably be in a separate file */

static int heap[NFEATURES];
int nheap = 0;

static void heap_reset(void) {
    nheap = 0;
}

/* push the top of heap down as needed to
   restore the heap property */
static void downheap(void) {
    int tmp;
    int i = 0;
    while(1) {
	int left =  (i << 1) + 1;
	int right = left + 1;
	if (left >= nheap)
	    return;
	if (right >= nheap) {
	    if (heap[i] < heap[left]) {
		tmp = heap[left];
		heap[left] = heap[i];
		heap[i] = tmp;
	    }
	    return;
	}
	if (heap[i] >= heap[left] &&
	    heap[i] >= heap[right])
	    return;
	if (heap[left] > heap[right]) {
	    tmp = heap[left];
	    heap[left] = heap[i];
	    heap[i] = tmp;
	    i = left;
	} else {
	    tmp = heap[right];
	    heap[right] = heap[i];
	    heap[i] = tmp;
	    i = right;
	}
    }
}

static int heap_extract_max(void) {
    int m;
    assert(nheap > 0);
    /* lift the last heap element to the top,
       replacing the current top element */
    m = heap[0];
    heap[0] = heap[--nheap];
    /* now restore the heap property */
    downheap();
    /* and return the former top */
    return m;
}

/* lift the last value on the heap up
   as needed to restore the heap property */
static void upheap(void) {
    int i = nheap - 1;
    assert(nheap > 0);
    while(i > 0) {
	int tmp;
	int parent = (i - 1) >> 1;
	if (heap[parent] >= heap[i])
	    return;
	tmp = heap[parent];
	heap[parent] = heap[i];
	heap[i] = tmp;
	i = parent;
    }
}

static void heap_insert(int v) {
    assert(nheap < NFEATURES);
    heap[nheap++] = v;
    upheap();
}

/* end heap code */

/* if crc is less than top of heap, extract
   top-of-heap, then insert crc.  don't worry
   about sign bits---doesn't matter here. */
static void crc_insert(int crc) {
    if(nheap == NFEATURES && heap[0] <= crc)
	return;
    if(nheap == NFEATURES)
	(void)heap_extract_max();
    heap_insert(crc);
}


/* return true if the file had at least enough bytes
   for a single shingle. */
static int running_crc(FILE *f) {
    char buf[NSHINGLE];
    int i;
    for (i = 0; i < NSHINGLE; i++) {
	int ch = fgetc(f);
	if (ch == EOF)
	    return 0;
	buf[i] = ch;
    }
    i = 0;
    while(1) {
	int ch;
	crc_insert(hash_crc32(buf, i, NSHINGLE));
	ch = fgetc(f);
	if (ch == EOF)
	    return 1;
	buf[i] = ch;
	i = (i + 1) % NSHINGLE;
    }
    assert(0);
    /*NOTREACHED*/
}


static void write_hash(FILE *f) {
    short s = htons(FILE_VERSION);  /* file/CRC version */
    fwrite(&s, sizeof(short), 1, f);
    s = htons(NSHINGLE);
    fwrite(&s, sizeof(short), 1, f);
    while(nheap > 0) {
	int hv = htonl(heap_extract_max());
	fwrite(&hv, sizeof(int), 1, f);
    }
}


static char nambuf[MAXPATHLEN];

void write_hashes(int argc, char **argv) {
    int i;
    for(i = 2; i < argc; i++) {
	FILE *f = fopen(argv[i], "r");
	FILE *of;
	if (!f) {
	    perror(argv[i]);
	    exit(1);
	}
	heap_reset();
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
    unsigned int nfeatures;
} hashinfo;
    

/* fills features with the features from f, and returns
   a pointer to info.  The info pointer is static data,
   the results are overwritten on each call.  A null pointer
   is returned on error. */
static hashinfo *read_hash(FILE *f, int *features, int nfeatures) {
    static hashinfo h;
    short s;
    int i;
    fread(&s, sizeof(short), 1, f);
    h.version = ntohs(s);
    if (h.version != FILE_VERSION) {
	fprintf(stderr, "bad file version\n");
	return 0;
    }
    fread(&s, sizeof(short), 1, f);
    h.nshingle = ntohs(s);
    i = 0;
    while(1) {
	int fe;
	int nread = fread(&fe, sizeof(int), 1, f);
	if (nread <= 0) {
	    if (ferror(f)) {
		perror("fread");
		return 0;
	    }
	    h.nfeatures = i;
	    return &h;
	}
	if (i >= nfeatures) {
	    fprintf(stderr, "too many features\n");
	    return 0;
	}
	features[i++] = ntohl(fe);
    }
    abort();
    /*NOTREACHED*/
}


static int read_hashfile(char *name, hashinfo *hi, int *h, int nh) {
    hashinfo *tmp;
    FILE *f = fopen(name, "r");
    if (!f) {
	perror(name);
	exit(1);
    }
    tmp = read_hash(f, h, nh);
    fclose(f);
    if (!tmp)
	return 0;
    *hi = *tmp;
    return 1;
}

hashinfo hi1, hi2;
int hash1[NFEATURES], hash2[NFEATURES];

static double score(void) {
    double unionsize;
    int i1 = 0;
    int i2 = 0;
    int matchcount = 0;
    while(i1 < hi1.nfeatures && i2 < hi2.nfeatures) {
	if (hash1[i1] < hash2[i2]) {
	    i2++;
	    continue;
	}
	if(hash1[i1] > hash2[i2]) {
	    i1++;
	    continue;
	}
	matchcount++;
	i1++;
	i2++;
    }
    unionsize = hi1.nfeatures + hi2.nfeatures - matchcount;
    return matchcount / unionsize;
}

static void compare_hashes(char *name1, char *name2) {
    if (!read_hashfile(name1, &hi1, hash1, NFEATURES))
	exit(1);
    if (!read_hashfile(name2, &hi2, hash2, NFEATURES))
	exit(1);
    if (hi1.nshingle != hi2.nshingle) {
	fprintf(stderr, "shingle size mismatch\n");
	exit(1);
    }
    if (hi1.nfeatures != NFEATURES ||
	hi2.nfeatures != NFEATURES)
	fprintf(stderr, "warning: unexpected feature count\n");
    printf("%g\n", score());
}


void usage(void) {
    fprintf(stderr, "simhash: usage:\n"
	    "\tsimhash [file]\n"
	    "\tsimhash -w [file] ...\n"
	    "\tsimhash -c hashfile hashfile\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc == 1) {
	heap_reset();
	running_crc(stdin);
	write_hash(stdout);
	return 0;
    }
    if (!strcmp(argv[1], "-w")) {
	write_hashes(argc, argv);
	return 0;
    }
    if (!strcmp(argv[1], "-c") && argc == 4) {
	compare_hashes(argv[2], argv[3]);
	return 0;
    }
    if (argc == 2) {
	FILE *f = fopen(argv[1], "r");
	if (!f) {
	    perror(argv[1]);
	    exit(1);
	}
	heap_reset();
	running_crc(f);
	fclose(f);
	write_hash(stdout);
	return 0;
    }
    usage();
    abort();
    /*NOTREACHED*/
}

