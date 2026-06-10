#include "engine2d.h"
#include "../gfx/gfx.h"
#include "keyboard.h"
#include "console.h"
#include "font8x16.h"
#include "wm.h"

#define TILE_SIZE 16
#define MAP_W 16
#define MAP_H 10

#define C_WALL    2    /* Dark gray */
#define C_FLOOR   25   /* Light gray */
#define C_BOX     60   /* Brownish */
#define C_TARGET  85   /* Yellow */
#define C_PLAYER  150  /* Blue */
#define C_SUCCESS 253  /* Green */

static const char *sokoban_levels[][MAP_H] = {
    { // Level 1
        "                ",
        "     ####       ",
        "     #  #       ",
        "   ###  #       ",
        "   # .$.#       ",
        "   # @  #       ",
        "   ######       ",
        "                ",
        "                ",
        "                "
    },
    { // Level 2
        "                ",
        "   #####        ",
        "   #   #        ",
        "   #.$ #        ",
        " ###  $##       ",
        " #  $ $ #       ",
        " # . . .#       ",
        " ## @ ###       ",
        "  #####         ",
        "                "
    }
};
#define NUM_SOKOBAN_LEVELS 2

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    int x, y; // Grid coords
    int px, py; // Pixel coords (for sliding animation)
    Direction dir;
} SokoEntity;

typedef enum {
    SOKO_TITLE,
    SOKO_PLAY,
    SOKO_WIN
} SokoState;

static SokoState s_state = SOKO_TITLE;
static int s_level = 0;
static char s_map[MAP_H][MAP_W];
static SokoEntity s_player;
static int s_moving = 0; // 0 = idle, 1 = sliding
static int s_move_timer = 0;
static int s_target_x, s_target_y;
static int s_box_idx_moving = -1;
static char s_last_key = 0;

static void soko_load_level(int level) {
    s_level = level;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char c = sokoban_levels[level][y][x];
            if (c == '@') {
                s_player.x = x; s_player.y = y;
                s_player.px = x * TILE_SIZE; s_player.py = y * TILE_SIZE;
                s_player.dir = DIR_DOWN;
                s_map[y][x] = ' '; // floor
            } else if (c == '+') { // player on target
                s_player.x = x; s_player.y = y;
                s_player.px = x * TILE_SIZE; s_player.py = y * TILE_SIZE;
                s_player.dir = DIR_DOWN;
                s_map[y][x] = '.'; // target
            } else {
                s_map[y][x] = c;
            }
        }
    }
    s_moving = 0;
}

static int soko_check_win(void) {
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            // If there is a target without a box on it (i.e. it's just '.')
            if (s_map[y][x] == '.' || s_map[y][x] == '+') return 0;
        }
    }
    return 1;
}

static void soko_draw_text(int x_pos, int y_pos, const char *str, uint8_t color) {
    extern const unsigned char font8x16_data[256][16];
    int xp = x_pos + 1;
    const char *s = str;
    while (*s) {
        unsigned char c = *s++;
        for (int r = 0; r < 16; r++) {
            unsigned char bits = font8x16_data[c][r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) gfx8_putpixel(xp + b, y_pos + r + 1, C_WALL);
            }
        }
        xp += 8;
    }
    s = str; xp = x_pos;
    while (*s) {
        unsigned char c = *s++;
        for (int r = 0; r < 16; r++) {
            unsigned char bits = font8x16_data[c][r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) gfx8_putpixel(xp + b, y_pos + r, color);
            }
        }
        xp += 8;
    }
}

static const char *spr_down[2][16] = {
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@ #  # @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "    BB    BB    ",
        "    BB    BB    ",
        "    SS    SS    ",
        "    SS    SS    ",
        "                ",
        "                ",
        "                "
    },
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@ #  # @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "    BB    BB    ",
        "     BB  SS     ",
        "     SS  SS     ",
        "                ",
        "                ",
        "                ",
        "                "
    }
};

static const char *spr_up[2][16] = {
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@      @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "    BB    BB    ",
        "    BB    BB    ",
        "    SS    SS    ",
        "    SS    SS    ",
        "                ",
        "                ",
        "                "
    },
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@      @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "    BB    BB    ",
        "     BB  SS     ",
        "     SS  SS     ",
        "                ",
        "                ",
        "                ",
        "                "
    }
};

static const char *spr_left[2][16] = {
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@ #    @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "      BB BB     ",
        "      BB BB     ",
        "      SS SS     ",
        "      SS SS     ",
        "                ",
        "                ",
        "                "
    },
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@ #    @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "     BB  BB     ",
        "     SS   BB    ",
        "     SS   SS    ",
        "                ",
        "                ",
        "                ",
        "                "
    }
};

static const char *spr_right[2][16] = {
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@    # @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "     BB BB      ",
        "     BB BB      ",
        "     SS SS      ",
        "     SS SS      ",
        "                ",
        "                ",
        "                "
    },
    {
        "                ",
        "     @@@@@@     ",
        "    @@@@@@@@    ",
        "   @@      @@   ",
        "   @@    # @@   ",
        "   @@      @@   ",
        "    @@@@@@@@    ",
        "      BBBB      ",
        "     BBBBBB     ",
        "     BB  BB     ",
        "    BB   SS     ",
        "    SS   SS     ",
        "                ",
        "                ",
        "                ",
        "                "
    }
};

static void soko_draw_sprite(int px, int py, const char *spr[16]) {
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            char ch = spr[r][c];
            if (ch == '@') gfx8_putpixel(px + c, py + r, 153); // Hat
            else if (ch == '#') gfx8_putpixel(px + c, py + r, 255); // Eye
            else if (ch == 'B') gfx8_putpixel(px + c, py + r, C_PLAYER); // Body
            else if (ch == 'S') gfx8_putpixel(px + c, py + r, 0); // Shoe
        }
    }
}

static void soko_draw_player(int px, int py, Direction dir) {
    int frame = 0;
    if (s_moving) {
        frame = (s_move_timer / 75) % 2;
    }
    
    if (dir == DIR_DOWN) soko_draw_sprite(px, py, spr_down[frame]);
    else if (dir == DIR_UP) soko_draw_sprite(px, py, spr_up[frame]);
    else if (dir == DIR_LEFT) soko_draw_sprite(px, py, spr_left[frame]);
    else if (dir == DIR_RIGHT) soko_draw_sprite(px, py, spr_right[frame]);
}

static void soko_update(int dt_ms) {
    if (s_state == SOKO_TITLE) {
        if (s_last_key == '\n' || s_last_key == ' ' || s_last_key == 'w') {
            soko_load_level(0);
            s_state = SOKO_PLAY;
            s_last_key = 0;
        }
        return;
    }
    
    if (s_state == SOKO_WIN) {
        if (s_last_key == ' ') {
            s_level++;
            if (s_level >= NUM_SOKOBAN_LEVELS) {
                s_state = SOKO_TITLE;
            } else {
                soko_load_level(s_level);
                s_state = SOKO_PLAY;
            }
            s_last_key = 0;
        }
        return;
    }
    
    if (s_last_key == 'r' || s_last_key == 'R') {
        soko_load_level(s_level);
        s_last_key = 0;
        return;
    }

    if (s_moving) {
        s_move_timer += dt_ms;
        int max_time = 150; // ms per slide
        if (s_move_timer >= max_time) {
            s_moving = 0;
            s_player.px = s_player.x * TILE_SIZE;
            s_player.py = s_player.y * TILE_SIZE;
            if (soko_check_win()) {
                s_state = SOKO_WIN;
            }
        } else {
            // Interpolate
            int src_x = s_player.x * TILE_SIZE;
            int src_y = s_player.y * TILE_SIZE;
            if (s_player.dir == DIR_LEFT) src_x += TILE_SIZE;
            if (s_player.dir == DIR_RIGHT) src_x -= TILE_SIZE;
            if (s_player.dir == DIR_UP) src_y += TILE_SIZE;
            if (s_player.dir == DIR_DOWN) src_y -= TILE_SIZE;
            
            s_player.px = src_x + ((s_player.x * TILE_SIZE - src_x) * s_move_timer) / max_time;
            s_player.py = src_y + ((s_player.y * TILE_SIZE - src_y) * s_move_timer) / max_time;
        }
    } else {
        int dx = 0, dy = 0;
        if (s_last_key == 'w' || s_last_key == 'W') { dy = -1; s_player.dir = DIR_UP; }
        else if (s_last_key == 's' || s_last_key == 'S') { dy = 1; s_player.dir = DIR_DOWN; }
        else if (s_last_key == 'a' || s_last_key == 'A') { dx = -1; s_player.dir = DIR_LEFT; }
        else if (s_last_key == 'd' || s_last_key == 'D') { dx = 1; s_player.dir = DIR_RIGHT; }
        
        if (dx != 0 || dy != 0) {
            int nx = s_player.x + dx;
            int ny = s_player.y + dy;
            char dest = s_map[ny][nx];
            
            if (dest == ' ' || dest == '.') {
                s_player.x = nx;
                s_player.y = ny;
                s_moving = 1;
                s_move_timer = 0;
            } else if (dest == '$' || dest == '*') {
                int nnx = nx + dx;
                int nny = ny + dy;
                char dest2 = s_map[nny][nnx];
                if (dest2 == ' ' || dest2 == '.') {
                    // Push box
                    s_map[ny][nx] = (dest == '*') ? '.' : ' ';
                    s_map[nny][nnx] = (dest2 == '.') ? '*' : '$';
                    s_player.x = nx;
                    s_player.y = ny;
                    s_moving = 1;
                    s_move_timer = 0;
                }
            }
            s_last_key = 0;
        }
    }
}

static void soko_draw(void) {
    gfx8_clear(10); // Background color
    
    if (s_state == SOKO_TITLE) {
        soko_draw_text(100, 60, "SOKOBAN", C_SUCCESS);
        soko_draw_text(110, 100, "[ PRESS SPACE ]", 255);
        gfx_present();
        return;
    }
    
    int off_x = (320 - MAP_W * TILE_SIZE) / 2;
    int off_y = (200 - MAP_H * TILE_SIZE) / 2;
    
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char t = s_map[y][x];
            int px = off_x + x * TILE_SIZE;
            int py = off_y + y * TILE_SIZE;
            
            if (t == '#') {
                gfx8_fillrect(px, py, TILE_SIZE, TILE_SIZE, C_WALL);
                gfx8_drawline(px, py, px+TILE_SIZE-1, py, 15);
                gfx8_drawline(px, py, px, py+TILE_SIZE-1, 15);
            } else if (t != '\0' && t != ' ') {
                gfx8_fillrect(px, py, TILE_SIZE, TILE_SIZE, C_FLOOR);
                if (t == '.') {
                    gfx8_fillrect(px + 6, py + 6, 4, 4, C_TARGET);
                } else if (t == '$') {
                    gfx8_fillrect(px + 2, py + 2, 12, 12, C_BOX);
                    gfx8_drawline(px+2, py+2, px+13, py+13, C_WALL);
                    gfx8_drawline(px+13, py+2, px+2, py+13, C_WALL);
                } else if (t == '*') {
                    gfx8_fillrect(px + 2, py + 2, 12, 12, C_SUCCESS);
                }
            } else {
                gfx8_fillrect(px, py, TILE_SIZE, TILE_SIZE, C_FLOOR);
            }
        }
    }
    
    // Draw player
    soko_draw_player(off_x + s_player.px, off_y + s_player.py, s_player.dir);
    
    if (s_state == SOKO_WIN) {
        soko_draw_text(100, 80, "LEVEL COMPLETE", C_SUCCESS);
        soko_draw_text(110, 100, "[ PRESS SPACE ]", 255);
    }
    
    soko_draw_text(10, 10, "[R] Restart", 255);
    
    gfx_present();
}

static void sokoban_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    soko_draw();
    
    extern uint16_t* fb_get_buffer(void);
    uint16_t* vram = fb_get_buffer();
    if (!vram) return;
    
    extern uint8_t gfx_backbuffer[];
    extern uint16_t gfx_faded_palette[];
    uint8_t *src = gfx_backbuffer;
    
    for (int y = 0; y < 200; y++) {
        int dest_y1 = cy + y * 2;
        int dest_y2 = dest_y1 + 1;
        if (dest_y1 >= cy + ch || dest_y1 >= 480) break;
        
        uint16_t *row1 = vram + dest_y1 * 640;
        uint16_t *row2 = vram + dest_y2 * 640;
        
        for (int x = 0; x < 320; x++) {
            int dest_x1 = cx + x * 2;
            int dest_x2 = dest_x1 + 1;
            if (dest_x1 >= cx + cw || dest_x1 >= 640) continue;
            
            uint16_t c = gfx_faded_palette[src[y * 320 + x]];
            row1[dest_x1] = c; row1[dest_x2] = c;
            row2[dest_x1] = c; row2[dest_x2] = c;
        }
    }
}

static void sokoban_update_window(struct window *win, int dt_ms) {
    (void)win;
    soko_update(dt_ms);
}

static void sokoban_key_event(struct window *win, char c) {
    (void)win;
    s_last_key = c;
}

void cmd_sokoban(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    /* Check if already running */
    extern struct window *window_list;
    struct window *curr = window_list;
    while (curr) {
        if (curr->update_client == (void*)sokoban_update_window) {
            curr->state = 0; /* WM_STATE_ACTIVE */
            return;
        }
        curr = curr->next;
    }
    
    gfx_init();
    gfx_set_fade(255);
    s_state = SOKO_TITLE;
    
    window_t *win = wm_add_window(0, 0, 640, 424, "Sokoban", (void*)sokoban_draw_window);
    if (win) {
        win->update_client = (void*)sokoban_update_window;
        win->key_event = (void*)sokoban_key_event;
    }
}
