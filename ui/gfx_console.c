/* ============================================================================
 * STAX — gfx_console.c
 * Text console buffer for Window Manager
 * ============================================================================ */

#include "gfx_console.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "wm.h"

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

    int max_scroll = head_line - (max_rows - 1);
    if (max_scroll < 0) max_scroll = 0;
    
    if (view_offset > max_scroll) view_offset = max_scroll;

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
    
    /* Draw scrollbar if needed */
    if (head_line >= max_rows) {
        int track_x = cx + cw - 16;
        int track_h = ch;
        int total_rows = head_line + 1;
        int max_scroll = head_line - (max_rows - 1);
        
        int thumb_h = (max_rows * track_h) / total_rows;
        if (thumb_h < 10) thumb_h = 10;
        
        int scroll_pos = max_scroll - view_offset; /* 0 at top, max_scroll at bottom */
        int thumb_y = cy;
        if (max_scroll > 0) {
            thumb_y = cy + (scroll_pos * (track_h - thumb_h)) / max_scroll;
        }
        
        /* Track */
        fb_fillrect(track_x, cy, 16, track_h, rgb565(40, 40, 40));
        /* Thumb */
        fb_fillrect(track_x + 2, thumb_y + 2, 12, thumb_h - 4, rgb565(120, 120, 120));
    }
}

void gfx_console_key_event(struct window *win, char c)
{
    (void)win;
    if (c == 0x11) { /* KB_UP */
        gfx_scroll(1);
    } else if (c == 0x12) { /* KB_DOWN */
        gfx_scroll(-1);
    }
}

void gfx_console_mouse_click(struct window *win, int mx, int my, int btn)
{
    (void)btn;
    if (!enabled) return;
    
    int cw = win->width;
    int ch = win->height;
    
    if (mx >= cw - 16) {
        int max_rows = ch / 16;
        if (head_line >= max_rows) {
            int track_h = ch;
            int total_rows = head_line + 1;
            int max_scroll = head_line - (max_rows - 1);
            if (max_scroll < 0) max_scroll = 0;
            
            int thumb_h = (max_rows * track_h) / total_rows;
            if (thumb_h < 10) thumb_h = 10;
            
            int scroll_pos = max_scroll - view_offset;
            int thumb_y = 0;
            if (max_scroll > 0) {
                thumb_y = (scroll_pos * (track_h - thumb_h)) / max_scroll;
            }
            
            if (my < thumb_y) {
                gfx_scroll(max_rows); /* Page up */
            } else if (my > thumb_y + thumb_h) {
                gfx_scroll(-max_rows); /* Page down */
            }
        }
    }
}

void gfx_console_mouse_drag(struct window *win, int mx, int my)
{
    if (!enabled) return;
    
    int cw = win->width;
    int ch = win->height;
    
    if (mx >= cw - 24) { /* Allow slight leeway */
        int max_rows = ch / 16;
        if (head_line >= max_rows) {
            int track_h = ch;
            int total_rows = head_line + 1;
            int max_scroll = head_line - (max_rows - 1);
            if (max_scroll < 0) max_scroll = 0;
            
            int thumb_h = (max_rows * track_h) / total_rows;
            if (thumb_h < 10) thumb_h = 10;
            
            int track_range = track_h - thumb_h;
            if (track_range <= 0) return;
            
            int drag_y = my - (thumb_h / 2);
            if (drag_y < 0) drag_y = 0;
            if (drag_y > track_range) drag_y = track_range;
            
            int new_scroll_pos = (drag_y * max_scroll) / track_range;
            view_offset = max_scroll - new_scroll_pos;
            if (view_offset < 0) view_offset = 0;
            if (view_offset > max_scroll) view_offset = max_scroll;
        }
    }
}
