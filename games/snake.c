/* ============================================================================
 * TIOS — snake.c
 * Graphical Snake game rendered directly via Framebuffer and 8x16 font.
 * ============================================================================ */

#include "snake.h"
#include "console.h"
#include "framebuffer.h"
#include "font8x16.h"
#include <stdint.h>

#define BW          38      /* playfield inner width  (columns: 1..38) */
#define BH          27      /* playfield inner height (rows: 2..28)    */
#define MAX_LEN     (BW * BH)

#define CELL_SIZE   16

extern volatile unsigned int tick_count;

/* --------------------------------------------------------------------------
 * Font rendering helper routines
 * -------------------------------------------------------------------------- */
static void draw_char(int x, int y, char c, uint16_t color)
{
    const unsigned char *g = font8x16_data[(unsigned char)c];
    uint16_t *fbuf = fb_get_buffer();
    for (int r = 0; r < 16; r++) {
        unsigned char bits = g[r];
        for (int b = 0; b < 8; b++) {
            if (bits & (0x80u >> b)) {
                fbuf[(y + r) * 640 + (x + b)] = color;
            } else {
                fbuf[(y + r) * 640 + (x + b)] = FB_BG;
            }
        }
    }
}

static void draw_string(int x, int y, const char *str, uint16_t color)
{
    while (*str) {
        draw_char(x, y, *str++, color);
        x += 8;
    }
}

static void draw_uint(int x, int y, unsigned int n, uint16_t color)
{
    char buf[12];
    int i = 0;
    if (n == 0) { draw_char(x, y, '0', color); return; }
    while (n) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    while (i-- > 0) {
        draw_char(x, y, buf[i], color);
        x += 8;
    }
}

/* --------------------------------------------------------------------------
 * Game state
 * -------------------------------------------------------------------------- */
typedef struct { signed char x; signed char y; } pt_t;

static pt_t body[MAX_LEN];
static int  g_head;
static int  g_len;
static int  g_dx, g_dy;
static int  g_ndx, g_ndy;
static pt_t g_food;
static int  g_score;
static int  g_alive;
static unsigned int g_move_ticks;
static unsigned int g_rng;

static unsigned int sn_rand(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static void place_food(void)
{
    int attempt;
    for (attempt = 0; attempt < 1000; attempt++) {
        int fx = 1 + (int)(sn_rand() % (unsigned int)BW);
        int fy = 2 + (int)(sn_rand() % (unsigned int)BH);
        int ok = 1, i;
        for (i = 0; i < g_len; i++) {
            int idx = (g_head - i + MAX_LEN) % MAX_LEN;
            if (body[idx].x == (signed char)fx &&
                body[idx].y == (signed char)fy) { ok = 0; break; }
        }
        if (ok) { g_food.x = (signed char)fx; g_food.y = (signed char)fy; return; }
    }
}

/* --------------------------------------------------------------------------
 * Graphical drawing routines
 * -------------------------------------------------------------------------- */
static void draw_score(void)
{
    /* Clear score area */
    fb_fillrect(0, 0, 640, 16, FB_BG);
    draw_string(16, 0, "T-OS GRAPHICAL SNAKE", COLOR_CYAN);
    draw_string(280, 0, "Score: ", COLOR_YELLOW);
    draw_uint(336, 0, (unsigned int)g_score, COLOR_GREEN);
    draw_string(450, 0, "WASD = Move | Q = Quit", COLOR_GRAY);
}

static void draw_cell(int cx, int cy, uint16_t color)
{
    fb_fillrect(cx * CELL_SIZE + 1, cy * CELL_SIZE + 1, CELL_SIZE - 2, CELL_SIZE - 2, color);
}

static void erase_cell(int cx, int cy)
{
    fb_fillrect(cx * CELL_SIZE, cy * CELL_SIZE, CELL_SIZE, CELL_SIZE, FB_BG);
}

static void draw_walls(void)
{
    /* Top wall */
    fb_fillrect(0, 16, 640, CELL_SIZE, COLOR_GRAY);
    /* Bottom wall */
    fb_fillrect(0, 29 * CELL_SIZE, 640, CELL_SIZE, COLOR_GRAY);
    /* Left wall */
    fb_fillrect(0, 16, CELL_SIZE, 480 - 16, COLOR_GRAY);
    /* Right wall */
    fb_fillrect(39 * CELL_SIZE, 16, CELL_SIZE, 480 - 16, COLOR_GRAY);
}

static void full_redraw(void)
{
    fb_clear(FB_BG);
    draw_walls();
    draw_score();

    /* Draw snake segments */
    for (int i = 0; i < g_len; i++) {
        int idx = (g_head - i + MAX_LEN) % MAX_LEN;
        draw_cell(body[idx].x, body[idx].y, (i == 0) ? COLOR_YELLOW : COLOR_GREEN);
    }

    /* Draw food */
    draw_cell(g_food.x, g_food.y, COLOR_RED);
}

static void game_init(void)
{
    int sx = 20;
    int sy = 15;

    g_len = 4;
    g_head = 3;
    g_dx = 1; g_dy = 0;
    g_ndx = 1; g_ndy = 0;
    g_score = 0;
    g_alive = 1;

    for (int i = 0; i < g_len; i++) {
        body[i].x = (signed char)(sx - (g_len - 1 - i));
        body[i].y = (signed char)sy;
    }

    g_rng = tick_count ^ 0xDEADBEEFU;
    if (g_rng == 0) g_rng = 1;
    g_move_ticks = 1; /* update every tick for smooth gameplay */
    place_food();
}

void snake_run(void)
{
    char c;
    unsigned int last_tick;

    /* Initialize display */
    fb_init();
    
    game_init();
    full_redraw();

    last_tick = tick_count;
    /* 100ms per frame for snake (10 FPS) */

    while (g_alive) {
        /* Input handling */
        c = kgetc();
        if (c) {
            if ((c == 'w' || c == 'W') && g_dy == 0) { g_ndx = 0; g_ndy = -1; }
            if ((c == 's' || c == 'S') && g_dy == 0) { g_ndx = 0; g_ndy = 1; }
            if ((c == 'a' || c == 'A') && g_dx == 0) { g_ndx = -1; g_ndy = 0; }
            if ((c == 'd' || c == 'D') && g_dx == 0) { g_ndx = 1; g_ndy = 0; }
            if (c == 'q' || c == 'Q') { g_alive = 0; break; }
        }

        /* Speed control — 1000 Hz timer: 80 ticks = 80ms/step (normal)
         *                                40 ticks = 40ms/step (fast) */
        unsigned int current = tick_count;
        unsigned int ticks_needed = 80;
        if (g_score >= 150) ticks_needed = 40;
        
        if (current - last_tick < ticks_needed) {
            asm volatile ("mcr p15, 0, %0, c7, c0, 4" : : "r" (0));
            continue;
        }
        last_tick = current;

        g_dx = g_ndx;
        g_dy = g_ndy;

        int nx = (int)body[g_head].x + g_dx;
        int ny = (int)body[g_head].y + g_dy;

        /* Wall collision */
        if (nx <= 0 || nx >= 39 || ny <= 1 || ny >= 29) {
            g_alive = 0;
            break;
        }

        /* Self collision */
        for (int i = 0; i < g_len - 1; i++) {
            int idx = (g_head - i + MAX_LEN) % MAX_LEN;
            if ((int)body[idx].x == nx && (int)body[idx].y == ny) {
                g_alive = 0;
                break;
            }
        }
        if (!g_alive) break;

        int ate = (nx == (int)g_food.x && ny == (int)g_food.y);
        
        int tail_idx = (g_head - g_len + 1 + MAX_LEN) % MAX_LEN;
        int old_tx = (int)body[tail_idx].x;
        int old_ty = (int)body[tail_idx].y;

        /* Advance head */
        g_head = (g_head + 1) % MAX_LEN;
        body[g_head].x = (signed char)nx;
        body[g_head].y = (signed char)ny;

        if (ate) {
            g_len++;
            g_score += 10;
            place_food();
            draw_cell(g_food.x, g_food.y, COLOR_RED);
            draw_score();
        } else {
            erase_cell(old_tx, old_ty);
        }

        /* Draw body segment that was head */
        int prev_idx = (g_head - 1 + MAX_LEN) % MAX_LEN;
        draw_cell(body[prev_idx].x, body[prev_idx].y, COLOR_GREEN);
        
        /* Draw new head */
        draw_cell(body[g_head].x, body[g_head].y, COLOR_YELLOW);
    }

    /* ---- Game Over Screen ---- */
    fb_fillrect(160, 160, 320, 160, COLOR_WHITE);
    fb_fillrect(162, 162, 316, 156, FB_BG);
    
    draw_string(260, 190, "GAME OVER!", COLOR_RED);
    draw_string(230, 220, "Final Score: ", COLOR_YELLOW);
    draw_uint(330, 220, (unsigned int)g_score, COLOR_GREEN);
    draw_string(190, 260, "Press Enter or Q to return...", COLOR_GRAY);

    /* Wait for Enter or Q */
    do {
        c = kgetc();
    } while (c != '\r' && c != '\n' && c != 'q' && c != 'Q');
}
