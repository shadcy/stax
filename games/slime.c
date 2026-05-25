#include "engine2d.h"
#include "../gfx/gfx.h"
#include "keyboard.h"
#include "console.h"
#include "font8x16.h"

#define TILE_SIZE 16

/* Physics Constants (Time integrated: values are per ms)
 * Halved from 640x480 scale to match 320x200 scale */
#define GRAVITY     1              /* ~0.004 px/ms^2 */
#define MAX_FALL    (TO_FIX(1)/2)  /* 0.5 px/ms */
#define JUMP_VEL    (-TO_FIX(1)/2) /* -0.5 px/ms */
#define MOVE_SPEED  (TO_FIX(1)/4)  /* 0.25 px/ms */
#define STICKY_FALL (TO_FIX(1)/16) /* 0.0625 px/ms */

// 8-bit DOOM-style Palette Indices
#define C_WALL    2    /* Dark gray */
#define C_ACID    35   /* Magenta */
#define C_SLIME   20   /* Green */
#define C_PORTAL  50   /* Cyan */
#define C_BG      0    /* Black */
#define C_HUD     85   /* Yellow */

static const char *level_map[] = {
    "####################",
    "#                  #",
    "#      ##          #",
    "#               @  #",
    "#                  #",
    "#    ###           #",
    "#                  #",
    "#           ####   #",
    "#                  #",
    "#                  #",
    "#   ^     ^^^^^^   #",
    "####################"
};
#define MAP_H 12
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

static void draw_text(int x_pos, int y_pos, const char *str, uint8_t color) {
    while (*str) {
        unsigned char c = *str++;
        const unsigned char *g = font8x16_data[c];
        for (int r = 0; r < 16; r++) {
            unsigned char bits = g[r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) {
                    gfx8_putpixel(x_pos + b, y_pos + r, color);
                }
            }
        }
        x_pos += 8;
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
    player.y = TO_FIX(9 * TILE_SIZE);
    player.vx = 0;
    player.vy = 0;
    player.on_ground = 0;
    player.on_wall = 0;
    player.dead = 0;
    player.won = 0;
}

static void slime_init(void) {
    gfx_init(); // Init palette
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
    gfx_profiler_update();
    
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
    int p_size = 8;
    
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
    gfx8_clear(C_BG);
    
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char t = level_map[y][x];
            int px = x * TILE_SIZE;
            int py = y * TILE_SIZE;
            
            if (t == '#') {
                gfx8_fillrect(px, py, TILE_SIZE, TILE_SIZE, C_WALL);
                gfx8_drawline(px, py, px + TILE_SIZE - 1, py, 10); // Lighter gray
                gfx8_drawline(px, py, px, py + TILE_SIZE - 1, 10);
            } else if (t == '^') {
                gfx8_fillrect(px, py + TILE_SIZE/2, TILE_SIZE, TILE_SIZE/2, C_ACID);
                gfx8_drawline(px + TILE_SIZE/4, py + TILE_SIZE/2, px + TILE_SIZE/2, py, C_ACID);
                gfx8_drawline(px + TILE_SIZE/2, py, px + 3*TILE_SIZE/4, py + TILE_SIZE/2, C_ACID);
            } else if (t == '@') {
                gfx8_fillrect(px + 4, py + 4, 8, 8, C_PORTAL);
                gfx8_drawline(px, py, px + TILE_SIZE, py + TILE_SIZE, C_PORTAL);
                gfx8_drawline(px, py + TILE_SIZE, px + TILE_SIZE, py, C_PORTAL);
            }
        }
    }
    
    int p_size = 8;
    int px = FROM_FIX(player.x);
    int py = FROM_FIX(player.y);
    if (!player.dead) {
        gfx8_fillrect(px, py, p_size, p_size, C_SLIME);
        gfx8_fillrect(px + 2, py + 2, 4, 4, 255); // White eye
    } else {
        gfx8_fillrect(px, py + 5, 2, 2, C_SLIME);
        gfx8_fillrect(px + 5, py + 6, 2, 2, C_SLIME);
        gfx8_fillrect(px + 2, py + 3, 2, 2, C_SLIME);
    }
    
    draw_text(10, 10, "SLIME ESCAPE", C_HUD);
    
    if (player.dead) {
        draw_text(80, 80, "SLIME MELTED", 70); // Red
        draw_text(85, 100, "R to Restart", 255);
    } else if (player.won) {
        draw_text(80, 80, "SECTOR CLEARED", C_PORTAL);
        draw_text(85, 100, "R to Restart", 255);
    }
    
    if (debug_mode) {
        gfx8_drawline(px, py, px + p_size, py, C_HUD);
        gfx8_drawline(px, py, px, py + p_size, C_HUD);
        gfx8_drawline(px + p_size, py, px + p_size, py + p_size, C_HUD);
        gfx8_drawline(px, py + p_size, px + p_size, py + p_size, C_HUD);
        
        char v_str[64] = "VX: ";
        char temp[16];
        itoa(player.vx, temp);
        int i = 0, j = 0;
        while (v_str[i]) i++;
        while (temp[j]) v_str[i++] = temp[j++];
        v_str[i++] = ' '; v_str[i++] = 'V'; v_str[i++] = 'Y'; v_str[i++] = ':'; v_str[i++] = ' ';
        itoa(player.vy, temp);
        j = 0;
        while (temp[j]) v_str[i++] = temp[j++];
        v_str[i] = '\0';
        
        draw_text(10, 180, v_str, C_HUD);
    }
    
    gfx_profiler_draw();
    
    /* Blit 8-bit 320x200 backbuffer scaled 2x to VRAM */
    gfx_present();
}

void cmd_slime(int argc, char **argv) {
    debug_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == 'd') {
            debug_mode = 1;
        }
    }
    
    kputs("Starting Slime Escape (8-bit Software Rendered)...\n");
    
    EngineApp app = {
        .init = slime_init,
        .update = slime_update,
        .draw = slime_draw
    };
    engine2d_run(&app);
    
    extern int gfx_console_init(void);
    gfx_console_init();
}
