#include "engine2d.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "console.h"
#include "font8x16.h"

#define TILE_SIZE 32

/* Fixed-point arithmetic (8-bit fraction = 1/256 precision) */
#define FIX_SHIFT 8
#define TO_FIX(x) ((x) << FIX_SHIFT)
#define FROM_FIX(x) ((x) >> FIX_SHIFT)

/* Physics Constants (Time integrated: values are per ms) */
#define GRAVITY     1              /* ~0.004 px/ms^2 */
#define MAX_FALL    TO_FIX(1)      /* 1.0 px/ms = 1000 px/sec */
#define JUMP_VEL    (-TO_FIX(1))   /* -1.0 px/ms = 4-tile jump height */
#define MOVE_SPEED  (TO_FIX(1)/2)  /* 0.5 px/ms = 500 px/sec */
#define STICKY_FALL (TO_FIX(1)/8)  /* 0.125 px/ms */

// Dark Cyberpunk Palette
#define C_WALL    COLOR_GRAY_2
#define C_ACID    COLOR_MAGENTA
#define C_SLIME   COLOR_GREEN
#define C_PORTAL  COLOR_CYAN
#define C_BG      COLOR_GRAY_1
#define C_HUD     COLOR_YELLOW

static const char *level_map[] = {
    "####################",
    "#                  #",
    "#                  #",
    "#                  #",
    "#      ##          #",
    "#               @  #",
    "#                  #",
    "#    ###           #",
    "#                  #",
    "#                  #",
    "#           ####   #",
    "#                  #",
    "#                  #",
    "#   ^     ^^^^^^   #",
    "####################"
};
#define MAP_H 15
#define MAP_W 20

typedef struct {
    int x, y;    /* Fixed-point positions */
    int vx, vy;  /* Fixed-point velocities */
    int on_ground;
    int on_wall; 
    int dead;
    int won;
} Slime;

static Slime player;
static int debug_mode = 0;

static void draw_text(int x_pos, int y_pos, const char *str, uint16_t color) {
    while (*str) {
        unsigned char c = *str++;
        const unsigned char *g = font8x16_data[c];
        for (int r = 0; r < FONT8X16_HEIGHT; r++) {
            unsigned char bits = g[r];
            for (int b = 0; b < FONT8X16_WIDTH; b++) {
                if (bits & (0x80u >> b)) {
                    fb_putpixel(x_pos + b, y_pos + r, color);
                }
            }
        }
        x_pos += FONT8X16_WIDTH;
    }
}

static void itoa(int n, char s[]) {
    int i = 0, sign;
    if ((sign = n) < 0) n = -n;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char c = s[j]; s[j] = s[k]; s[k] = c;
    }
}

static void reset_player(void) {
    player.x = TO_FIX(2 * TILE_SIZE);
    player.y = TO_FIX(12 * TILE_SIZE);
    player.vx = 0;
    player.vy = 0;
    player.on_ground = 0;
    player.on_wall = 0;
    player.dead = 0;
    player.won = 0;
}

static void slime_init(void) {
    reset_player();
}

static int is_solid(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return (level_map[ty][tx] == '#');
}

static int is_acid(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    return (level_map[ty][tx] == '^');
}

static int is_portal(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    return (level_map[ty][tx] == '@');
}

static void slime_update(int dt_ms) {
    if (player.dead || player.won) {
        if (kb_is_pressed('r') || kb_is_pressed('R')) reset_player();
        return;
    }

    int input_x = 0;
    if (kb_is_pressed(KB_LEFT) || kb_is_pressed('a') || kb_is_pressed('A')) input_x = -1;
    if (kb_is_pressed(KB_RIGHT) || kb_is_pressed('d') || kb_is_pressed('D')) input_x = 1;
    
    player.vx = input_x * MOVE_SPEED;
    
    /* Exact time integration for gravity */
    player.vy += GRAVITY * dt_ms;
    if (player.on_wall != 0 && player.vy > STICKY_FALL) {
        player.vy = STICKY_FALL;
    } else if (player.vy > MAX_FALL) {
        player.vy = MAX_FALL;
    }
    
    if ((kb_is_pressed(KB_UP) || kb_is_pressed('w') || kb_is_pressed('W') || kb_is_pressed(' ')) && player.on_ground) {
        player.vy = JUMP_VEL;
        player.on_ground = 0;
    } else if ((kb_is_pressed(KB_UP) || kb_is_pressed('w') || kb_is_pressed('W') || kb_is_pressed(' ')) && player.on_wall != 0) {
        player.vy = JUMP_VEL;
        player.vx = -player.on_wall * MOVE_SPEED;
        player.on_wall = 0;
    }
    
    /* X axis exact time integration */
    player.x += player.vx * dt_ms;
    int p_size = 16;
    
    int ix = FROM_FIX(player.x);
    int iy = FROM_FIX(player.y);
    
    if (player.vx > 0) {
        if (is_solid(ix + p_size, iy) || is_solid(ix + p_size, iy + p_size - 1)) {
            ix = ((ix + p_size) / TILE_SIZE) * TILE_SIZE - p_size - 1;
            player.x = TO_FIX(ix);
            player.on_wall = 1;
        } else {
            player.on_wall = 0;
        }
    } else if (player.vx < 0) {
        if (is_solid(ix, iy) || is_solid(ix, iy + p_size - 1)) {
            ix = (ix / TILE_SIZE + 1) * TILE_SIZE;
            player.x = TO_FIX(ix);
            player.on_wall = -1;
        } else {
            player.on_wall = 0;
        }
    } else {
        player.on_wall = 0;
    }
    
    /* Y axis exact time integration */
    player.y += player.vy * dt_ms;
    iy = FROM_FIX(player.y);
    player.on_ground = 0;
    
    if (player.vy > 0) {
        if (is_solid(ix, iy + p_size) || is_solid(ix + p_size - 1, iy + p_size)) {
            iy = ((iy + p_size) / TILE_SIZE) * TILE_SIZE - p_size;
            player.y = TO_FIX(iy);
            player.vy = 0;
            player.on_ground = 1;
            player.on_wall = 0;
        }
    } else if (player.vy < 0) {
        if (is_solid(ix, iy) || is_solid(ix + p_size - 1, iy)) {
            iy = (iy / TILE_SIZE + 1) * TILE_SIZE;
            player.y = TO_FIX(iy);
            player.vy = 0;
        }
    }
    
    int cx = ix + p_size / 2;
    int cy = iy + p_size / 2;
    if (is_acid(cx, cy)) {
        player.dead = 1;
    }
    if (is_portal(cx, cy)) {
        player.won = 1;
    }
}

static void slime_draw(void) {
    fb_clear(C_BG);
    
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char t = level_map[y][x];
            int px = x * TILE_SIZE;
            int py = y * TILE_SIZE;
            
            if (t == '#') {
                fb_fillrect(px, py, TILE_SIZE, TILE_SIZE, C_WALL);
                fb_drawline(px, py, px + TILE_SIZE - 1, py, COLOR_GRAY_3);
                fb_drawline(px, py, px, py + TILE_SIZE - 1, COLOR_GRAY_3);
            } else if (t == '^') {
                fb_fillrect(px, py + TILE_SIZE/2, TILE_SIZE, TILE_SIZE/2, C_ACID);
                fb_drawline(px + TILE_SIZE/4, py + TILE_SIZE/2, px + TILE_SIZE/2, py, C_ACID);
                fb_drawline(px + TILE_SIZE/2, py, px + 3*TILE_SIZE/4, py + TILE_SIZE/2, C_ACID);
            } else if (t == '@') {
                fb_fillrect(px + 8, py + 8, 16, 16, C_PORTAL);
                fb_drawline(px, py, px + TILE_SIZE, py + TILE_SIZE, C_PORTAL);
                fb_drawline(px, py + TILE_SIZE, px + TILE_SIZE, py, C_PORTAL);
            }
        }
    }
    
    int p_size = 16;
    int px = FROM_FIX(player.x);
    int py = FROM_FIX(player.y);
    if (!player.dead) {
        fb_fillrect(px, py, p_size, p_size, C_SLIME);
        fb_fillrect(px + 4, py + 4, 8, 8, COLOR_WHITE);
    } else {
        fb_fillrect(px, py + 10, 4, 4, C_SLIME);
        fb_fillrect(px + 10, py + 12, 4, 4, C_SLIME);
        fb_fillrect(px + 4, py + 6, 4, 4, C_SLIME);
    }
    
    draw_text(10, 10, "SLIME ESCAPE", C_HUD);
    draw_text(400, 10, "WASD / ARROWS to Move", C_HUD);
    draw_text(400, 30, "Q to Exit", C_HUD);
    
    if (player.dead) {
        draw_text(250, 200, "SYSTEM ERROR: SLIME MELTED", COLOR_RED);
        draw_text(260, 220, "Press R to Restart", COLOR_WHITE);
    } else if (player.won) {
        draw_text(250, 200, "SECTOR CLEARED", C_PORTAL);
        draw_text(260, 220, "Press R to Restart", COLOR_WHITE);
    }
    
    if (debug_mode) {
        int px = FROM_FIX(player.x);
        int py = FROM_FIX(player.y);
        fb_drawline(px, py, px + p_size, py, COLOR_YELLOW);
        fb_drawline(px, py, px, py + p_size, COLOR_YELLOW);
        fb_drawline(px + p_size, py, px + p_size, py + p_size, COLOR_YELLOW);
        fb_drawline(px, py + p_size, px + p_size, py + p_size, COLOR_YELLOW);
        
        char v_str[64] = "DEBUG VX: ";
        char temp[16];
        itoa(player.vx, temp);
        int i = 0, j = 0;
        while (v_str[i]) i++;
        while (temp[j]) v_str[i++] = temp[j++];
        v_str[i++] = ' '; v_str[i++] = 'V'; v_str[i++] = 'Y'; v_str[i++] = ':'; v_str[i++] = ' ';
        itoa(player.vy, temp);
        j = 0;
        while (temp[j]) v_str[i++] = temp[j++];
        v_str[i++] = ' '; v_str[i++] = 'G'; v_str[i++] = 'N'; v_str[i++] = 'D'; v_str[i++] = ':';
        v_str[i++] = player.on_ground ? '1' : '0';
        v_str[i] = '\0';
        
        draw_text(10, 460, v_str, COLOR_YELLOW);
    }
}

void cmd_slime(int argc, char **argv) {
    debug_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == 'd') {
            debug_mode = 1;
        }
    }
    
    kputs("Starting Slime Escape...\n");
    
    EngineApp app = {
        .init = slime_init,
        .update = slime_update,
        .draw = slime_draw
    };
    engine2d_run(&app);
    
    extern int gfx_console_init(void);
    gfx_console_init();
}
