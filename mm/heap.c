/* ============================================================================
 * STAX — heap.c
 * Block allocator over page allocator
 * ============================================================================ */

#include "heap.h"
#include "page.h"
#include "console.h"
#include <stddef.h>

#define ALIGNMENT 8
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define PAGE_SIZE 4096

static block_t *free_list = NULL;

void heap_init(void) {
    free_list = NULL;
    kputs("Heap Allocator: Initialized (backed by MMU paging).\n");
}

static void add_to_free_list(block_t *block) {
    block->next = NULL;
    if (!free_list) {
        free_list = block;
        return;
    }
    if (block < free_list) {
        block->next = free_list;
        free_list = block;
        return;
    }
    block_t *curr = free_list;
    while (curr->next && curr->next < block) {
        curr = curr->next;
    }
    block->next = curr->next;
    curr->next = block;
}

static void coalesce(void) {
    block_t *curr = free_list;
    while (curr && curr->next) {
        if ((uint8_t*)curr + sizeof(block_t) + curr->size == (uint8_t*)curr->next) {
            curr->size += sizeof(block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN_UP(size);
    
    block_t *prev = NULL;
    block_t *curr = free_list;
    
    /* First fit */
    while (curr) {
        if (curr->size >= size) {
            if (curr->size > size + sizeof(block_t) + ALIGNMENT) {
                /* Split block */
                block_t *new_block = (block_t *)((uint8_t*)curr + sizeof(block_t) + size);
                new_block->size = curr->size - size - sizeof(block_t);
                new_block->next = curr->next;
                curr->size = size;
                if (prev) prev->next = new_block;
                else free_list = new_block;
            } else {
                /* Exact match or too small to split */
                if (prev) prev->next = curr->next;
                else free_list = curr->next;
            }
            return (void *)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }
    
    /* Out of memory in the free list, ask page allocator */
    size_t required_size = size + sizeof(block_t);
    int pages_needed = (required_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    block_t *new_mem = (block_t *)alloc_pages(pages_needed);
    if (!new_mem) {
        kputs("kmalloc: OUT OF MEMORY (Page allocator exhausted)!\n");
        return NULL;
    }
    
    new_mem->size = (pages_needed * PAGE_SIZE) - sizeof(block_t);
    new_mem->next = NULL;
    
    /* If the requested pages are larger than what we strictly needed,
       put the remainder into the free list. */
    if (new_mem->size > size + sizeof(block_t) + ALIGNMENT) {
        block_t *remainder = (block_t *)((uint8_t*)new_mem + sizeof(block_t) + size);
        remainder->size = new_mem->size - size - sizeof(block_t);
        new_mem->size = size;
        add_to_free_list(remainder);
        coalesce();
    }
    
    return (void *)(new_mem + 1);
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *block = (block_t *)((uint8_t*)ptr - sizeof(block_t));
    add_to_free_list(block);
    coalesce();
}

uint32_t heap_get_free(void) {
    size_t total = 0;
    block_t *curr = free_list;
    while (curr) {
        total += curr->size;
        curr = curr->next;
    }
    return total;
}

uint32_t heap_get_total(void) {
    /* Total is now determined by the page allocator, so we approximate total managed pages */
    return get_total_memory();
}
