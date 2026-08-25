/* ============================================================================
 * STAX — snake.c
 * Modern Windowed Graphical Snake Game for STAX OS
 * ============================================================================ */

#include "snake.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "wm.h"
#include "keyboard.h"
#include "console.h"
#include <stdint.h>

extern void wm_bring_to_front(struct window *win);

#define GRID_COLS   24
#define GRID_ROWS   18
#define CELL_SZ     18
#define MAX_SNAKE   512

typedef struct {
    int x;
    int y;
} snake_pt_t;

typedef enum {
    SNAKE_STATE_PLAY,
    SNAKE_STATE_PAUSE,
    SNAKE_STATE_GAMEOVER
} snake_state_t;

static snake_pt_t s_body[MAX_SNAKE];
static int        s_head = 0;
static int        s_len  = 4;
static int        s_dx   = 1;
static int        s_dy   = 0;
static int        s_ndx  = 1;
static int        s_ndy  = 0;
static snake_pt_t s_food = {12, 9};
static int        s_score = 0;
static int        s_high_score = 0;
static snake_state_t s_state = SNAKE_STATE_PLAY;
static int        s_timer_ms = 0;
static uint32_t   s_rng = 0x12345678;

static void snake_itoa(int n, char s[]) {
    int i = 0, sign = n;
    if (sign < 0) n = -n;
    do { s[i++] = (char)(n % 10 + '0'); } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = s[j]; s[j] = s[k]; s[k] = temp;
    }
}

static uint32_t snake_rand(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static void snake_place_food(void) {
    for (int attempt = 0; attempt < 500; attempt++) {
        int fx = (int)(snake_rand() % GRID_COLS);
        int fy = (int)(snake_rand() % GRID_ROWS);
        int ok = 1;
        for (int i = 0; i < s_len; i++) {
            int idx = (s_head - i + MAX_SNAKE) % MAX_SNAKE;
            if (s_body[idx].x == fx && s_body[idx].y == fy) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            s_food.x = fx;
            s_food.y = fy;
            return;
        }
    }
}

static void snake_reset_game(void) {
    s_len = 4;
    s_head = 3;
    s_dx = 1; s_dy = 0;
    s_ndx = 1; s_ndy = 0;
    s_score = 0;
    s_timer_ms = 0;
    s_state = SNAKE_STATE_PLAY;

    int sx = 6, sy = 9;
    for (int i = 0; i < s_len; i++) {
        s_body[i].x = sx - (s_len - 1 - i);
        s_body[i].y = sy;
    }
    snake_place_food();
}

static void snake_step(void) {
    if (s_state != SNAKE_STATE_PLAY) return;

    s_dx = s_ndx;
    s_dy = s_ndy;

    int nx = s_body[s_head].x + s_dx;
    int ny = s_body[s_head].y + s_dy;

    /* Wall collision */
    if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) {
        s_state = SNAKE_STATE_GAMEOVER;
        if (s_score > s_high_score) s_high_score = s_score;
        return;
    }

    /* Self collision */
    for (int i = 0; i < s_len - 1; i++) {
        int idx = (s_head - i + MAX_SNAKE) % MAX_SNAKE;
        if (s_body[idx].x == nx && s_body[idx].y == ny) {
            s_state = SNAKE_STATE_GAMEOVER;
            if (s_score > s_high_score) s_high_score = s_score;
            return;
        }
    }

    /* Advance head */
    s_head = (s_head + 1) % MAX_SNAKE;
    s_body[s_head].x = nx;
    s_body[s_head].y = ny;

    /* Check food */
    if (nx == s_food.x && ny == s_food.y) {
        if (s_len < MAX_SNAKE - 1) s_len++;
        s_score += 10;
        if (s_score > s_high_score) s_high_score = s_score;
        snake_place_food();
    }
}

static void snake_key_event(struct window *win, char c) {
    (void)win;
    if (s_state == SNAKE_STATE_GAMEOVER) {
        if (c == 'r' || c == 'R' || c == ' ' || c == '\n' || c == '\r') {
            snake_reset_game();
        }
        return;
    }

    if (c == 'p' || c == 'P' || c == '\x1b') {
        if (s_state == SNAKE_STATE_PLAY) s_state = SNAKE_STATE_PAUSE;
        else if (s_state == SNAKE_STATE_PAUSE) s_state = SNAKE_STATE_PLAY;
        return;
    }

    if (s_state == SNAKE_STATE_PLAY) {
        if ((c == 'w' || c == 'W' || c == (char)0x48) && s_dy == 0) { s_ndx = 0; s_ndy = -1; }
        else if ((c == 's' || c == 'S' || c == (char)0x50) && s_dy == 0) { s_ndx = 0; s_ndy = 1; }
        else if ((c == 'a' || c == 'A' || c == (char)0x4B) && s_dx == 0) { s_ndx = -1; s_ndy = 0; }
        else if ((c == 'd' || c == 'D' || c == (char)0x4D) && s_dx == 0) { s_ndx = 1; s_ndy = 0; }
    }
}

static void snake_update_window(struct window *win, int dt_ms) {
    (void)win;
    if (s_state != SNAKE_STATE_PLAY) return;

    s_timer_ms += dt_ms;
    int interval = 90;
    if (s_score >= 100) interval = 75;
    if (s_score >= 200) interval = 60;
    if (s_score >= 350) interval = 50;

    if (s_timer_ms >= interval) {
        s_timer_ms = 0;
        snake_step();
    }
}

static void snake_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    /* Client area background */
    fb_fillrect(cx, cy, cw, ch, rgb565(18, 20, 26));

    /* Top HUD stats bar */
    int hud_h = 30;
    fb_fillrect(cx, cy, cw, hud_h, rgb565(26, 28, 38));
    fb_drawline(cx, cy + hud_h - 1, cx + cw - 1, cy + hud_h - 1, rgb565(45, 50, 65));

    char sc_str[32] = "SCORE: ";
    char num_buf[16];
    snake_itoa(s_score, num_buf);
    int k = 0, n = 0;
    while (sc_str[k]) k++;
    while (num_buf[n]) sc_str[k++] = num_buf[n++];
    sc_str[k] = '\0';
    draw_text(cx + 12, cy + 7, sc_str, rgb565(50, 235, 100));

    char best_str[32] = "BEST: ";
    snake_itoa(s_high_score, num_buf);
    k = 0; n = 0;
    while (best_str[k]) k++;
    while (num_buf[n]) best_str[k++] = num_buf[n++];
    best_str[k] = '\0';
    draw_text(cx + 140, cy + 7, best_str, rgb565(245, 195, 30));

    draw_text(cx + cw - 190, cy + 7, "WASD: Move | P: Pause", rgb565(150, 155, 175));

    /* Playfield offset */
    int board_w = GRID_COLS * CELL_SZ;
    int board_h = GRID_ROWS * CELL_SZ;
    int off_x = cx + (cw - board_w) / 2;
    int off_y = cy + hud_h + (ch - hud_h - board_h) / 2;

    /* Playfield border */
    fb_drawline(off_x - 1, off_y - 1, off_x + board_w, off_y - 1, rgb565(60, 65, 80));
    fb_drawline(off_x - 1, off_y + board_h, off_x + board_w, off_y + board_h, rgb565(60, 65, 80));
    fb_drawline(off_x - 1, off_y - 1, off_x - 1, off_y + board_h, rgb565(60, 65, 80));
    fb_drawline(off_x + board_w, off_y - 1, off_x + board_w, off_y + board_h, rgb565(60, 65, 80));

    /* Grid background */
    fb_fillrect(off_x, off_y, board_w, board_h, rgb565(14, 16, 22));

    /* Subtle grid dots */
    for (int r = 1; r < GRID_ROWS; r++) {
        for (int c = 1; c < GRID_COLS; c++) {
            fb_putpixel(off_x + c * CELL_SZ, off_y + r * CELL_SZ, rgb565(25, 28, 38));
        }
    }

    /* Draw Food (Apple) */
    int fx = off_x + s_food.x * CELL_SZ;
    int fy = off_y + s_food.y * CELL_SZ;
    fb_fillrect(fx + 3, fy + 4, CELL_SZ - 6, CELL_SZ - 6, rgb565(240, 50, 50));
    fb_fillrect(fx + 5, fy + 2, 4, 3, rgb565(60, 220, 80)); /* Leaf */
    fb_fillrect(fx + 5, fy + 6, 3, 3, rgb565(255, 140, 140)); /* Highlight */

    /* Draw Snake Body */
    for (int i = 0; i < s_len; i++) {
        int idx = (s_head - i + MAX_SNAKE) % MAX_SNAKE;
        int bx = off_x + s_body[idx].x * CELL_SZ;
        int by = off_y + s_body[idx].y * CELL_SZ;

        if (i == 0) {
            /* Head */
            fb_fillrect(bx + 1, by + 1, CELL_SZ - 2, CELL_SZ - 2, rgb565(40, 235, 100));
            fb_drawline(bx + 1, by + 1, bx + CELL_SZ - 2, by + 1, rgb565(120, 255, 160));
            /* Eyes */
            int eye1_x = bx + 4, eye1_y = by + 4;
            int eye2_x = bx + 11, eye2_y = by + 4;
            if (s_dx == 1) { eye1_x = bx + 11; eye1_y = by + 4; eye2_x = bx + 11; eye2_y = by + 11; }
            else if (s_dx == -1) { eye1_x = bx + 4; eye1_y = by + 4; eye2_x = bx + 4; eye2_y = by + 11; }
            else if (s_dy == 1) { eye1_x = bx + 4; eye1_y = by + 11; eye2_x = bx + 11; eye2_y = by + 11; }
            fb_fillrect(eye1_x, eye1_y, 3, 3, COLOR_BLACK);
            fb_fillrect(eye2_x, eye2_y, 3, 3, COLOR_BLACK);
        } else {
            /* Body segment */
            uint16_t body_col = (i % 2 == 0) ? rgb565(30, 195, 80) : rgb565(25, 175, 70);
            fb_fillrect(bx + 2, by + 2, CELL_SZ - 4, CELL_SZ - 4, body_col);
            fb_putpixel(bx + 3, by + 3, rgb565(80, 230, 120));
        }
    }

    /* Overlay Cards for Game Over and Pause */
    if (s_state == SNAKE_STATE_GAMEOVER) {
        int ov_w = 260, ov_h = 110;
        int ov_x = off_x + (board_w - ov_w) / 2;
        int ov_y = off_y + (board_h - ov_h) / 2;

        fb_fillrect(ov_x, ov_y, ov_w, ov_h, rgb565(32, 34, 44));
        fb_drawline(ov_x, ov_y, ov_x + ov_w - 1, ov_y, rgb565(240, 70, 70));
        fb_drawline(ov_x, ov_y, ov_x, ov_y + ov_h - 1, rgb565(240, 70, 70));
        fb_drawline(ov_x + ov_w - 1, ov_y, ov_x + ov_w - 1, ov_y + ov_h - 1, rgb565(140, 30, 30));
        fb_drawline(ov_x, ov_y + ov_h - 1, ov_x + ov_w - 1, ov_y + ov_h - 1, rgb565(140, 30, 30));

        draw_text(ov_x + 85, ov_y + 16, "GAME OVER", rgb565(245, 60, 60));
        
        char final_sc[32] = "Final Score: ";
        snake_itoa(s_score, num_buf);
        k = 0; n = 0;
        while (final_sc[k]) k++;
        while (num_buf[n]) final_sc[k++] = num_buf[n++];
        final_sc[k] = '\0';
        draw_text(ov_x + 65, ov_y + 42, final_sc, COLOR_WHITE);

        draw_text(ov_x + 35, ov_y + 74, "Press [ R ] to Play Again", rgb565(50, 235, 100));
    } else if (s_state == SNAKE_STATE_PAUSE) {
        int ov_w = 200, ov_h = 80;
        int ov_x = off_x + (board_w - ov_w) / 2;
        int ov_y = off_y + (board_h - ov_h) / 2;

        fb_fillrect(ov_x, ov_y, ov_w, ov_h, rgb565(32, 34, 44));
        fb_drawline(ov_x, ov_y, ov_x + ov_w - 1, ov_y, rgb565(80, 150, 255));
        fb_drawline(ov_x, ov_y + ov_h - 1, ov_x + ov_w - 1, ov_y + ov_h - 1, rgb565(30, 70, 140));

        draw_text(ov_x + 72, ov_y + 18, "PAUSED", rgb565(245, 195, 30));
        draw_text(ov_x + 25, ov_y + 46, "Press [ P ] to Resume", COLOR_WHITE);
    }
}

void cmd_snake(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Check if Snake window is already running */
    extern struct window *window_list;
    struct window *curr = window_list;
    while (curr) {
        if (curr->update_client == snake_update_window) {
            curr->state = 0; /* WM_STATE_ACTIVE */
            wm_bring_to_front(curr);
            return;
        }
        curr = curr->next;
    }

    snake_reset_game();

    int win_w = 480;
    int win_h = 390;
    int win_x = ((int)fb_width > win_w) ? ((int)fb_width - win_w) / 2 : 20;
    int win_y = 48;

    window_t *win = wm_add_window(win_x, win_y, win_w, win_h, "Snake Game", snake_draw_window);
    if (win) {
        win->update_client = snake_update_window;
        win->key_event     = snake_key_event;
    }
}

void snake_run(void) {
    cmd_snake(0, 0);
}
