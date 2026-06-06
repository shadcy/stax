#ifndef BMP_H
#define BMP_H

#include <stdint.h>

void bmp_load_and_draw(const char *filename, int x_offset, int y_offset);
uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);

#endif
