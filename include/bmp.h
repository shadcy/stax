#ifndef BMP_H
#define BMP_H

#include <stdint.h>

void bmp_load_and_draw(const char *filename, int x_offset, int y_offset);
uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);
uint16_t *bmp_decode_mem(const uint8_t *data, uint32_t len, int *out_w, int *out_h);

#endif
