#include <stdlib.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* HASH FILE VERSION */
#define FILE_VERSION 0xcb00

/* SUFFIX for hash outputs */
#define SUFFIX ".simhash"

/* NFEATURES should be a power of 2 */
#define NFEATURES 64

/* size of a shingle in bytes.  should be
   at least 4 to make CRC32 work */
#define NSHINGLE 16


/* the CRC code is at the bottom for license inclusion */
static int hash_crc32(char *buf, int i, int nbuf);

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
	if (!f) {
	    perror(argv[i]);
	    exit(1);
	}
	heap_reset();
	running_crc(f);
	fclose(f);
	if (argc > 3) {
	    FILE *of;
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
	} else {
	    write_hash(stdout);
	}
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
	    "\tsimhash -w [file] ...\n"
	    "\tsimhash -c hashfile hashfile\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2)
	usage();
    if (!strcmp(argv[1], "-w")) {
	write_hashes(argc, argv);
	return 0;
    }
    if (!strcmp(argv[1], "-c") && argc == 4) {
	compare_hashes(argv[2], argv[3]);
	return 0;
    }
    usage();
    abort();
    /*NOTREACHED*/
}

/* from cksum -- calculate and print POSIX checksums and sizes of files
   Copyright (C) 92, 1995-2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/* Written by Q. Frank Xia, qx@math.columbia.edu.
   Cosmetic changes and reorganization by David MacKenzie, djm@gnu.ai.mit.edu.

  Usage: cksum [file...]

  The code segment between "#ifdef CRCTAB" and "#else" is the code
  which calculates the "crctab". It is included for those who want
  verify the correctness of the "crctab". To recreate the "crctab",
  do following:

      cc -DCRCTAB -o crctab cksum.c
      crctab > crctab.h

  As Bruce Evans pointed out to me, the crctab in the sample C code
  in 4.9.10 Rationale of P1003.2/D11.2 is represented in reversed order.
  Namely, 0x01 is represented as 0x80, 0x02 is represented as 0x40, etc.
  The generating polynomial is crctab[0x80]=0xedb88320 instead of
  crctab[1]=0x04C11DB7.  But the code works only for a non-reverse order
  crctab.  Therefore, the sample implementation is wrong.

  This software is compatible with neither the System V nor the BSD
  `sum' program.  It is supposed to conform to P1003.2/D11.2,
  except foreign language interface (4.9.5.3 of P1003.2/D11.2) support.
  Any inconsistency with the standard except 4.9.5.3 is a bug.  */

static unsigned int crctab[256] =
{
  0x0,
  0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B,
  0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6,
  0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
  0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC,
  0x5BD4B01B, 0x569796C2, 0x52568B75, 0x6A1936C8, 0x6ED82B7F,
  0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A,
  0x745E66CD, 0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039,
  0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58,
  0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033,
  0xA4AD16EA, 0xA06C0B5D, 0xD4326D90, 0xD0F37027, 0xDDB056FE,
  0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
  0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4,
  0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D, 0x34867077, 0x30476DC0,
  0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5,
  0x2AC12072, 0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16,
  0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA, 0x7897AB07,
  0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C,
  0x6211E6B5, 0x66D0FB02, 0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1,
  0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
  0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B,
  0xBB60ADFC, 0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698,
  0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D,
  0x94EA7B2A, 0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E,
  0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 0xC6BCF05F,
  0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34,
  0xDC3ABDED, 0xD8FBA05A, 0x690CE0EE, 0x6DCDFD59, 0x608EDB80,
  0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
  0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A,
  0x58C1663D, 0x558240E4, 0x51435D53, 0x251D3B9E, 0x21DC2629,
  0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C,
  0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF,
  0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623, 0xF12F560E,
  0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65,
  0xEBA91BBC, 0xEF68060B, 0xD727BBB6, 0xD3E6A601, 0xDEA580D8,
  0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
  0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2,
  0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6, 0x9FF77D71,
  0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74,
  0x857130C3, 0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640,
  0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C, 0x7B827D21,
  0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A,
  0x61043093, 0x65C52D24, 0x119B4BE9, 0x155A565E, 0x18197087,
  0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
  0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D,
  0x2056CD3A, 0x2D15EBE3, 0x29D4F654, 0xC5A92679, 0xC1683BCE,
  0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB,
  0xDBEE767C, 0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18,
  0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4, 0x89B8FD09,
  0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662,
  0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF,
  0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
};


/* IV for crc */
#define IV 0x12345678

/* This is (like) the standard POSIX CRC-32.
   We need a running version of this. */
static int hash_crc32(char *buf, int i0, int nbuf) {
    int crc = 0x12345678;  /* IV */
    int i = i0;
    assert(nbuf >= 4);
    do {
	crc = (crc << 8) ^ crctab[((crc >> 24) ^ buf[i]) & 0xFF];
	i = (i + 1) % nbuf;
    } while (i != i0);
    return crc;
}
