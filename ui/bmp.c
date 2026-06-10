#include "bmp.h"
#include "fat.h"
#include "framebuffer.h"
#include "console.h"
#include "heap.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t res1;
    uint16_t res2;
    uint32_t offset;
} bmp_file_header_t;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_ppm;
    int32_t y_ppm;
    uint32_t clr_used;
    uint32_t clr_important;
} bmp_info_header_t;
#pragma pack(pop)

void bmp_load_and_draw(const char *filename, int x_offset, int y_offset) {
    fat_file_t *file = fat_open(filename);
    if (!file) {
        kputs("bmp: file not found (");
        kputs(filename);
        kputs(")\n");
        return;
    }
    
    bmp_file_header_t fh;
    if (fat_read(file, &fh, sizeof(fh)) != sizeof(fh)) {
        kputs("bmp: read error (fh)\n");
        fat_close(file);
        return;
    }
    
    if (fh.type != 0x4D42) { /* "BM" */
        kputs("bmp: invalid format (not BM)\n");
        fat_close(file);
        return;
    }
    
    bmp_info_header_t ih;
    if (fat_read(file, &ih, sizeof(ih)) != sizeof(ih)) {
        kputs("bmp: read error (ih)\n");
        fat_close(file);
        return;
    }
    
    if (ih.bpp != 16) {
        kputs("bmp: only 16-bit supported\n");
        fat_close(file);
        return;
    }
    
    fat_seek(file, fh.offset);
    
    int row_size = ((ih.width * 2) + 3) & ~3;
    uint8_t *row_buf = (uint8_t *)kmalloc(row_size);
    if (!row_buf) {
        kputs("bmp: out of memory allocating row buffer\n");
        fat_close(file);
        return;
    }
    
    for (int y = ih.height - 1; y >= 0; y--) {
        if (fat_read(file, row_buf, row_size) != (uint32_t)row_size) break;
        for (int x = 0; x < ih.width; x++) {
            uint16_t pixel = *(uint16_t *)(&row_buf[x * 2]);
            /* Plot pixel only if within FB bounds */
            if ((x + x_offset) < FB_WIDTH && (y + y_offset) < FB_HEIGHT &&
                (x + x_offset) >= 0 && (y + y_offset) >= 0) {
                fb_putpixel(x + x_offset, y + y_offset, pixel);
            }
        }
    }
    
    kfree(row_buf);
    fat_close(file);
}

uint16_t *bmp_load(const char *filename, int *out_w, int *out_h) {
    fat_file_t *file = fat_open(filename);
    if (!file) return NULL;
    
    bmp_file_header_t fh;
    if (fat_read(file, &fh, sizeof(fh)) != sizeof(fh) || fh.type != 0x4D42) {
        fat_close(file);
        return NULL;
    }
    
    bmp_info_header_t ih;
    if (fat_read(file, &ih, sizeof(ih)) != sizeof(ih) || ih.bpp != 16) {
        fat_close(file);
        return NULL;
    }
    
    fat_seek(file, fh.offset);
    
    int w = ih.width;
    int h = ih.height;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    
    uint16_t *img_buf = (uint16_t *)kmalloc(w * h * 2);
    if (!img_buf) {
        fat_close(file);
        return NULL;
    }
    
    int row_size = ((w * 2) + 3) & ~3;
    uint8_t *row_buf = (uint8_t *)kmalloc(row_size);
    if (!row_buf) {
        kfree(img_buf);
        fat_close(file);
        return NULL;
    }
    
    for (int y = h - 1; y >= 0; y--) {
        if (fat_read(file, row_buf, row_size) != (uint32_t)row_size) break;
        for (int x = 0; x < w; x++) {
            img_buf[y * w + x] = *(uint16_t *)(&row_buf[x * 2]);
        }
    }
    
    kfree(row_buf);
    fat_close(file);
    return img_buf;
}
