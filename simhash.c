/*
 * Copyright (c) 2005-2007 Bart Massey
 * ALL RIGHTS RESERVED
 * Please see the end of this file for license information.
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
#if 0
    /* this isn't normally necessary when things are
       working properly */
    if (hi1->nfeature != hi2->nfeature)
	fprintf(stderr, "warning: feature set size mismatch %d %d\n",
		hi1->nfeature, hi2->nfeature);
#endif
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




/*
 * 
 * 		    GNU GENERAL PUBLIC LICENSE
 * 		       Version 2, June 1991
 * 
 *  Copyright (C) 1989, 1991 Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *  Everyone is permitted to copy and distribute verbatim copies
 *  of this license document, but changing it is not allowed.
 * 
 * 			    Preamble
 * 
 *   The licenses for most software are designed to take away your
 * freedom to share and change it.  By contrast, the GNU General Public
 * License is intended to guarantee your freedom to share and change free
 * software--to make sure the software is free for all its users.  This
 * General Public License applies to most of the Free Software
 * Foundation's software and to any other program whose authors commit to
 * using it.  (Some other Free Software Foundation software is covered by
 * the GNU Lesser General Public License instead.)  You can apply it to
 * your programs, too.
 * 
 *   When we speak of free software, we are referring to freedom, not
 * price.  Our General Public Licenses are designed to make sure that you
 * have the freedom to distribute copies of free software (and charge for
 * this service if you wish), that you receive source code or can get it
 * if you want it, that you can change the software or use pieces of it
 * in new free programs; and that you know you can do these things.
 * 
 *   To protect your rights, we need to make restrictions that forbid
 * anyone to deny you these rights or to ask you to surrender the rights.
 * These restrictions translate to certain responsibilities for you if you
 * distribute copies of the software, or if you modify it.
 * 
 *   For example, if you distribute copies of such a program, whether
 * gratis or for a fee, you must give the recipients all the rights that
 * you have.  You must make sure that they, too, receive or can get the
 * source code.  And you must show them these terms so they know their
 * rights.
 * 
 *   We protect your rights with two steps: (1) copyright the software, and
 * (2) offer you this license which gives you legal permission to copy,
 * distribute and/or modify the software.
 * 
 *   Also, for each author's protection and ours, we want to make certain
 * that everyone understands that there is no warranty for this free
 * software.  If the software is modified by someone else and passed on, we
 * want its recipients to know that what they have is not the original, so
 * that any problems introduced by others will not reflect on the original
 * authors' reputations.
 * 
 *   Finally, any free program is threatened constantly by software
 * patents.  We wish to avoid the danger that redistributors of a free
 * program will individually obtain patent licenses, in effect making the
 * program proprietary.  To prevent this, we have made it clear that any
 * patent must be licensed for everyone's free use or not licensed at all.
 * 
 *   The precise terms and conditions for copying, distribution and
 * modification follow.
 * 
 * 		    GNU GENERAL PUBLIC LICENSE
 *    TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 * 
 *   0. This License applies to any program or other work which contains
 * a notice placed by the copyright holder saying it may be distributed
 * under the terms of this General Public License.  The "Program", below,
 * refers to any such program or work, and a "work based on the Program"
 * means either the Program or any derivative work under copyright law:
 * that is to say, a work containing the Program or a portion of it,
 * either verbatim or with modifications and/or translated into another
 * language.  (Hereinafter, translation is included without limitation in
 * the term "modification".)  Each licensee is addressed as "you".
 * 
 * Activities other than copying, distribution and modification are not
 * covered by this License; they are outside its scope.  The act of
 * running the Program is not restricted, and the output from the Program
 * is covered only if its contents constitute a work based on the
 * Program (independent of having been made by running the Program).
 * Whether that is true depends on what the Program does.
 * 
 *   1. You may copy and distribute verbatim copies of the Program's
 * source code as you receive it, in any medium, provided that you
 * conspicuously and appropriately publish on each copy an appropriate
 * copyright notice and disclaimer of warranty; keep intact all the
 * notices that refer to this License and to the absence of any warranty;
 * and give any other recipients of the Program a copy of this License
 * along with the Program.
 * 
 * You may charge a fee for the physical act of transferring a copy, and
 * you may at your option offer warranty protection in exchange for a fee.
 * 
 *   2. You may modify your copy or copies of the Program or any portion
 * of it, thus forming a work based on the Program, and copy and
 * distribute such modifications or work under the terms of Section 1
 * above, provided that you also meet all of these conditions:
 * 
 *     a) You must cause the modified files to carry prominent notices
 *     stating that you changed the files and the date of any change.
 * 
 *     b) You must cause any work that you distribute or publish, that in
 *     whole or in part contains or is derived from the Program or any
 *     part thereof, to be licensed as a whole at no charge to all third
 *     parties under the terms of this License.
 * 
 *     c) If the modified program normally reads commands interactively
 *     when run, you must cause it, when started running for such
 *     interactive use in the most ordinary way, to print or display an
 *     announcement including an appropriate copyright notice and a
 *     notice that there is no warranty (or else, saying that you provide
 *     a warranty) and that users may redistribute the program under
 *     these conditions, and telling the user how to view a copy of this
 *     License.  (Exception: if the Program itself is interactive but
 *     does not normally print such an announcement, your work based on
 *     the Program is not required to print an announcement.)
 * 
 * These requirements apply to the modified work as a whole.  If
 * identifiable sections of that work are not derived from the Program,
 * and can be reasonably considered independent and separate works in
 * themselves, then this License, and its terms, do not apply to those
 * sections when you distribute them as separate works.  But when you
 * distribute the same sections as part of a whole which is a work based
 * on the Program, the distribution of the whole must be on the terms of
 * this License, whose permissions for other licensees extend to the
 * entire whole, and thus to each and every part regardless of who wrote it.
 * 
 * Thus, it is not the intent of this section to claim rights or contest
 * your rights to work written entirely by you; rather, the intent is to
 * exercise the right to control the distribution of derivative or
 * collective works based on the Program.
 * 
 * In addition, mere aggregation of another work not based on the Program
 * with the Program (or with a work based on the Program) on a volume of
 * a storage or distribution medium does not bring the other work under
 * the scope of this License.
 * 
 *   3. You may copy and distribute the Program (or a work based on it,
 * under Section 2) in object code or executable form under the terms of
 * Sections 1 and 2 above provided that you also do one of the following:
 * 
 *     a) Accompany it with the complete corresponding machine-readable
 *     source code, which must be distributed under the terms of Sections
 *     1 and 2 above on a medium customarily used for software interchange; or,
 * 
 *     b) Accompany it with a written offer, valid for at least three
 *     years, to give any third party, for a charge no more than your
 *     cost of physically performing source distribution, a complete
 *     machine-readable copy of the corresponding source code, to be
 *     distributed under the terms of Sections 1 and 2 above on a medium
 *     customarily used for software interchange; or,
 * 
 *     c) Accompany it with the information you received as to the offer
 *     to distribute corresponding source code.  (This alternative is
 *     allowed only for noncommercial distribution and only if you
 *     received the program in object code or executable form with such
 *     an offer, in accord with Subsection b above.)
 * 
 * The source code for a work means the preferred form of the work for
 * making modifications to it.  For an executable work, complete source
 * code means all the source code for all modules it contains, plus any
 * associated interface definition files, plus the scripts used to
 * control compilation and installation of the executable.  However, as a
 * special exception, the source code distributed need not include
 * anything that is normally distributed (in either source or binary
 * form) with the major components (compiler, kernel, and so on) of the
 * operating system on which the executable runs, unless that component
 * itself accompanies the executable.
 * 
 * If distribution of executable or object code is made by offering
 * access to copy from a designated place, then offering equivalent
 * access to copy the source code from the same place counts as
 * distribution of the source code, even though third parties are not
 * compelled to copy the source along with the object code.
 * 
 *   4. You may not copy, modify, sublicense, or distribute the Program
 * except as expressly provided under this License.  Any attempt
 * otherwise to copy, modify, sublicense or distribute the Program is
 * void, and will automatically terminate your rights under this License.
 * However, parties who have received copies, or rights, from you under
 * this License will not have their licenses terminated so long as such
 * parties remain in full compliance.
 * 
 *   5. You are not required to accept this License, since you have not
 * signed it.  However, nothing else grants you permission to modify or
 * distribute the Program or its derivative works.  These actions are
 * prohibited by law if you do not accept this License.  Therefore, by
 * modifying or distributing the Program (or any work based on the
 * Program), you indicate your acceptance of this License to do so, and
 * all its terms and conditions for copying, distributing or modifying
 * the Program or works based on it.
 * 
 *   6. Each time you redistribute the Program (or any work based on the
 * Program), the recipient automatically receives a license from the
 * original licensor to copy, distribute or modify the Program subject to
 * these terms and conditions.  You may not impose any further
 * restrictions on the recipients' exercise of the rights granted herein.
 * You are not responsible for enforcing compliance by third parties to
 * this License.
 * 
 *   7. If, as a consequence of a court judgment or allegation of patent
 * infringement or for any other reason (not limited to patent issues),
 * conditions are imposed on you (whether by court order, agreement or
 * otherwise) that contradict the conditions of this License, they do not
 * excuse you from the conditions of this License.  If you cannot
 * distribute so as to satisfy simultaneously your obligations under this
 * License and any other pertinent obligations, then as a consequence you
 * may not distribute the Program at all.  For example, if a patent
 * license would not permit royalty-free redistribution of the Program by
 * all those who receive copies directly or indirectly through you, then
 * the only way you could satisfy both it and this License would be to
 * refrain entirely from distribution of the Program.
 * 
 * If any portion of this section is held invalid or unenforceable under
 * any particular circumstance, the balance of the section is intended to
 * apply and the section as a whole is intended to apply in other
 * circumstances.
 * 
 * It is not the purpose of this section to induce you to infringe any
 * patents or other property right claims or to contest validity of any
 * such claims; this section has the sole purpose of protecting the
 * integrity of the free software distribution system, which is
 * implemented by public license practices.  Many people have made
 * generous contributions to the wide range of software distributed
 * through that system in reliance on consistent application of that
 * system; it is up to the author/donor to decide if he or she is willing
 * to distribute software through any other system and a licensee cannot
 * impose that choice.
 * 
 * This section is intended to make thoroughly clear what is believed to
 * be a consequence of the rest of this License.
 * 
 *   8. If the distribution and/or use of the Program is restricted in
 * certain countries either by patents or by copyrighted interfaces, the
 * original copyright holder who places the Program under this License
 * may add an explicit geographical distribution limitation excluding
 * those countries, so that distribution is permitted only in or among
 * countries not thus excluded.  In such case, this License incorporates
 * the limitation as if written in the body of this License.
 * 
 *   9. The Free Software Foundation may publish revised and/or new versions
 * of the General Public License from time to time.  Such new versions will
 * be similar in spirit to the present version, but may differ in detail to
 * address new problems or concerns.
 * 
 * Each version is given a distinguishing version number.  If the Program
 * specifies a version number of this License which applies to it and "any
 * later version", you have the option of following the terms and conditions
 * either of that version or of any later version published by the Free
 * Software Foundation.  If the Program does not specify a version number of
 * this License, you may choose any version ever published by the Free Software
 * Foundation.
 * 
 *   10. If you wish to incorporate parts of the Program into other free
 * programs whose distribution conditions are different, write to the author
 * to ask for permission.  For software which is copyrighted by the Free
 * Software Foundation, write to the Free Software Foundation; we sometimes
 * make exceptions for this.  Our decision will be guided by the two goals
 * of preserving the free status of all derivatives of our free software and
 * of promoting the sharing and reuse of software generally.
 * 
 * 			    NO WARRANTY
 * 
 *   11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
 * FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
 * OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
 * PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
 * OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
 * TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
 * PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
 * REPAIR OR CORRECTION.
 * 
 *   12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
 * WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
 * REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
 * INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
 * OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
 * TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
 * YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
 * PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 * 
 * 		     END OF TERMS AND CONDITIONS
 * 
 * 	    How to Apply These Terms to Your New Programs
 * 
 *   If you develop a new program, and you want it to be of the greatest
 * possible use to the public, the best way to achieve this is to make it
 * free software which everyone can redistribute and change under these terms.
 * 
 *   To do so, attach the following notices to the program.  It is safest
 * to attach them to the start of each source file to most effectively
 * convey the exclusion of warranty; and each file should have at least
 * the "copyright" line and a pointer to where the full notice is found.
 * 
 *     <one line to give the program's name and a brief idea of what it does.>
 *     Copyright (C) <year>  <name of author>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * Also add information on how to contact you by electronic and paper mail.
 * 
 * If the program is interactive, make it output a short notice like this
 * when it starts in an interactive mode:
 * 
 *     Gnomovision version 69, Copyright (C) year name of author
 *     Gnomovision comes with ABSOLUTELY NO WARRANTY; for details type `show w'.
 *     This is free software, and you are welcome to redistribute it
 *     under certain conditions; type `show c' for details.
 * 
 * The hypothetical commands `show w' and `show c' should show the appropriate
 * parts of the General Public License.  Of course, the commands you use may
 * be called something other than `show w' and `show c'; they could even be
 * mouse-clicks or menu items--whatever suits your program.
 * 
 * You should also get your employer (if you work as a programmer) or your
 * school, if any, to sign a "copyright disclaimer" for the program, if
 * necessary.  Here is a sample; alter the names:
 * 
 *   Yoyodyne, Inc., hereby disclaims all copyright interest in the program
 *   `Gnomovision' (which makes passes at compilers) written by James Hacker.
 * 
 *   <signature of Ty Coon>, 1 April 1989
 *   Ty Coon, President of Vice
 * 
 * This General Public License does not permit incorporating your program into
 * proprietary programs.  If your program is a subroutine library, you may
 * consider it more useful to permit linking proprietary applications with the
 * library.  If this is what you want to do, use the GNU Lesser General
 * Public License instead of this License.
 */
