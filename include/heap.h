/* ============================================================================
 * TIOS — heap.h
 * Simple bump allocator with free list for Phase 6d
 * ============================================================================ */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

/* MUST match `. += 65536` in linker.ld.in — keep both in sync */
#define HEAP_SIZE  (64 * 1024)  /* 64 KB heap */

typedef struct block {
    struct block *next;
    size_t size;
    uint8_t data[];
} block_t;

/* Initialise the heap region */
void heap_init(void);

/* Allocate memory (malloc) */
void *kmalloc(size_t size);

/* Free memory (free) */
void kfree(void *ptr);

/* Print heap statistics */
void heap_stats(void);

#endif /* HEAP_H */
