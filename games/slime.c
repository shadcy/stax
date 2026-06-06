#include "engine2d.h"
#include "../gfx/gfx.h"
#include "keyboard.h"
#include "console.h"
#include "font8x16.h"
#include "assets.h"
/* Physics Constants (16.16 Fixed Point, Pixels per Second) */
#define GRAVITY     TO_FIX(2000)    /* Extremely snappy gravity */
#define MAX_FALL    TO_FIX(600)     
#define JUMP_VEL    (-TO_FIX(500))  /* Fast jump */
#define MOVE_SPEED  TO_FIX(300)     /* Fast run */
#define STICKY_FALL TO_FIX(80)      

#define TILE_SIZE 16

// 8-bit DOOM-style Palette Indices
#define C_WALL    2    /* Dark gray */
#define C_ACID    35   /* Magenta */
#define C_SLIME   253  /* Custom Green */
#define C_PORTAL  50   /* Cyan */
#define C_BG      0    /* Black */
#define C_HUD     85   /* Yellow */
#define C_RED     254  /* Custom Red */

#define NUM_LEVELS 6
static const char *levels[NUM_LEVELS][12] = {
    { // Level 1: Basics
        "####################",
        "#                  #",
        "#                  #",
        "#               @  #",
        "#                  #",
        "#    ###           #",
        "#                  #",
        "#           ####   #",
        "#                  #",
        "#                  #",
        "#         ^^^^^^   #",
        "####################"
    },
    { // Level 2: Jumps
        "####################",
        "#                  #",
        "#  @               #",
        "# ###              #",
        "#       ###        #",
        "#                  #",
        "#             ###  #",
        "#                  #",
        "#      ###         #",
        "#                  #",
        "#^^^^^^^^^^^^^^^^^^#",
        "####################"
    },
    { // Level 3: Wall jumps
        "####################",
        "#                  #",
        "#           #   @  #",
        "#           #  ### #",
        "#     #     #      #",
        "#     #     #      #",
        "#     #            #",
        "#     #            #",
        "#                  #",
        "#                  #",
        "# ###^^^^^^^^^^^^^^#",
        "####################"
    },
    { // Level 4: Tunnels
        "####################",
        "#                  #",
        "#                  #",
        "#   #########   @  #",
        "#   #       #  ### #",
        "#   #       #      #",
        "#   #       #      #",
        "#   #              #",
        "#   #       #      #",
        "#           #      #",
        "# ###^^^^^^^#^^^^^^#",
        "####################"
    },
    { // Level 5: Platforms
        "####################",
        "#               @  #",
        "#              ### #",
        "#     ###          #",
        "#                  #",
        "#  ###             #",
        "#          ###     #",
        "#                  #",
        "#                  #",
        "#      #           #",
        "#^^^^^^^^^^^^^^^^^^#",
        "####################"
    },
    { // Level 6: Gauntlet
        "####################",
        "#                  #",
        "#                  #",
        "#   #   #   #   @  #",
        "#   #   #   #  ### #",
        "#   #   #   #      #",
        "#   #   #   #      #",
        "#                  #",
        "#                  #",
        "#                  #",
        "#^^^^^^^^^^^^^^^^^^#",
        "####################"
    }
};

#define MAP_H 12
#define MAP_W 20
static int current_level = 0;

typedef struct {
    int x, y;    
    int vx, vy;  
    int on_ground;
    int on_wall; 
} Slime;

typedef enum {
    STATE_SPLASH,
    STATE_TITLE,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_GAMEOVER,
    STATE_WIN
} GameState;

static Slime player;
static GameState game_state = STATE_PLAY;
static int debug_mode = 0;
static int splash_timer = 0;
static int fade_level = 0;
static int menu_selection = 0; // 0=Play, 1=Debug, 2=Exit
static int up_pressed = 0;
static int down_pressed = 0;
static int select_pressed = 0;
static int death_count = 0;

static void slime_draw_text(int x_pos, int y_pos, const char *str, uint8_t color) {
    /* Draw text shadow first */
    const char *s = str;
    int xp = x_pos + 1;
    while (*s) {
        unsigned char c = *s++;
        const unsigned char *g = font8x16_data[c];
        for (int r = 0; r < 16; r++) {
            unsigned char bits = g[r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) gfx8_putpixel(xp + b, y_pos + r + 1, C_WALL);
            }
        }
        xp += 8;
    }
    /* Draw actual text */
    s = str;
    xp = x_pos;
    while (*s) {
        unsigned char c = *s++;
        const unsigned char *g = font8x16_data[c];
        for (int r = 0; r < 16; r++) {
            unsigned char bits = g[r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) gfx8_putpixel(xp + b, y_pos + r, color);
            }
        }
        xp += 8;
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
}

static void slime_init(void) {
    gfx_init(); // Init palette
    gfx_set_fade(255); // No fade, full brightness
    splash_timer = 0;
    game_state = STATE_PLAY;
    reset_player();
}

static int is_solid(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return (levels[current_level][ty][tx] == '#');
}

static int check_collision(int px, int py, int size) {
    return is_solid(px, py) || 
           is_solid(px + size - 1, py) ||
           is_solid(px, py + size - 1) ||
           is_solid(px + size - 1, py + size - 1);
}

static int is_acid(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    return (levels[current_level][ty][tx] == '^');
}

static int is_portal(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    return (levels[current_level][ty][tx] == '@');
}
static char slime_last_key = 0;

static void slime_key_event(struct window *win, char c) {
    (void)win;
    slime_last_key = c;
}

static void slime_update(int dt_ms) {
    if (game_state == STATE_SPLASH) {
        splash_timer += dt_ms;
        gfx_set_fade(255); // No fade, full brightness
        if (splash_timer >= 6500) {
            game_state = STATE_TITLE;
        }
        return;
    }

    if (game_state == STATE_TITLE) {
        int current_up = kb_is_pressed('w') || kb_is_pressed('W') || kb_is_pressed(0x48) || slime_last_key == 'w' || slime_last_key == 'W';
        int current_down = kb_is_pressed('s') || kb_is_pressed('S') || kb_is_pressed(0x50) || slime_last_key == 's' || slime_last_key == 'S';
        int current_select = kb_is_pressed(' ') || kb_is_pressed('\n') || slime_last_key == ' ' || slime_last_key == '\n' || slime_last_key == '\r';
        
        if (current_up && !up_pressed) {
            menu_selection--;
            if (menu_selection < 0) menu_selection = 1; /* only Play and Debug for now */
        }
        if (current_down && !down_pressed) {
            menu_selection++;
            if (menu_selection > 1) menu_selection = 0;
        }
        
        if (current_select && !select_pressed) {
            if (menu_selection == 0) {
                reset_player();
                game_state = STATE_PLAY;
            } else if (menu_selection == 1) {
                debug_mode = !debug_mode;
            }
        }
        
        up_pressed = current_up;
        down_pressed = current_down;
        select_pressed = current_select;
        return;
    }
    if (game_state == STATE_PAUSE) {
        if (slime_last_key == 'p' || slime_last_key == 'P' || slime_last_key == '\x1b') {
            game_state = STATE_PLAY;
            slime_last_key = 0;
        }
        return;
    }
    
    if (game_state == STATE_GAMEOVER || game_state == STATE_WIN) {
        if (kb_is_pressed('r') || kb_is_pressed('R') || kb_is_pressed(' ') || kb_is_pressed('\n') || slime_last_key == 'r' || slime_last_key == 'R' || slime_last_key == ' ') {
            reset_player();
            game_state = STATE_PLAY;
        }
        return;
    }
    
    if (slime_last_key == 'p' || slime_last_key == 'P' || slime_last_key == '\x1b') {
        game_state = STATE_PAUSE;
        slime_last_key = 0;
        return;
    }

    gfx_profiler_update();
    
    if (dt_ms > 40) dt_ms = 40; /* Cap dt to prevent physics tunneling on emulator lag spikes */

    int input_x = 0;
    if (kb_is_pressed(KB_LEFT) || kb_is_pressed('a') || kb_is_pressed('A') || slime_last_key == 'a' || slime_last_key == 'A') input_x = -1;
    if (kb_is_pressed(KB_RIGHT) || kb_is_pressed('d') || kb_is_pressed('D') || slime_last_key == 'd' || slime_last_key == 'D') input_x = 1;
    
    player.vx = input_x * MOVE_SPEED;
    
    /* Exact time integration for gravity */
    player.vy += (GRAVITY * dt_ms) / 1000;
    
    if (player.on_wall != 0 && player.vy > STICKY_FALL) {
        player.vy = STICKY_FALL;
    } else if (player.vy > MAX_FALL) {
        player.vy = MAX_FALL;
    }
    
    if ((kb_is_pressed(KB_UP) || kb_is_pressed('w') || kb_is_pressed('W') || kb_is_pressed(' ') || slime_last_key == 'w' || slime_last_key == 'W' || slime_last_key == ' ') && player.on_ground) {
        player.vy = JUMP_VEL;
        player.on_ground = 0;
    } else if ((kb_is_pressed(KB_UP) || kb_is_pressed('w') || kb_is_pressed('W') || kb_is_pressed(' ') || slime_last_key == 'w' || slime_last_key == 'W' || slime_last_key == ' ') && player.on_wall != 0) {
        player.vy = JUMP_VEL;
        player.vx = -player.on_wall * MOVE_SPEED;
        player.on_wall = 0;
    }
    
    int p_size = 8;
    
    /* X axis integration and collision */
    player.x += (player.vx * dt_ms) / 1000;
    int ix = FROM_FIX(player.x);
    int iy = FROM_FIX(player.y);
    
    if (check_collision(ix, iy, p_size)) {
        if (player.vx > 0) {
            ix = ((ix + p_size - 1) / TILE_SIZE) * TILE_SIZE - p_size;
            player.on_wall = 1;
        } else if (player.vx < 0) {
            ix = (ix / TILE_SIZE + 1) * TILE_SIZE;
            player.on_wall = -1;
        }
        player.x = TO_FIX(ix);
        player.vx = 0;
    } else {
        player.on_wall = 0;
    }
    
    /* Y axis integration and collision */
    player.y += (player.vy * dt_ms) / 1000;
    ix = FROM_FIX(player.x);
    iy = FROM_FIX(player.y);
    player.on_ground = 0;
    
    if (check_collision(ix, iy, p_size)) {
        if (player.vy > 0) {
            iy = ((iy + p_size - 1) / TILE_SIZE) * TILE_SIZE - p_size;
            player.on_ground = 1;
            player.on_wall = 0;
        } else if (player.vy < 0) {
            iy = (iy / TILE_SIZE + 1) * TILE_SIZE;
        }
        player.y = TO_FIX(iy);
        player.vy = 0;
    }
    
    int cx = ix + p_size / 2;
    int cy = iy + p_size / 2;
    if (is_acid(cx, cy)) {
        game_state = STATE_GAMEOVER;
        death_count++;
    }
    if (is_portal(cx, cy)) {
        current_level++;
        if (current_level >= NUM_LEVELS) {
            game_state = STATE_WIN;
            current_level = 0; /* Reset for next play */
        } else {
            reset_player();
        }
    }
    
    slime_last_key = 0;
}

static void slime_draw(void) {
    gfx8_clear(C_BG);
    
    if (game_state == STATE_SPLASH) {
        int x = (320 - spr_tos_engine_width) / 2;
        int y = (200 - spr_tos_engine_height) / 2;
        gfx8_draw_sprite(x, y, spr_tos_engine_width, spr_tos_engine_height, spr_tos_engine_data);
        gfx_present();
        return;
    }
    
    if (game_state == STATE_TITLE) {
        slime_draw_text(110, 50, "SLIME ESCAPE", C_SLIME);
        slime_draw_text(120, 110, "NEW GAME", menu_selection == 0 ? C_HUD : C_WALL);
        slime_draw_text(120, 130, debug_mode ? "DEBUG: ON" : "DEBUG: OFF", menu_selection == 1 ? C_HUD : C_WALL);
        
        slime_draw_text(105, 110 + (menu_selection * 20), ">", C_HUD);
        gfx_present();
        return;
    }
    
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char t = levels[current_level][y][x];
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
    if (game_state == STATE_PLAY) {
        int w = 12, h = 12;
        int off_x = -2, off_y = -4;
        
        /* Procedural squish animation based on velocity */
        if (player.vy < -TO_FIX(100)) { 
            w = 8; h = 16; off_x = 0; off_y = -8; 
        } else if (player.vy > TO_FIX(100)) { 
            w = 8; h = 14; off_x = 0; off_y = -6; 
        } else if (player.vx != 0) { 
            w = 14; h = 10; off_x = -3; off_y = -2; 
        } else if (player.on_ground) {
            w = 14; h = 10; off_x = -3; off_y = -2; /* idle breathing flat */
        }
        
        if (player.on_wall == 1) { w = 10; h = 14; off_x = -2; off_y = -6; }
        else if (player.on_wall == -1) { w = 10; h = 14; off_x = 0; off_y = -6; }

        gfx8_fillrect(px + off_x, py + off_y, w, h, C_SLIME);
        
        /* Eyes */
        int eye_x = player.vx > 0 ? 6 : (player.vx < 0 ? 2 : 4);
        int eye_y = player.vy < 0 ? 2 : (player.vy > 0 ? 6 : 4);
        gfx8_fillrect(px + off_x + eye_x, py + off_y + eye_y, 2, 2, C_BG);
        gfx8_fillrect(px + off_x + eye_x + 4, py + off_y + eye_y, 2, 2, C_BG);
    } else {
        /* Splat animation for death */
        gfx8_fillrect(px, py + 5, 4, 4, C_SLIME);
        gfx8_fillrect(px + 6, py + 6, 3, 3, C_SLIME);
        gfx8_fillrect(px - 3, py + 4, 3, 3, C_SLIME);
    }
    
    if (game_state == STATE_PAUSE) {
        gfx8_fillrect(110, 70, 100, 40, C_WALL);
        gfx8_drawline(110, 70, 210, 70, 10);
        gfx8_drawline(110, 70, 110, 110, 10);
        slime_draw_text(136, 78, "PAUSED", C_HUD);
        slime_draw_text(120, 96, "[ PRESS P ]", 255);
    }
    
    if (game_state == STATE_GAMEOVER) {
        slime_draw_text(130, 80, "MELTED", C_RED); // Custom Red
        slime_draw_text(115, 100, "[ PRESS R ]", 255);
    } else if (game_state == STATE_WIN) {
        slime_draw_text(130, 80, "CLEARED", C_PORTAL);
        slime_draw_text(115, 100, "[ PRESS R ]", 255);
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
        
        slime_draw_text(10, 180, v_str, C_HUD);
        gfx_profiler_draw();
    }
}

static void slime_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    
    slime_draw(); /* Updates the 320x200 gfx_backbuffer */
    
    /* Now blit 320x200 gfx_backbuffer to cx, cy scaled 2x */
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
            
            if (dest_x1 >= cx + cw || dest_x1 >= 640) break;
            
            uint16_t color = gfx_faded_palette[src[y * 320 + x]];
            
            row1[dest_x1] = color;
            if (dest_x2 < cx + cw && dest_x2 < 640) {
                row1[dest_x2] = color;
            }
            
            if (dest_y2 < cy + ch && dest_y2 < 480) {
                row2[dest_x1] = color;
                if (dest_x2 < cx + cw && dest_x2 < 640) {
                    row2[dest_x2] = color;
                }
            }
        }
    }
}

static void slime_update_window(struct window *win, int dt_ms) {
    (void)win;
    slime_update(dt_ms);
}

#include "../include/wm.h"

void cmd_slime(int argc, char **argv) {
    debug_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == 'd') {
            debug_mode = 1;
        }
    }
    
    slime_init();
    
    /* Open a 640x400 window (+ titlebar and borders) */
    /* Width: 640, Height: 400 */
    window_t *win = wm_add_window(0, 0, 640, 400, "Slime Escape", slime_draw_window);
    if (win) {
        win->update_client = slime_update_window;
        win->key_event = slime_key_event;
    }
}
