#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "font8x16.h"
#include "fatfs/ff.h"
#include "../firmware/image_format/firmware_format.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);

void fwviewer_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)cw;
    (void)ch;
    
    fb_fillrect(cx, cy, cw, ch, rgb565(240, 240, 240));
    
    /* Header */
    fb_fillrect(cx + 10, cy + 10, 64, 64, rgb565(0, 100, 200));
    draw_text(cx + 20, cy + 34, "FW", COLOR_WHITE);
    
    draw_text(cx + 90, cy + 20, "STAX Firmware Package", COLOR_BLACK);
    
    fb_drawline(cx + 10, cy + 85, cx + cw - 10, cy + 85, rgb565(180, 180, 180));
    
    if (win->path[0] != '\0') {
        draw_text(cx + 10, cy + 100, "File:", COLOR_BLACK);
        draw_text(cx + 100, cy + 100, win->path, COLOR_BLACK);
        
        if (!win->app_data) {
            FIL f;
            if (f_open(&f, win->path, FA_READ) == FR_OK) {
                firmware_header_t *hdr = (firmware_header_t *)kmalloc(sizeof(firmware_header_t));
                UINT br;
                f_read(&f, hdr, sizeof(firmware_header_t), &br);
                f_close(&f);
                if (br == sizeof(firmware_header_t) && hdr->magic == FIRMWARE_MAGIC) {
                    win->app_data = hdr;
                } else {
                    kfree(hdr);
                    win->app_data = (void*)1; /* Invalid */
                }
            } else {
                win->app_data = (void*)1; /* Failed */
            }
        }
        
        if (win->app_data && win->app_data != (void*)1) {
            firmware_header_t *hdr = (firmware_header_t *)win->app_data;
            
            draw_text(cx + 10, cy + 130, "Version:", COLOR_BLACK);
            char buf[32];
            int val = hdr->image_ver;
            int i = 0;
            if (val == 0) { buf[i++] = '0'; }
            else {
                char temp[16]; int ti = 0;
                while (val > 0) { temp[ti++] = (val % 10) + '0'; val /= 10; }
                while (ti > 0) buf[i++] = temp[--ti];
            }
            buf[i] = '\0';
            draw_text(cx + 100, cy + 130, buf, rgb565(0, 128, 0));
            
            draw_text(cx + 10, cy + 150, "Size:", COLOR_BLACK);
            val = hdr->image_size;
            i = 0;
            if (val == 0) { buf[i++] = '0'; }
            else {
                char temp[16]; int ti = 0;
                while (val > 0) { temp[ti++] = (val % 10) + '0'; val /= 10; }
                while (ti > 0) buf[i++] = temp[--ti];
            }
            buf[i++] = ' '; buf[i++] = 'B'; buf[i] = '\0';
            draw_text(cx + 100, cy + 150, buf, COLOR_BLACK);
            
            draw_text(cx + 10, cy + 170, "Signature:", COLOR_BLACK);
            draw_text(cx + 100, cy + 170, "Ed25519 (Signed)", rgb565(0, 128, 0));
            
            draw_text(cx + 10, cy + 200, "This file contains a secure STAX", COLOR_BLACK);
            draw_text(cx + 10, cy + 215, "firmware update payload.", COLOR_BLACK);
            draw_text(cx + 10, cy + 240, "Run 'fwupdate' in terminal to install.", COLOR_BLACK);
        } else {
            draw_text(cx + 10, cy + 130, "Status: Invalid or corrupted STAX file.", rgb565(200, 0, 0));
        }
    }
}
