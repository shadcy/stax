/* ============================================================================
 * TIOS — snake.c
 * Terminal Snake game rendered via ANSI escape codes over the PL011 UART.
 *
 * Rendering strategy:
 *   - Board: 38-wide x 18-tall inner playfield surrounded by a border
 *   - Every cell is one character in the terminal
 *   - Only changed cells are redrawn each frame (no full-screen clear per tick)
 *   - ANSI cursor positioning: ESC[row;colH
 *   - Colors via ANSI SGR codes
 *
 * Input:
 *   - W/A/S/D or Arrow keys to steer
 *   - Q or Ctrl-C to quit
 *
 * Timing:
 *   - tick_count increments at 10 Hz (every 100 ms) from the SP804 timer ISR
 *   - Snake starts at 1 tick/step (100 ms = 10 Hz); speeds up every 50 pts
 *
 * Memory:
 *   - All state is static (no kmalloc) — ~1.5 KB of BSS
 * ============================================================================ */

#include "snake.h"
#include "console.h"
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Board geometry
 * -------------------------------------------------------------------------- */
#define BW          38      /* playfield inner width  (columns)   */
#define BH          18      /* playfield inner height (rows)      */
#define MAX_LEN     (BW * BH)   /* absolute maximum snake length  */

/* Screen offsets: border is drawn at screen row/col 1-indexed
 *
 *  Row 1          : score bar
 *  Row 2          : top border      +---------+
 *  Rows 3..BH+2   : side borders    | field   |
 *  Row BH+3       : bottom border   +---------+
 *  Row BH+4       : hint line
 *
 * A playfield cell (px, py) [0-indexed] maps to screen (col, row):
 *   col = px + 2
 *   row = py + 3
 */
#define SCR_COL(px)  ((px) + 2)
#define SCR_ROW(py)  ((py) + 3)

/* --------------------------------------------------------------------------
 * Timing
 * -------------------------------------------------------------------------- */
#define MOVE_TICKS_START  1   /* ticks between steps at start (10 Hz = 100 ms) */
#define MOVE_TICKS_MIN    1   /* fastest possible — 1 tick = 100 ms            */
/* Speed-up: every 50 pts shave one tick off move interval (floor = MIN)      */

/* --------------------------------------------------------------------------
 * External kernel state
 * -------------------------------------------------------------------------- */
extern volatile unsigned int tick_count;

/* --------------------------------------------------------------------------
 * ANSI helpers  (all inlined so the game has zero external dependencies
 *                beyond kputc / kputs / kgetc from console.c)
 * -------------------------------------------------------------------------- */
static void sn_uint(unsigned int n)
{
    char buf[12];
    int  i = 0;
    if (n == 0) { kputc('0'); return; }
    while (n) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    while (i-- > 0) kputc(buf[i]);
}

/* Move terminal cursor to 1-indexed (col, row) */
static void sn_goto(int col, int row)
{
    kputs("\033[");
    sn_uint((unsigned int)row);
    kputc(';');
    sn_uint((unsigned int)col);
    kputc('H');
}

/* Draw a single character cell with an ANSI colour prefix */
static void sn_cell(int px, int py, const char *colour, char ch)
{
    sn_goto(SCR_COL(px), SCR_ROW(py));
    kputs(colour);
    kputc(ch);
    kputs("\033[0m");
}

/* --------------------------------------------------------------------------
 * Game state  (all static — lives in .bss, no heap allocation)
 * -------------------------------------------------------------------------- */
typedef struct { signed char x; signed char y; } pt_t;

/* Ring buffer: body[head] is the snake head, body[(head-1+MAX_LEN)%MAX_LEN]
 * is the next segment, …, body[(head-len+1+MAX_LEN)%MAX_LEN] is the tail. */
static pt_t body[MAX_LEN];
static int  g_head;     /* ring-buffer head index    */
static int  g_len;      /* current snake length      */

static int  g_dx, g_dy;   /* current movement delta   */
static int  g_ndx, g_ndy; /* buffered next direction  */

static pt_t g_food;
static int  g_score;
static int  g_alive;
static unsigned int g_move_ticks;   /* current move interval in ticks */

/* --------------------------------------------------------------------------
 * Tiny XorShift32 PRNG — seeded from tick_count at game start
 * -------------------------------------------------------------------------- */
static unsigned int g_rng;

static unsigned int sn_rand(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/* --------------------------------------------------------------------------
 * Food placement — pick a random empty cell
 * -------------------------------------------------------------------------- */
static void place_food(void)
{
    int attempt;
    for (attempt = 0; attempt < 400; attempt++) {
        int fx = (int)(sn_rand() % (unsigned int)BW);
        int fy = (int)(sn_rand() % (unsigned int)BH);
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
 * Draw routines  (only draw what changed to avoid flicker)
 * -------------------------------------------------------------------------- */
static void draw_border(void)
{
    int i;
    kputs("\033[1m\033[36m");   /* bold cyan */

    /* Top edge */
    sn_goto(1, 2);
    kputc('+');
    for (i = 0; i < BW; i++) kputc('-');
    kputc('+');

    /* Bottom edge */
    sn_goto(1, BH + 3);
    kputc('+');
    for (i = 0; i < BW; i++) kputc('-');
    kputc('+');

    /* Side edges */
    for (i = 0; i < BH; i++) {
        sn_goto(1,      i + 3); kputc('|');
        sn_goto(BW + 2, i + 3); kputc('|');
    }
    kputs("\033[0m");
}

static void draw_score(void)
{
    sn_goto(1, 1);
    kputs("\033[1m\033[33m");
    kputs(" T-OS SNAKE ");
    kputs("\033[0m\033[33m");
    kputs(" Score: ");
    kputs("\033[1m\033[92m");
    sn_uint((unsigned int)g_score);
    kputs("      \033[0m");   /* trailing spaces erase old wider numbers */
}

static void draw_food(void)
{
    sn_cell(g_food.x, g_food.y, "\033[1m\033[31m", '*');
}

/* Draw the head (@) and the segment just behind it (o).
 * Called every move — only 2 cells change per step. */
static void draw_snake_head(void)
{
    /* Head */
    int hx = body[g_head].x;
    int hy = body[g_head].y;
    sn_cell(hx, hy, "\033[1m\033[92m", '@');

    /* Segment behind head (was head, now body colour) */
    if (g_len > 1) {
        int prev = (g_head - 1 + MAX_LEN) % MAX_LEN;
        sn_cell(body[prev].x, body[prev].y, "\033[32m", 'o');
    }
}

static void erase_cell(int px, int py)
{
    sn_goto(SCR_COL(px), SCR_ROW(py));
    kputc(' ');
}

/* Full initial draw: border + all segments + food + score */
static void full_redraw(void)
{
    int i;
    kputs("\033[2J");       /* clear screen */
    draw_border();
    draw_score();

    /* Draw each body segment */
    for (i = 0; i < g_len; i++) {
        int idx = (g_head - i + MAX_LEN) % MAX_LEN;
        if (i == 0) {
            sn_cell(body[idx].x, body[idx].y, "\033[1m\033[92m", '@');
        } else {
            sn_cell(body[idx].x, body[idx].y, "\033[32m", 'o');
        }
    }
    draw_food();

    /* Hint line */
    sn_goto(1, BH + 4);
    kputs("\033[90m  Controls: W/A/S/D or Arrow keys | Q = quit \033[0m");
}

/* --------------------------------------------------------------------------
 * Game initialisation
 * -------------------------------------------------------------------------- */
static void game_init(void)
{
    int i;
    int sx = BW / 2;
    int sy = BH / 2;

    g_len  = 3;
    g_head = 2;
    g_dx   = 1;  g_dy  = 0;    /* start moving right */
    g_ndx  = 1;  g_ndy = 0;
    g_score = 0;
    g_alive = 1;

    /* Lay out initial body: tail…head moving right */
    for (i = 0; i < g_len; i++) {
        body[i].x = (signed char)(sx - (g_len - 1 - i));
        body[i].y = (signed char)sy;
    }

    g_rng = tick_count ^ 0xDEADBEEFU;
    if (g_rng == 0) g_rng = 1;
    g_move_ticks = MOVE_TICKS_START;
    place_food();
}

/* --------------------------------------------------------------------------
 * Main game loop
 * -------------------------------------------------------------------------- */
void snake_run(void)
{
    unsigned int last_tick;
    char c;

    kputs("\033[?25l");     /* hide cursor */

    game_init();
    full_redraw();

    last_tick = tick_count;

    while (g_alive) {

        /* ---- Input polling ---- */
        c = kgetc();
        if (c) {
            /* WASD steering — never allow 180° reversal */
            if ((c == 'w' || c == 'W') && g_dy == 0) { g_ndx =  0; g_ndy = -1; }
            if ((c == 's' || c == 'S') && g_dy == 0) { g_ndx =  0; g_ndy =  1; }
            if ((c == 'a' || c == 'A') && g_dx == 0) { g_ndx = -1; g_ndy =  0; }
            if ((c == 'd' || c == 'D') && g_dx == 0) { g_ndx =  1; g_ndy =  0; }
            if (c == 'q' || c == 'Q')  { g_alive = 0; break; }

            /* Arrow keys: ESC [ A/B/C/D — read the two extra bytes */
            if (c == '\033') {
                char c2 = kgetc();
                if (c2 == '[') {
                    char c3 = kgetc();
                    if (c3 == 'A' && g_dy == 0) { g_ndx =  0; g_ndy = -1; }
                    if (c3 == 'B' && g_dy == 0) { g_ndx =  0; g_ndy =  1; }
                    if (c3 == 'C' && g_dx == 0) { g_ndx =  1; g_ndy =  0; }
                    if (c3 == 'D' && g_dx == 0) { g_ndx = -1; g_ndy =  0; }
                }
            }
        }

        /* ---- Tick-gated update ---- */
        if (tick_count - last_tick < g_move_ticks) {
            /* Yield a bit to reduce UART polling hammering */
            volatile int nop;
            for (nop = 0; nop < 500; nop++) __asm__ volatile ("nop");
            continue;
        }
        last_tick = tick_count;

        /* Commit buffered direction */
        g_dx = g_ndx;
        g_dy = g_ndy;

        /* Compute new head position */
        int nx = (int)body[g_head].x + g_dx;
        int ny = (int)body[g_head].y + g_dy;

        /* Wall collision */
        if (nx < 0 || nx >= BW || ny < 0 || ny >= BH) {
            g_alive = 0;
            break;
        }

        /* Self-collision (skip the very tail — it will vacate this tick) */
        {
            int i;
            for (i = 0; i < g_len - 1; i++) {
                int idx = (g_head - i + MAX_LEN) % MAX_LEN;
                if ((int)body[idx].x == nx && (int)body[idx].y == ny) {
                    g_alive = 0;
                    break;
                }
            }
        }
        if (!g_alive) break;

        /* Check food */
        int ate = (nx == (int)g_food.x && ny == (int)g_food.y);

        /* Remember old tail before advancing head */
        int tail_idx = (g_head - g_len + 1 + MAX_LEN) % MAX_LEN;
        int old_tx   = (int)body[tail_idx].x;
        int old_ty   = (int)body[tail_idx].y;

        /* Advance head in ring buffer */
        g_head = (g_head + 1) % MAX_LEN;
        body[g_head].x = (signed char)nx;
        body[g_head].y = (signed char)ny;

        if (ate) {
            g_len++;
            g_score += 10;

            /* Speed up every 50 points — subtract 1 tick, floor at minimum */
            if ((g_score % 50) == 0 && g_move_ticks > (unsigned int)MOVE_TICKS_MIN)
                g_move_ticks--;

            place_food();
            draw_food();
            draw_score();
        } else {
            /* Erase the vacated tail */
            erase_cell(old_tx, old_ty);
        }

        /* Redraw new head (and former head → body colour) */
        draw_snake_head();
    }

    /* ---- Game over screen ---- */
    {
        int cx = BW / 2 - 5;
        int cy = BH / 2 + 1;
        sn_goto(SCR_COL(cx), SCR_ROW(cy));
        kputs("\033[1m\033[41m\033[37m  GAME OVER  \033[0m");
        sn_goto(SCR_COL(cx - 1), SCR_ROW(cy + 1));
        kputs("\033[33m  Score: ");
        sn_uint((unsigned int)g_score);
        kputs("  \033[0m");
        sn_goto(1, BH + 5);
        kputs("\033[90m  Press Enter to return to shell... \033[0m");
    }

    kputs("\033[?25h");     /* restore cursor */

    /* Wait for Enter/Q before returning to shell */
    do { c = kgetc(); } while (c != '\r' && c != '\n' && c != 'q' && c != 'Q');

    /* Clean up: full clear so shell prompt is tidy */
    kputs("\033[2J\033[H\033[0m");
}
