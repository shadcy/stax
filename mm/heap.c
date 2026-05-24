/* ============================================================================
 * TIOS — heap.c
 * Simple bump allocator with free list implementation

 @shadcy: just a simple mental model to understand;
 we are basically reussing free chucks if possible;
 otherwise if we need more space, we take it from the heap (grow linearly)
 merge neighbouring free chunks;

 kmalloc suballocate heap memory;
 
 (need to verify this part!!) but there is a twist, we will use the space after the metadata to store the free list;
 and  coalesce helps in reducing fragmentation
 * ============================================================================ */

#include "heap.h"
#include <stddef.h>

/* Heap region defined in linker script */
extern uint8_t __heap_start[];
extern uint8_t __heap_end[];

/* Bump allocator state */
static uint8_t *heap_bump = NULL;
static block_t *free_list = NULL;

/* Alignment for allocations */
#define ALIGNMENT 8
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/* ============================================================================
 * heap_init — initialise heap region and free list
 * ============================================================================ */
void heap_init(void)
{
    heap_bump = __heap_start;
    free_list = NULL;
}

/* ============================================================================
 * add_to_free_list — insert a block into the free list (sorted by address)
 * ============================================================================ */
static void add_to_free_list(block_t *block)
{
    block->next = NULL;
    
    if (!free_list) {
        free_list = block;
        return;
    }
    
    /* Insert at head if block address is before current head */
    if (block < free_list) {
        block->next = free_list;
        free_list = block;
        return;
    }
    
    /* Find insertion point */
    block_t *curr = free_list;
    while (curr->next && curr->next < block) {
        curr = curr->next;
    }
    
    block->next = curr->next;
    curr->next = block;
}

/* ============================================================================
 * coalesce — merge adjacent free blocks
 * ============================================================================ */
static void coalesce(void)
{
    block_t *curr = free_list;
    
    while (curr && curr->next) {
        if ((uint8_t*)curr + sizeof(block_t) + curr->size == (uint8_t*)curr->next) {
            /* Merge with next block */
            curr->size += sizeof(block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

/* ============================================================================
 * kmalloc — allocate memory
 * ============================================================================ */
void *kmalloc(size_t size)
{
    if (size == 0) return NULL;
    
    /* Align size */
    size = ALIGN_UP(size);
    
    /* Try free list first */
    block_t *prev = NULL;
    block_t *curr = free_list;
    
    while (curr) {
        if (curr->size >= size) {
            /* Found a suitable block */
            if (prev) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }
            
            /* Return data portion */
            return curr->data;
        }
        prev = curr;
        curr = curr->next;
    }
    
    /* No suitable free block, allocate from bump */
    size_t total_size = sizeof(block_t) + size;
    uint8_t *new_block = heap_bump;
    
    if (new_block + total_size > __heap_end) {
        return NULL;  /* Out of memory */
    }
    
    heap_bump += total_size;
    
    /* Initialize block header */
    block_t *block = (block_t*)new_block;
    block->size = size;
    block->next = NULL;
    
    return block->data;
}

/* ============================================================================
 * kfree — free memory
 * ============================================================================ */
void kfree(void *ptr)
{
    if (!ptr) return;
    
    /* Get block header */
    block_t *block = (block_t*)((uint8_t*)ptr - offsetof(block_t, data));
    
    /* Add to free list and coalesce */
    add_to_free_list(block);
    coalesce();
}

/* ============================================================================
 * heap_stats — print heap statistics
 * ============================================================================ */
void heap_stats(void)
{
    size_t free_blocks = 0;
    size_t free_bytes = 0;
    size_t bump_used = heap_bump - __heap_start;
    
    block_t *curr = free_list;
    while (curr) {
        free_blocks++;
        free_bytes += curr->size;
        curr = curr->next;
    }
    
    /* Print heap statistics using console functions */
    extern void kputs(const char *s);
    extern void kput_uint(unsigned int n);
    
    kputs("Heap size: ");
    kput_uint(HEAP_SIZE);
    kputs(" bytes\n");
    
    kputs("Bump allocated: ");
    kput_uint(bump_used);
    kputs(" bytes\n");
    
    kputs("Free blocks: ");
    kput_uint(free_blocks);
    kputs("\n");
    
    kputs("Free bytes: ");
    kput_uint(free_bytes);
    kputs(" bytes\n");
    
    kputs("Total used: ");
    kput_uint(bump_used - free_bytes);
    kputs(" bytes\n");
}
