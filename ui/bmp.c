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

uint16_t *bmp_decode_mem(const uint8_t *data, uint32_t len, int *out_w, int *out_h) {
    if (!data || len < sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t))
        return NULL;

    const bmp_file_header_t *fh = (const bmp_file_header_t *)data;
    if (fh->type != 0x4D42) /* "BM" */
        return NULL;

    const bmp_info_header_t *ih = (const bmp_info_header_t *)(data + sizeof(bmp_file_header_t));
    int w = ih->width;
    int h = ih->height;
    int top_down = 0;
    if (h < 0) {
        h = -h;
        top_down = 1;
    }
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
        return NULL;

    if (fh->offset >= len)
        return NULL;

    int bpp = ih->bpp;
    if (bpp != 16 && bpp != 24 && bpp != 32)
        return NULL;

    uint16_t *img_buf = (uint16_t *)kmalloc(w * h * sizeof(uint16_t));
    if (!img_buf)
        return NULL;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    int bytes_per_pixel = bpp / 8;
    int row_stride = ((w * bytes_per_pixel) + 3) & ~3;
    const uint8_t *pixel_data = data + fh->offset;

    for (int y = 0; y < h; y++) {
        int src_y = top_down ? y : (h - 1 - y);
        const uint8_t *row = pixel_data + src_y * row_stride;
        if ((uint32_t)(fh->offset + src_y * row_stride + row_stride) > len)
            break;

        for (int x = 0; x < w; x++) {
            uint16_t c16 = 0;
            if (bpp == 16) {
                c16 = *(const uint16_t *)(&row[x * 2]);
            } else if (bpp == 24) {
                uint8_t b = row[x * 3 + 0];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];
                c16 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            } else if (bpp == 32) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                c16 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
            img_buf[y * w + x] = c16;
        }
    }

    return img_buf;
}

void bmp_load_and_draw(const char *filename, int x_offset, int y_offset) {
    int w = 0, h = 0;
    uint16_t *pixels = bmp_load(filename, &w, &h);
    if (!pixels) {
        kputs("bmp: unable to load (");
        kputs(filename);
        kputs(")\n");
        return;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t pixel = pixels[y * w + x];
            if (pixel == 0xF81F) continue; /* Transparent color key */
            int px = x + x_offset;
            int py = y + y_offset;
            if (px >= 0 && px < (int)fb_width && py >= 0 && py < (int)fb_height) {
                fb_putpixel(px, py, pixel);
            }
        }
    }
    kfree(pixels);
}

uint16_t *bmp_load(const char *filename, int *out_w, int *out_h) {
    fat_file_t *file = fat_open(filename);
    if (!file) return NULL;

    /* Get file size */
    FIL *fp = (FIL *)file;
    uint32_t fsize = (uint32_t)f_size(fp);
    if (fsize < sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t) || fsize > 4 * 1024 * 1024) {
        fat_close(file);
        return NULL;
    }

    uint8_t *raw_data = (uint8_t *)kmalloc(fsize);
    if (!raw_data) {
        fat_close(file);
        return NULL;
    }

    int bytes = fat_read(file, raw_data, fsize);
    fat_close(file);
    if (bytes != (int)fsize) {
        kfree(raw_data);
        return NULL;
    }

    uint16_t *img = bmp_decode_mem(raw_data, fsize, out_w, out_h);
    kfree(raw_data);
    return img;
}
