#include "page.h"
#include <stdint.h>
#include "console.h"

/* The physical memory available for paging starts after the kernel heap start.
 * In linker.ld, we will define __heap_start and use it as the start of our managed pool.
 * Total RAM is 32MB (0x02000000).
 */
extern uint8_t __heap_start[];
#define RAM_LIMIT 0x02000000
#define PAGE_SIZE 4096

/* Bitmap to track pages (32MB / 4KB = 8192 pages. 8192 bits = 1024 bytes) */
#define NUM_PAGES (RAM_LIMIT / PAGE_SIZE)
static uint8_t page_bitmap[NUM_PAGES / 8];
static int total_free_pages = 0;
static int total_pages = 0;

/* Start index of pages managed by the allocator */
static int first_managed_page = 0;

static void local_itoa(int n, char s[]) {
    int i = 0, sign = n;
    if (sign < 0) n = -n;
    do { s[i++] = n % 10 + '0'; } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = s[j]; s[j] = s[k]; s[k] = temp;
    }
}

void page_init(void) {
    uint32_t start_addr = (uint32_t)__heap_start;
    start_addr = (start_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); /* Align up to 4KB */
    
    first_managed_page = start_addr / PAGE_SIZE;
    
    /* Mark all pages up to first_managed_page as USED (bit = 1) */
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        if (i < first_managed_page) {
            page_bitmap[i / 8] |= (1 << (i % 8));
        } else {
            page_bitmap[i / 8] &= ~(1 << (i % 8));
            total_free_pages++;
        }
    }
    total_pages = NUM_PAGES - first_managed_page;
    
    kputs("Page Allocator: Initialized. ");
    char buf[16];
    local_itoa(total_free_pages * 4, buf);
    kputs(buf);
    kputs(" KB free.\n");
}

void *alloc_pages(int count) {
    if (count <= 0) return NULL;
    
    int consecutive = 0;
    int start_page = -1;
    
    for (int i = first_managed_page; i < NUM_PAGES; i++) {
        if ((page_bitmap[i / 8] & (1 << (i % 8))) == 0) {
            if (consecutive == 0) start_page = i;
            consecutive++;
            if (consecutive == count) {
                /* Found enough pages, mark them used */
                for (int j = 0; j < count; j++) {
                    page_bitmap[(start_page + j) / 8] |= (1 << ((start_page + j) % 8));
                }
                total_free_pages -= count;
                return (void *)(start_page * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return NULL; /* Out of memory */
}

void *alloc_page(void) {
    return alloc_pages(1);
}

void free_pages(void *ptr, int count) {
    uint32_t addr = (uint32_t)ptr;
    if (addr % PAGE_SIZE != 0 || count <= 0) return; /* Must be page aligned */
    
    int start_page = addr / PAGE_SIZE;
    if (start_page < first_managed_page || start_page + count > NUM_PAGES) return;
    
    for (int i = 0; i < count; i++) {
        page_bitmap[(start_page + i) / 8] &= ~(1 << ((start_page + i) % 8));
    }
    total_free_pages += count;
}

void free_page(void *ptr) {
    free_pages(ptr, 1);
}

int get_free_memory(void) {
    return total_free_pages * PAGE_SIZE;
}

int get_total_memory(void) {
    return total_pages * PAGE_SIZE;
}
