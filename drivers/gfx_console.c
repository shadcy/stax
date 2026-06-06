/* ============================================================================
 * TIOS — gfx_console.c
 * Text console buffer for Window Manager
 * ============================================================================ */

#include "gfx_console.h"
#include "framebuffer.h"
#include "font8x16.h"

#define COLS   80
#define ROWS   30
#define MAX_LINES 256

static int      cur_x    = 0;
static int      enabled  = 0;
static uint16_t fg       = COLOR_WHITE;

static int      cur_on   = 1;
static int      blink_n  = 0;
#define BLINK_DIV  50

static char term_text[MAX_LINES][COLS];
static uint16_t term_color[MAX_LINES][COLS];
static int head_line = 0;
static int view_offset = 0;

int gfx_console_init(void)
{
    if (fb_init() != 0) return -1;
    cur_x = 0;
    head_line = 0;
    view_offset = 0;
    blink_n = 0; cur_on = 1;
    enabled = 1;
    fg = COLOR_WHITE;
    
    for (int i=0; i<MAX_LINES; i++) {
        for (int j=0; j<COLS; j++) {
            term_text[i][j] = 0;
            term_color[i][j] = COLOR_WHITE;
        }
    }
    return 0;
}

void gfx_set_color(uint16_t color) { fg = color; }

void gfx_tick(void)
{
    if (!enabled) return;
    if (++blink_n >= BLINK_DIV) {
        blink_n = 0;
        cur_on  = !cur_on;
    }
}

static void advance_line(void)
{
    head_line++;
    cur_x = 0;
    for (int i=0; i<COLS; i++) {
        term_text[head_line % MAX_LINES][i] = 0;
        term_color[head_line % MAX_LINES][i] = fg;
    }
    if (view_offset > 0) view_offset++;
    if (view_offset >= head_line) view_offset = head_line;
}

void gfx_putc(char c)
{
    static int in_escape = 0;

    if (in_escape) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            in_escape = 0;
        }
        return;
    }

    if (c == '\x1b') {
        in_escape = 1;
        return;
    }

    if (view_offset > 0) {
        view_offset = 0;
    }

    if (!enabled) return;

    if (c == '\n') {
        advance_line();
    } else if (c == '\r') {
        cur_x = 0;
    } else if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
            term_text[head_line % MAX_LINES][cur_x] = 0;
        }
    } else if ((unsigned char)c >= 32) {
        term_text[head_line % MAX_LINES][cur_x] = c;
        term_color[head_line % MAX_LINES][cur_x] = fg;
        
        if (++cur_x >= COLS) {
            advance_line();
        }
    }
    cur_on = 1; blink_n = 0;
}

void gfx_puts(const char *s)
{
    if (!enabled) return;
    while (*s) gfx_putc(*s++);
}

void gfx_puts_color(const char *s, uint16_t color)
{
    uint16_t saved = fg; fg = color;
    gfx_puts(s);
    fg = saved;
}

void gfx_clear(void)
{
    if (!enabled) return;
    head_line = 0;
    cur_x = 0;
    view_offset = 0;
    for (int i=0; i<MAX_LINES; i++) {
        for (int j=0; j<COLS; j++) {
            term_text[i][j] = 0;
            term_color[i][j] = COLOR_WHITE;
        }
    }
}

void gfx_console_enable(int e) { enabled = e; }

void gfx_scroll(int lines)
{
    if (!enabled) return;
    view_offset += lines;
    if (view_offset < 0) view_offset = 0;
    
    int max_scroll = head_line;
    if (max_scroll < 0) max_scroll = 0;
    if (view_offset > max_scroll) view_offset = max_scroll;
}

/* WM Callback for rendering */
void gfx_console_draw_window(struct window *win, int cx, int cy, int cw, int ch)
{
    (void)win;
    if (!enabled) return;
    
    int max_cols = cw / 8;
    int max_rows = ch / 16;
    if (max_cols > COLS) max_cols = COLS;
    if (max_rows > ROWS) max_rows = ROWS;

    int start_line;
    if (head_line < max_rows) {
        start_line = 0;
    } else {
        start_line = head_line - (max_rows - 1) - view_offset;
        if (start_line < 0) start_line = 0;
    }

    uint16_t *fbuf = fb_get_buffer();
    
    for (int r = 0; r < max_rows; r++) {
        int buf_line = start_line + r;
        if (buf_line < 0 || buf_line > head_line) continue;
        
        for (int c = 0; c < max_cols; c++) {
            char ch_val = term_text[buf_line % MAX_LINES][c];
            if (ch_val >= 32) {
                uint16_t color = term_color[buf_line % MAX_LINES][c];
                const unsigned char *g = font8x16_data[(unsigned char)ch_val];
                int px = cx + c * 8;
                int py = cy + r * 16;
                for (int gr = 0; gr < 16; gr++) {
                    unsigned char bits = g[gr];
                    for (int gb = 0; gb < 8; gb++) {
                        if (bits & (0x80 >> gb)) {
                            if (py+gr >= 0 && py+gr < FB_HEIGHT && px+gb >= 0 && px+gb < FB_WIDTH) {
                                fbuf[(py+gr)*FB_WIDTH + (px+gb)] = color;
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Draw cursor */
    if (view_offset == 0 && cur_on) {
        int cursor_row = (head_line < max_rows) ? head_line : (max_rows - 1);
        int px = cx + cur_x * 8;
        int py = cy + cursor_row * 16;
        if (px + 8 <= cx + cw && py + 16 <= cy + ch) {
            fb_fillrect(px, py, 8, 16, COLOR_GRAY_5);
        }
    }
}
