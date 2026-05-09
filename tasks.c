/* ============================================================================
 * TIOS — tasks.c
 * User tasks for testing scheduler and filesystem
 * ============================================================================ */

#include "console.h"
#include "scheduler.h"
#include "heap.h"
#include "fat.h"

/* External functions */

static void task_a(void)
{
    kputs("[task A start]\n");
    char *buf = kmalloc(64);
    if (buf) {
        kputs("[task A allocated 64 bytes]\n");
        kfree(buf);
    }
    /* Test filesystem */
    fat_file_t *file = fat_open("TEST.TXT");
    if (file) {
        kputs("[task A opened TEST.TXT]\n");
        char read_buf[32];
        int bytes = fat_read(file, read_buf, sizeof(read_buf) - 1);
        if (bytes > 0) {
            read_buf[bytes] = '\0';
            kputs("[task A read: ");
            for (int i = 0; i < bytes; i++) {
                kputc(read_buf[i]);
            }
            kputs("]\n");
        }
        fat_close(file);
    } else {
        kputs("[task A failed to open TEST.TXT]\n");
    }
    
    /* Run 3 times then exit */
    for (int i = 0; i < 3; i++) {
        kputs("[task A running]\n");
        for (volatile int j = 0; j < 100000; j++) __asm__ volatile ("nop");
    }
    kputs("[task A DONE]\n");
    task_exit();
}

static void task_b(void)
{
    static int count = 0;
    kputs("[task B start]\n");
    char *buf1 = kmalloc(32);
    char *buf2 = kmalloc(128);
    if (buf1 && buf2) {
        kputs("[task B allocated 32+128 bytes]\n");
        kfree(buf2);
        kfree(buf1);
    }
    
    /* Run 5 times then exit */
    for (int i = 0; i < 5; i++) {
        kputs("[task B ");
        kput_uint(count++);
        kputs("]\n");
        for (volatile int j = 0; j < 100000; j++) __asm__ volatile ("nop");
    }
    kputs("[task B DONE]\n");
    task_exit();
}

/* Task creation functions */
void create_tasks(void)
{
    task_create(task_a);
    task_create(task_b);
}
