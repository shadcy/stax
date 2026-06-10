#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "console.h"
#include "font8x16.h"

typedef struct {
    int current_val;
    int saved_val;
    char op;
    int new_input;
} calc_state_t;

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    char temp[16];
    int i = 0;
    while (val > 0) {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    }
    if (is_neg) temp[i++] = '-';
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)cw;
    (void)ch;
    
    if (!win->app_data) {
        win->app_data = kmalloc(sizeof(calc_state_t));
        calc_state_t *st = (calc_state_t*)win->app_data;
        if (st) {
            st->current_val = 0;
            st->saved_val = 0;
            st->op = 0;
            st->new_input = 1;
        }
    }
    calc_state_t *st = (calc_state_t*)win->app_data;
    if (!st) return;
    
    fb_fillrect(cx, cy, cw, ch, rgb565(200, 200, 200));
    
    /* Display */
    fb_fillrect(cx + 10, cy + 10, 180, 40, COLOR_WHITE);
    fb_drawline(cx + 10, cy + 10, cx + 190, cy + 10, COLOR_BLACK);
    fb_drawline(cx + 10, cy + 10, cx + 10, cy + 50, COLOR_BLACK);
    
    char buf[32];
    int_to_str(st->current_val, buf);
    draw_text(cx + 160 - strlen(buf)*8, cy + 22, buf, COLOR_BLACK);
    
    /* Buttons */
    const char *btns[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + 10 + c * 45;
            int by = cy + 60 + r * 45;
            fb_fillrect(bx, by, 40, 40, rgb565(180, 180, 180));
            fb_drawline(bx, by, bx+40, by, COLOR_WHITE);
            fb_drawline(bx, by, bx, by+40, COLOR_WHITE);
            fb_drawline(bx+40, by, bx+40, by+40, rgb565(100, 100, 100));
            fb_drawline(bx, by+40, bx+40, by+40, rgb565(100, 100, 100));
            draw_text(bx + 16, by + 12, btns[r*4 + c], COLOR_BLACK);
        }
    }
}

void calculator_mouse_click(struct window *win, int mx, int my, int button) {
    if (button != 1) return;
    calc_state_t *st = (calc_state_t*)win->app_data;
    if (!st) return;
    
    const char *btns[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = 10 + c * 45;
            int by = 60 + r * 45;
            if (mx >= bx && mx < bx + 40 && my >= by && my < by + 40) {
                const char *btn = btns[r*4 + c];
                char b = btn[0];
                if (b >= '0' && b <= '9') {
                    if (st->new_input) {
                        st->current_val = b - '0';
                        st->new_input = 0;
                    } else {
                        st->current_val = st->current_val * 10 + (b - '0');
                    }
                } else if (b == 'C') {
                    st->current_val = 0;
                    st->saved_val = 0;
                    st->op = 0;
                    st->new_input = 1;
                } else if (b == '=') {
                    if (st->op == '+') st->current_val = st->saved_val + st->current_val;
                    else if (st->op == '-') st->current_val = st->saved_val - st->current_val;
                    else if (st->op == '*') st->current_val = st->saved_val * st->current_val;
                    else if (st->op == '/') {
                        if (st->current_val != 0) st->current_val = st->saved_val / st->current_val;
                        else st->current_val = 0;
                    }
                    st->op = 0;
                    st->new_input = 1;
                } else {
                    /* Operator */
                    if (st->op) {
                        /* evaluate previous */
                        if (st->op == '+') st->saved_val = st->saved_val + st->current_val;
                        else if (st->op == '-') st->saved_val = st->saved_val - st->current_val;
                        else if (st->op == '*') st->saved_val = st->saved_val * st->current_val;
                        else if (st->op == '/') {
                            if (st->current_val != 0) st->saved_val = st->saved_val / st->current_val;
                        }
                        st->current_val = st->saved_val;
                    } else {
                        st->saved_val = st->current_val;
                    }
                    st->op = b;
                    st->new_input = 1;
                }
            }
        }
    }
}
