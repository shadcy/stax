/* ============================================================================
 * STAX — app_calculator.c
 * Advanced Clean UI Scientific & Desktop Calculator
 * ============================================================================ */

#include "app_calculator.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "font8x16.h"

/* Fixed-point math representation (scale factor 10000 = 4 decimal places) */
#define CALC_SCALE 10000

typedef struct {
    int64_t current_val;    /* In units of 1/CALC_SCALE */
    int64_t saved_val;      /* Stored operand */
    int64_t memory_val;     /* Memory register (MR/M+/M-) */
    char    op;             /* '+', '-', '*', '/', '%' */
    char    display_buf[32];/* Formatted display text */
    char    sub_buf[32];    /* Upper expression text */
    int     has_decimal;    /* Decimal place active */
    int     dec_div;        /* Divisor for decimals typed */
    int     new_input;      /* Flag for next number entry */
    int     has_memory;     /* Memory indicator active */
    int     has_error;      /* Division by zero or overflow */
} calc_state_t;

/* Format fixed-point value into clean human-readable string */
static void format_val(int64_t val, char *buf) {
    if (val == 0) {
        strcpy(buf, "0");
        return;
    }
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }

    int64_t integer_part = val / CALC_SCALE;
    int64_t frac_part    = val % CALC_SCALE;

    char int_str[24];
    int idx = 0;
    if (integer_part == 0) {
        int_str[idx++] = '0';
    } else {
        char tmp[24];
        int t = 0;
        while (integer_part > 0) {
            tmp[t++] = '0' + (integer_part % 10);
            integer_part /= 10;
        }
        while (t > 0) int_str[idx++] = tmp[--t];
    }
    int_str[idx] = '\0';

    int pos = 0;
    if (is_neg) buf[pos++] = '-';
    for (int k = 0; int_str[k]; k++) buf[pos++] = int_str[k];

    /* Trim trailing decimal zeroes */
    if (frac_part > 0) {
        buf[pos++] = '.';
        char frac_str[8];
        frac_str[0] = '0' + (frac_part / 1000);
        frac_str[1] = '0' + ((frac_part / 100) % 10);
        frac_str[2] = '0' + ((frac_part / 10) % 10);
        frac_str[3] = '0' + (frac_part % 10);
        frac_str[4] = '\0';
        int end_f = 3;
        while (end_f > 0 && frac_str[end_f] == '0') end_f--;
        for (int k = 0; k <= end_f; k++) buf[pos++] = frac_str[k];
    }
    buf[pos] = '\0';
}

/* Integer square root for fixed point */
static int64_t isqrt64(int64_t n) {
    if (n <= 0) return 0;
    int64_t x0 = n / 2;
    if (x0 == 0) return 1;
    int64_t x1 = (x0 + n / x0) / 2;
    while (x1 < x0) {
        x0 = x1;
        x1 = (x0 + n / x0) / 2;
    }
    return x0;
}

static void calc_update_display(calc_state_t *st) {
    if (st->has_error) {
        strcpy(st->display_buf, "Error: Div by 0");
        return;
    }
    format_val(st->current_val, st->display_buf);
}

static void calc_init_state(calc_state_t *st) {
    st->current_val = 0;
    st->saved_val = 0;
    st->memory_val = 0;
    st->op = 0;
    st->has_decimal = 0;
    st->dec_div = 10;
    st->new_input = 1;
    st->has_memory = 0;
    st->has_error = 0;
    strcpy(st->display_buf, "0");
    st->sub_buf[0] = '\0';
}

static void calc_do_op(calc_state_t *st, char next_op) {
    if (st->has_error) {
        calc_init_state(st);
        return;
    }

    if (st->op) {
        /* Evaluate existing operation */
        if (st->op == '+') {
            st->saved_val = st->saved_val + st->current_val;
        } else if (st->op == '-') {
            st->saved_val = st->saved_val - st->current_val;
        } else if (st->op == '*') {
            st->saved_val = (st->saved_val * st->current_val) / CALC_SCALE;
        } else if (st->op == '/') {
            if (st->current_val == 0) {
                st->has_error = 1;
            } else {
                st->saved_val = (st->saved_val * CALC_SCALE) / st->current_val;
            }
        } else if (st->op == '%') {
            if (st->current_val != 0) {
                st->saved_val = st->saved_val % st->current_val;
            }
        }
        st->current_val = st->saved_val;
    } else {
        st->saved_val = st->current_val;
    }

    if (!st->has_error) {
        if (next_op == '=') {
            char val_str[32];
            format_val(st->saved_val, val_str);
            strcpy(st->sub_buf, "Ans = ");
            strcat(st->sub_buf, val_str);
            st->op = 0;
        } else {
            char val_str[32];
            format_val(st->saved_val, val_str);
            strcpy(st->sub_buf, val_str);
            int sl = (int)strlen(st->sub_buf);
            st->sub_buf[sl++] = ' ';
            st->sub_buf[sl++] = next_op;
            st->sub_buf[sl] = '\0';
            st->op = next_op;
        }
    }

    st->new_input = 1;
    st->has_decimal = 0;
    st->dec_div = 10;
    calc_update_display(st);
}

void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)cw; (void)ch;
    if (!win->app_data) {
        win->app_data = kmalloc(sizeof(calc_state_t));
        calc_state_t *st = (calc_state_t *)win->app_data;
        if (st) calc_init_state(st);
    }
    calc_state_t *st = (calc_state_t *)win->app_data;
    if (!st) return;

    /* Body Background (Dark Slate) */
    fb_fillrect(cx, cy, cw, ch, rgb565(30, 32, 40));

    /* LCD Screen Display Frame */
    int disp_x = cx + 10;
    int disp_y = cy + 10;
    int disp_w = cw - 20;
    int disp_h = 56;

    fb_fillrect(disp_x, disp_y, disp_w, disp_h, rgb565(16, 18, 24));
    fb_drawline(disp_x, disp_y, disp_x + disp_w - 1, disp_y, rgb565(10, 12, 16));
    fb_drawline(disp_x, disp_y, disp_x, disp_y + disp_h - 1, rgb565(10, 12, 16));
    fb_drawline(disp_x + disp_w - 1, disp_y, disp_x + disp_w - 1, disp_y + disp_h - 1, rgb565(45, 48, 60));
    fb_drawline(disp_x, disp_y + disp_h - 1, disp_x + disp_w - 1, disp_y + disp_h - 1, rgb565(45, 48, 60));

    /* Memory indicator */
    if (st->has_memory) {
        draw_text(disp_x + 8, disp_y + 6, "M", rgb565(80, 240, 120));
    }

    /* Sub expression (upper formula) */
    if (st->sub_buf[0]) {
        int sub_len = (int)strlen(st->sub_buf);
        draw_text(disp_x + disp_w - 10 - sub_len * 8, disp_y + 6, st->sub_buf, rgb565(140, 150, 170));
    }

    /* Main digits */
    int main_len = (int)strlen(st->display_buf);
    int main_x = disp_x + disp_w - 10 - main_len * 8;
    if (main_x < disp_x + 8) main_x = disp_x + 8;
    draw_text(main_x, disp_y + 30, st->display_buf, st->has_error ? rgb565(255, 80, 80) : COLOR_WHITE);

    /* 5 Columns x 6 Rows Keypad */
    const char *btn_labels[6][5] = {
        {"MC", "MR", "M+", "M-", "AC"},
        {"x²", "√x", "1/x","%",  "÷"},
        {"7",  "8",  "9",  "DEL","×"},
        {"4",  "5",  "6",  "+/-","−"},
        {"1",  "2",  "3",  "00", "+"},
        {"0",  ".",  "=",  "=",  "="}
    };

    int start_y = cy + 74;
    int btn_h = 38;
    int gap = 6;
    int btn_w = (disp_w - gap * 4) / 5;

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 5; c++) {
            const char *label = btn_labels[r][c];
            if (r == 5 && c > 2) continue; /* Merged '=' button */

            int bx = disp_x + c * (btn_w + gap);
            int by = start_y + r * (btn_h + gap);
            int bw = btn_w;
            if (r == 5 && c == 2) {
                bw = btn_w * 3 + gap * 2; /* Wide '=' button */
            }

            uint16_t bg = rgb565(55, 58, 70);   /* Standard number key */
            uint16_t fg = COLOR_WHITE;

            /* Operator styling */
            if (strcmp(label, "÷") == 0 || strcmp(label, "×") == 0 ||
                strcmp(label, "−") == 0 || strcmp(label, "+") == 0) {
                bg = rgb565(40, 115, 225); /* Vibrant Blue */
            } else if (strcmp(label, "=") == 0) {
                bg = rgb565(245, 130, 20); /* Safety Orange */
            } else if (strcmp(label, "AC") == 0) {
                bg = rgb565(190, 45, 45);  /* Crimson */
            } else if (strcmp(label, "MC") == 0 || strcmp(label, "MR") == 0 ||
                       strcmp(label, "M+") == 0 || strcmp(label, "M-") == 0 ||
                       strcmp(label, "x²") == 0 || strcmp(label, "√x") == 0 ||
                       strcmp(label, "1/x") == 0 || strcmp(label, "%") == 0 ||
                       strcmp(label, "DEL") == 0 || strcmp(label, "+/-") == 0) {
                bg = rgb565(75, 80, 95);   /* Slate */
            }

            /* Draw button frame */
            fb_fillrect(bx, by, bw, btn_h, bg);
            fb_drawline(bx, by, bx + bw - 1, by, rgb565(255, 255, 255));
            fb_drawline(bx, by, bx, by + btn_h - 1, rgb565(255, 255, 255));
            fb_drawline(bx + bw - 1, by, bx + bw - 1, by + btn_h - 1, rgb565(20, 22, 30));
            fb_drawline(bx, by + btn_h - 1, bx + bw - 1, by + btn_h - 1, rgb565(20, 22, 30));

            int tlen = (int)strlen(label);
            int tx = bx + (bw - tlen * 8) / 2;
            int ty = by + (btn_h - 16) / 2;
            draw_text(tx, ty, label, fg);
        }
    }
}

static void calc_handle_action(calc_state_t *st, const char *btn) {
    if (strcmp(btn, "0") >= 0 && strcmp(btn, "9") <= 0 && strlen(btn) == 1) {
        int d = btn[0] - '0';
        if (st->new_input) {
            st->current_val = d * CALC_SCALE;
            st->new_input = 0;
            st->has_decimal = 0;
            st->dec_div = 10;
        } else {
            if (st->has_decimal) {
                if (st->dec_div <= CALC_SCALE) {
                    st->current_val += (d * CALC_SCALE) / st->dec_div;
                    st->dec_div *= 10;
                }
            } else {
                st->current_val = st->current_val * 10 + (d * CALC_SCALE);
            }
        }
        calc_update_display(st);
    } else if (strcmp(btn, "00") == 0) {
        if (!st->new_input && !st->has_decimal) {
            st->current_val = st->current_val * 100;
            calc_update_display(st);
        }
    } else if (strcmp(btn, ".") == 0) {
        if (st->new_input) {
            st->current_val = 0;
            st->new_input = 0;
        }
        st->has_decimal = 1;
        st->dec_div = 10;
    } else if (strcmp(btn, "AC") == 0) {
        calc_init_state(st);
    } else if (strcmp(btn, "DEL") == 0) {
        if (!st->new_input) {
            st->current_val = (st->current_val / (CALC_SCALE * 10)) * CALC_SCALE;
            calc_update_display(st);
        }
    } else if (strcmp(btn, "+/-") == 0) {
        st->current_val = -st->current_val;
        calc_update_display(st);
    } else if (strcmp(btn, "+") == 0) {
        calc_do_op(st, '+');
    } else if (strcmp(btn, "−") == 0 || strcmp(btn, "-") == 0) {
        calc_do_op(st, '-');
    } else if (strcmp(btn, "×") == 0 || strcmp(btn, "*") == 0) {
        calc_do_op(st, '*');
    } else if (strcmp(btn, "÷") == 0 || strcmp(btn, "/") == 0) {
        calc_do_op(st, '/');
    } else if (strcmp(btn, "%") == 0) {
        st->current_val = st->current_val / 100;
        calc_update_display(st);
    } else if (strcmp(btn, "x²") == 0) {
        st->current_val = (st->current_val * st->current_val) / CALC_SCALE;
        calc_update_display(st);
    } else if (strcmp(btn, "√x") == 0) {
        if (st->current_val < 0) {
            st->has_error = 1;
        } else {
            st->current_val = isqrt64(st->current_val * CALC_SCALE);
        }
        calc_update_display(st);
    } else if (strcmp(btn, "1/x") == 0) {
        if (st->current_val == 0) {
            st->has_error = 1;
        } else {
            st->current_val = (CALC_SCALE * CALC_SCALE) / st->current_val;
        }
        calc_update_display(st);
    } else if (strcmp(btn, "=") == 0) {
        calc_do_op(st, '=');
    } else if (strcmp(btn, "MC") == 0) {
        st->memory_val = 0;
        st->has_memory = 0;
    } else if (strcmp(btn, "MR") == 0) {
        st->current_val = st->memory_val;
        st->new_input = 1;
        calc_update_display(st);
    } else if (strcmp(btn, "M+") == 0) {
        st->memory_val += st->current_val;
        st->has_memory = 1;
        st->new_input = 1;
    } else if (strcmp(btn, "M-") == 0) {
        st->memory_val -= st->current_val;
        st->has_memory = 1;
        st->new_input = 1;
    }
}

void calculator_mouse_click(struct window *win, int mx, int my, int button) {
    if (button != 1) return;
    calc_state_t *st = (calc_state_t *)win->app_data;
    if (!st) return;

    int disp_x = 10;
    int disp_w = win->width - 20;
    int start_y = 74;
    int btn_h = 38;
    int gap = 6;
    int btn_w = (disp_w - gap * 4) / 5;

    const char *btn_labels[6][5] = {
        {"MC", "MR", "M+", "M-", "AC"},
        {"x²", "√x", "1/x","%",  "÷"},
        {"7",  "8",  "9",  "DEL","×"},
        {"4",  "5",  "6",  "+/-","−"},
        {"1",  "2",  "3",  "00", "+"},
        {"0",  ".",  "=",  "=",  "="}
    };

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 5; c++) {
            if (r == 5 && c > 2) continue;

            int bx = disp_x + c * (btn_w + gap);
            int by = start_y + r * (btn_h + gap);
            int bw = btn_w;
            if (r == 5 && c == 2) {
                bw = btn_w * 3 + gap * 2;
            }

            if (mx >= bx && mx < bx + bw && my >= by && my < by + btn_h) {
                calc_handle_action(st, btn_labels[r][c]);
                return;
            }
        }
    }
}

void calculator_key_event(struct window *win, char c) {
    calc_state_t *st = (calc_state_t *)win->app_data;
    if (!st) return;

    if (c >= '0' && c <= '9') {
        char b[2] = {c, '\0'};
        calc_handle_action(st, b);
    } else if (c == '.') {
        calc_handle_action(st, ".");
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        char b[2] = {c, '\0'};
        calc_handle_action(st, b);
    } else if (c == '=' || c == '\r' || c == '\n') {
        calc_handle_action(st, "=");
    } else if (c == '\b' || c == 0x7F) {
        calc_handle_action(st, "DEL");
    } else if (c == 'c' || c == 'C') {
        calc_handle_action(st, "AC");
    } else if (c == '%') {
        calc_handle_action(st, "%");
    }
}
