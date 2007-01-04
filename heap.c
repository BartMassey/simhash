/*
 * heap max int priority queue
 * Bart Massey 2005/03
 */
#include <assert.h>
#include <stdlib.h>
#include "heap.h"

unsigned *heap = 0;
int nheap = 0;
static int maxheap = 0;

void heap_reset(int size) {
    nheap = 0;
    maxheap = size;
    if (heap)
	free (heap);
    heap = malloc(size * sizeof(*heap));
    assert(heap);
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

unsigned heap_extract_max(void) {
    unsigned m;
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

void heap_insert(unsigned v) {
    assert(nheap < maxheap);
    heap[nheap++] = v;
    upheap();
}
