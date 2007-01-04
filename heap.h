/*
 * Copyright (c) 2005-2007 Bart Massey
 * ALL RIGHTS RESERVED
 * Please see the file COPYING in this directory for license information.
 */

extern unsigned *heap;
extern int nheap;
extern void heap_reset(int);
extern unsigned heap_extract_max(void);
extern void heap_insert(unsigned);
