#ifndef PAGE_H
#define PAGE_H

#include <stddef.h>

void page_init(void);
void *alloc_page(void);
void *alloc_pages(int count);
void free_page(void *ptr);
void free_pages(void *ptr, int count);
int get_free_memory(void);
int get_total_memory(void);

#endif
