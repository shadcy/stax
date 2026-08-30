/* ============================================================================
 * STAX — app_editor.c
 * VS Code Inspired Text Editor with Gutter, Action Pills & Ubuntu Mono
 * ============================================================================ */

#include "app_editor.h"
#include "wm.h"
#include "framebuffer.h"
#include "font.h"
#include "fatfs/ff.h"
#include "heap.h"
#include "string.h"

#define MAX_EDITOR_SIZE 4096
#define GUTTER_W        36

typedef struct {
    char text[MAX_EDITOR_SIZE];
    int cursor;
    int len;
    int scroll_y;
    int is_loaded;
    int cur_line;
    int cur_col;
} editor_state_t;

static void editor_save(struct window *win, editor_state_t *st) {
    if (!win->path[0]) strcpy(win->path, "UNTITLED.TXT");
    FIL f;
    if (f_open(&f, win->path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        UINT bw;
        f_write(&f, st->text, st->len, &bw);
        f_close(&f);
    }
}

void editor_mouse_click(struct window *win, int mx, int my, int button) {
    (void)button;
    editor_state_t *st = (editor_state_t *)win->app_data;
    if (!st) return;

    /* Check Top Action Pills */
    if (my >= 3 && my < 23) {
        if (mx >= win->width - 150 && mx < win->width - 105) {
            /* [New] */
            st->len = 0;
            st->cursor = 0;
            st->text[0] = '\0';
            win->path[0] = '\0';
        } else if (mx >= win->width - 100 && mx < win->width - 55) {
            /* [Save] */
            editor_save(win, st);
        } else if (mx >= win->width - 50 && mx < win->width - 10) {
            /* [Clear] */
            st->len = 0;
            st->cursor = 0;
            st->text[0] = '\0';
        }
    }
}

void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    
    if (!st) {
        st = (editor_state_t *)kmalloc(sizeof(editor_state_t));
        if (st) {
            st->cursor = 0;
            st->len = 0;
            st->scroll_y = 0;
            st->text[0] = '\0';
            st->is_loaded = 0;
            st->cur_line = 1;
            st->cur_col = 1;
            win->app_data = st;
            win->mouse_click = editor_mouse_click;
        } else {
            return;
        }
    }
    
    if (!st->is_loaded && win->path[0] != '\0') {
        FIL f;
        if (f_open(&f, win->path, FA_READ) == FR_OK) {
            UINT br;
            f_read(&f, st->text, MAX_EDITOR_SIZE - 1, &br);
            st->len = br;
            st->text[br] = '\0';
            st->cursor = st->len;
            f_close(&f);
        }
        st->is_loaded = 1;
    }
    
    /* Background (Midnight Slate VS Code theme) */
    fb_fillrect(cx, cy, cw, ch, rgb565(20, 22, 30));
    
    /* Top Toolbar */
    int bar_h = 26;
    fb_fillrect(cx, cy, cw, bar_h, rgb565(28, 30, 42));
    fb_drawline(cx, cy + bar_h - 1, cx + cw - 1, cy + bar_h - 1, rgb565(45, 50, 68));
    
    /* File Name badge */
    const char *doc_name = win->path[0] ? win->path : "Untitled.txt";
    font_draw_text(cx + 8, cy + 4, doc_name, theme_get_primary_accent(), FONT_STYLE_REGULAR);
    
    /* Action Pills */
    int new_x = cx + cw - 150;
    fb_fill_rounded_rect(new_x, cy + 3, 44, 20, 3, rgb565(40, 44, 60));
    font_draw_text(new_x + 8, cy + 5, "New", rgb565(210, 215, 230), FONT_STYLE_REGULAR);

    int save_x = cx + cw - 100;
    fb_fill_rounded_rect(save_x, cy + 3, 44, 20, 3, theme_get_primary_accent());
    font_draw_text(save_x + 6, cy + 5, "Save", COLOR_WHITE, FONT_STYLE_REGULAR);

    int clr_x = cx + cw - 50;
    fb_fill_rounded_rect(clr_x, cy + 3, 42, 20, 3, rgb565(180, 50, 50));
    font_draw_text(clr_x + 6, cy + 5, "Clear", COLOR_WHITE, FONT_STYLE_REGULAR);

    /* Left Line Numbers Gutter */
    int body_y = cy + bar_h;
    int body_h = ch - bar_h - 20;
    fb_fillrect(cx, body_y, GUTTER_W, body_h, rgb565(24, 26, 36));
    fb_drawline(cx + GUTTER_W - 1, body_y, cx + GUTTER_W - 1, body_y + body_h - 1, rgb565(42, 46, 62));

    /* Render text & gutter lines */
    int tx = cx + GUTTER_W + 6;
    int ty = body_y + 4 - (st->scroll_y * 16);
    int line_num = 1;
    
    int cx_pos = tx;
    int cy_pos = ty;
    
    st->cur_line = 1;
    st->cur_col = 1;
    int calc_line = 1;
    int calc_col = 1;

    /* Draw first line number */
    if (ty >= body_y && ty + 14 < body_y + body_h) {
        char lstr[8]; lstr[0] = '1'; lstr[1] = '\0';
        uint16_t num_col = (st->cur_line == 1) ? theme_get_primary_accent() : rgb565(90, 95, 120);
        font_draw_text(cx + 16, ty, lstr, num_col, FONT_STYLE_MONO);
    }

    int text_clip_r = cx + cw - 8;
    int text_clip_b = body_y + body_h;

    for (int i = 0; i <= st->len; i++) {
        if (i == st->cursor) {
            cx_pos = tx;
            cy_pos = ty;
            st->cur_line = calc_line;
            st->cur_col = calc_col;
        }
        
        if (i < st->len) {
            char c = st->text[i];
            if (c == '\n') {
                tx = cx + GUTTER_W + 6;
                ty += 16;
                calc_line++;
                calc_col = 1;
                line_num++;
                
                /* Draw line number in gutter */
                if (ty >= body_y && ty + 14 < text_clip_b && line_num < 100) {
                    char lstr[8];
                    if (line_num >= 10) {
                        lstr[0] = '0' + (line_num / 10);
                        lstr[1] = '0' + (line_num % 10);
                        lstr[2] = '\0';
                    } else {
                        lstr[0] = '0' + line_num;
                        lstr[1] = '\0';
                    }
                    uint16_t num_col = (line_num == st->cur_line) ? theme_get_primary_accent() : rgb565(90, 95, 120);
                    font_draw_text(cx + (line_num >= 10 ? 10 : 16), ty, lstr, num_col, FONT_STYLE_MONO);
                }
            } else if (c == '\r') {
                /* skip */
            } else {
                if (ty >= body_y && ty + 16 <= text_clip_b) {
                    font_draw_char_clipped(tx, ty, c, rgb565(225, 230, 245), FONT_STYLE_MONO,
                                           cx + GUTTER_W, body_y, text_clip_r, text_clip_b);
                }
                tx += 8;
                calc_col++;
                if (tx >= text_clip_r) {
                    tx = cx + GUTTER_W + 6;
                    ty += 16;
                }
            }
        }
    }
    
    /* Cursor block */
    if (cy_pos >= body_y && cy_pos + 16 <= text_clip_b) {
        fb_fillrect(cx_pos, cy_pos + 1, 8, 14, theme_get_primary_accent());
    }

    /* Bottom Status Bar */
    int sb_y = cy + ch - 20;
    fb_fillrect(cx, sb_y, cw, 20, rgb565(28, 30, 42));
    fb_drawline(cx, sb_y, cx + cw - 1, sb_y, rgb565(45, 50, 68));

    char status_str[64];
    /* Build "Ln X, Col Y" */
    char lbuf[8], cbuf[8];
    lbuf[0] = '0' + (st->cur_line / 10 % 10);
    lbuf[1] = '0' + (st->cur_line % 10);
    lbuf[2] = '\0';
    cbuf[0] = '0' + (st->cur_col / 10 % 10);
    cbuf[1] = '0' + (st->cur_col % 10);
    cbuf[2] = '\0';

    strcpy(status_str, "Ln ");
    strcat(status_str, (st->cur_line >= 10) ? lbuf : lbuf + 1);
    strcat(status_str, ", Col ");
    strcat(status_str, (st->cur_col >= 10) ? cbuf : cbuf + 1);
    strcat(status_str, "  |  UTF-8");

    font_draw_text(cx + 10, sb_y + 2, status_str, rgb565(140, 145, 165), FONT_STYLE_REGULAR);
    font_draw_text(cx + cw - 110, sb_y + 2, "STAX NanoEdit", theme_get_primary_accent(), FONT_STYLE_REGULAR);
}

void editor_key_event(struct window *win, char c) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    if (!st) return;
    
    /* Save: Ctrl+S (0x13) */
    if (c == 0x13) {
        editor_save(win, st);
        return;
    }
    
    /* Backspace */
    if (c == '\b' || c == 0x7F) {
        if (st->cursor > 0) {
            for (int i = st->cursor; i <= st->len; i++) {
                st->text[i - 1] = st->text[i];
            }
            st->cursor--;
            st->len--;
        }
        return;
    }
    
    /* Regular char */
    if (c >= 32 || c == '\n') {
        if (st->len < MAX_EDITOR_SIZE - 1) {
            for (int i = st->len; i > st->cursor; i--) {
                st->text[i] = st->text[i - 1];
            }
            st->text[st->cursor] = c;
            st->cursor++;
            st->len++;
            st->text[st->len] = '\0';
        }
    }
}

void editor_autosave(struct window *win) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    if (!st || !win->path[0] || st->len == 0) return;
    editor_save(win, st);
}
