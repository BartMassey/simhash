/*
 * Copyright (c) 2005-2007 Bart Massey
 * ALL RIGHTS RESERVED
 * Please see the file COPYING in this directory for license information.
 */

extern uint32_t *heap;
extern int nheap;
extern void heap_reset(int);
extern uint32_t heap_extract_max(void);
extern void heap_insert(uint32_t);
