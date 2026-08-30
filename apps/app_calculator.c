/* ============================================================================
 * STAX — app_calculator.c
 * Advanced Clean UI Scientific & Desktop Calculator
 * ============================================================================ */

#include "app_calculator.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "font.h"

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

    /* Body Background (Dark Midnight Slate) */
    fb_fillrect(cx, cy, cw, ch, rgb565(22, 24, 32));

    /* LCD Screen Display Frame */
    int disp_x = cx + 10;
    int disp_y = cy + 8;
    int disp_w = cw - 20;
    int disp_h = 50;

    fb_fill_rounded_rect(disp_x, disp_y, disp_w, disp_h, 6, rgb565(12, 14, 20));
    fb_drawline(disp_x + 6, disp_y, disp_x + disp_w - 6, disp_y, theme_get_primary_accent());

    /* Memory indicator */
    if (st->has_memory) {
        font_draw_text(disp_x + 8, disp_y + 4, "M", rgb565(80, 240, 120), FONT_STYLE_REGULAR);
    }

    /* Sub expression (upper formula) */
    if (st->sub_buf[0]) {
        int sub_w = font_get_string_width(st->sub_buf, FONT_STYLE_REGULAR);
        font_draw_text(disp_x + disp_w - 10 - sub_w, disp_y + 4, st->sub_buf, rgb565(130, 140, 160), FONT_STYLE_REGULAR);
    }

    /* Main digits */
    int main_w = font_get_string_width(st->display_buf, FONT_STYLE_REGULAR);
    int main_x = disp_x + disp_w - 10 - main_w;
    if (main_x < disp_x + 8) main_x = disp_x + 8;
    font_draw_text_clipped(main_x, disp_y + 24, st->display_buf, 
                           st->has_error ? rgb565(255, 80, 80) : COLOR_WHITE, 
                           FONT_STYLE_REGULAR, disp_x + 6, disp_y + 20, disp_x + disp_w - 6, disp_y + disp_h);

    /* 5 Columns x 6 Rows Keypad */
    const char *btn_labels[6][5] = {
        {"MC", "MR", "M+", "M-", "AC"},
        {"x²", "√x", "1/x","%",  "÷"},
        {"7",  "8",  "9",  "DEL","×"},
        {"4",  "5",  "6",  "+/-","−"},
        {"1",  "2",  "3",  "00", "+"},
        {"0",  ".",  "=",  "=",  "="}
    };

    int start_y = cy + 64;
    int btn_h = 32;
    int gap = 5;
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

            uint16_t bg = rgb565(44, 48, 62);   /* Standard number key */
            uint16_t fg = COLOR_WHITE;

            /* Operator styling */
            if (strcmp(label, "÷") == 0 || strcmp(label, "×") == 0 ||
                strcmp(label, "−") == 0 || strcmp(label, "+") == 0) {
                bg = theme_get_primary_accent();
            } else if (strcmp(label, "=") == 0) {
                bg = theme_get_secondary_accent();
            } else if (strcmp(label, "AC") == 0) {
                bg = rgb565(195, 45, 55);  /* Crimson */
            } else if (strcmp(label, "MC") == 0 || strcmp(label, "MR") == 0 ||
                       strcmp(label, "M+") == 0 || strcmp(label, "M-") == 0 ||
                       strcmp(label, "x²") == 0 || strcmp(label, "√x") == 0 ||
                       strcmp(label, "1/x") == 0 || strcmp(label, "%") == 0 ||
                       strcmp(label, "DEL") == 0 || strcmp(label, "+/-") == 0) {
                bg = rgb565(60, 65, 82);   /* Function Slate */
            }

            /* Draw tactile rounded button */
            fb_fill_rounded_rect(bx, by, bw, btn_h, 4, bg);
            fb_drawline(bx + 2, by, bx + bw - 3, by, rgb565(80, 85, 105));

            int tw = font_get_string_width(label, FONT_STYLE_REGULAR);
            int tx = bx + (bw - tw) / 2;
            int ty = by + (btn_h - 16) / 2;
            font_draw_text(tx, ty, label, fg, FONT_STYLE_REGULAR);
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

    int disp_x = 8;
    int disp_w = win->width - 16;
    int start_y = 60;
    int btn_h = 32;
    int gap = 4;
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
