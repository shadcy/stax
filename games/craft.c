#include "craft.h"
#include "wm.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "font8x16.h"
#include "console.h"
#include "string.h"
#include "math_fixed.h"
#include "timer.h"

/* World size */
#define MAP_W 32
#define MAP_H 8
#define MAP_D 32

/* Block types */
#define B_AIR 0
#define B_GRASS 1
#define B_DIRT 2
#define B_STONE 3
#define B_WOOD 4

static uint8_t world[MAP_W][MAP_H][MAP_D];

/* Camera state */
static int player_x, player_y, player_z; /* 16.16 fixed point */
static int player_angle; /* 0 to 359 */
static int player_pitch; /* -100 to 100 */

/* Rendering state */
#define SCREEN_W 320
#define SCREEN_H 200
#define FOV 256 /* pixels to projection plane */

static int c_state = 0; /* 0 = running, 1 = pause */

static void craft_init(void) {
    /* Generate a simple terrain */
    for (int x = 0; x < MAP_W; x++) {
        for (int z = 0; z < MAP_D; z++) {
            /* Randomish height */
            int h = 3 + (fix_sin_table[(x * 20) % 360] + fix_cos_table[(z * 30) % 360]) / 32768;
            if (h < 1) h = 1;
            if (h >= MAP_H) h = MAP_H - 1;
            
            for (int y = 0; y < MAP_H; y++) {
                if (y < h - 1) world[x][y][z] = B_STONE;
                else if (y == h - 1) world[x][y][z] = B_DIRT;
                else if (y == h) world[x][y][z] = B_GRASS;
                else world[x][y][z] = B_AIR;
            }
        }
    }
    
    /* Starting pos */
    player_x = TO_FIX(16) + TO_FIX(1)/2;
    player_z = TO_FIX(16) + TO_FIX(1)/2;
    player_y = TO_FIX(5) + TO_FIX(1)/2; /* Head height */
    player_angle = 90;
    player_pitch = 0;
}

static uint16_t get_block_color(uint8_t type, int side) {
    uint16_t base = 0;
    switch(type) {
        case B_GRASS: base = rgb565(34, 139, 34); break;
        case B_DIRT:  base = rgb565(139, 69, 19); break;
        case B_STONE: base = rgb565(169, 169, 169); break;
        case B_WOOD:  base = rgb565(205, 133, 63); break;
        default: return 0;
    }
    /* Simple lighting */
    if (side == 1) return (base >> 1) & 0x7BEF; /* Darker */
    return base;
}

/* Fast absolute value */
static inline int iabs(int x) { return x < 0 ? -x : x; }

static uint16_t buffer_cols[SCREEN_W][SCREEN_H];

/* Render a single frame using 2.5D raycasting */
static void craft_draw(void) {
    /* Draw sky and ground (flat shading) */
    for (int x = 0; x < SCREEN_W; x++) {
        int horizon = SCREEN_H/2 + player_pitch;
        for (int y = 0; y < SCREEN_H; y++) {
            buffer_cols[x][y] = (y < horizon) ? rgb565(135, 206, 235) : rgb565(60, 60, 60);
        }
    }
    
    /* Raycaster for 3D grid */
    for (int x = 0; x < SCREEN_W; x++) {
        /* Screen x mapped to angle */
        int a_offset = (x - (SCREEN_W / 2)) * 60 / SCREEN_W; /* ~60 deg FOV */
        int ray_angle = (player_angle + a_offset) % 360;
        if (ray_angle < 0) ray_angle += 360;
        
        int r_cos = fix_cos_table[ray_angle];
        int r_sin = fix_sin_table[ray_angle];
        
        /* DDA variables */
        int map_x = FROM_FIX(player_x);
        int map_z = FROM_FIX(player_z);
        
        int delta_dist_x = (r_cos == 0) ? 0x7FFFFFFF : iabs(fix_div(TO_FIX(1), r_cos));
        int delta_dist_z = (r_sin == 0) ? 0x7FFFFFFF : iabs(fix_div(TO_FIX(1), r_sin));
        
        int step_x, step_z;
        int side_dist_x, side_dist_z;
        
        if (r_cos < 0) {
            step_x = -1;
            side_dist_x = fix_mul(player_x - TO_FIX(map_x), delta_dist_x);
        } else {
            step_x = 1;
            side_dist_x = fix_mul(TO_FIX(map_x + 1) - player_x, delta_dist_x);
        }
        
        if (r_sin < 0) {
            step_z = -1;
            side_dist_z = fix_mul(player_z - TO_FIX(map_z), delta_dist_z);
        } else {
            step_z = 1;
            side_dist_z = fix_mul(TO_FIX(map_z + 1) - player_z, delta_dist_z);
        }
        
        /* Raycast grid up to max distance */
        int hit = 0;
        int side = 0; /* 0 = X side, 1 = Z side */
        int dist = 0;
        
        int y_top_limit = SCREEN_H;
        int y_bottom_limit = 0;
        
        while (hit < 24) { /* Max 24 steps */
            if (side_dist_x < side_dist_z) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
                dist = side_dist_x - delta_dist_x;
            } else {
                side_dist_z += delta_dist_z;
                map_z += step_z;
                side = 1;
                dist = side_dist_z - delta_dist_z;
            }
            
            if (map_x < 0 || map_x >= MAP_W || map_z < 0 || map_z >= MAP_D) break;
            
            /* Render voxel column at this map_x, map_z */
            for (int y = MAP_H - 1; y >= 0; y--) {
                uint8_t block = world[map_x][y][map_z];
                if (block != B_AIR) {
                    /* Correct fish-eye */
                    int corr_dist = fix_mul(dist, fix_cos_table[(a_offset + 360) % 360]);
                    if (corr_dist < TO_FIX(1)/4) corr_dist = TO_FIX(1)/4;
                    
                    /* Project top and bottom of block */
                    int rel_y_top = TO_FIX(y + 1) - player_y;
                    int rel_y_bot = TO_FIX(y) - player_y;
                    
                    int draw_y_top = (SCREEN_H / 2 + player_pitch) - FROM_FIX(fix_div(fix_mul(rel_y_top, TO_FIX(FOV)), corr_dist));
                    int draw_y_bot = (SCREEN_H / 2 + player_pitch) - FROM_FIX(fix_div(fix_mul(rel_y_bot, TO_FIX(FOV)), corr_dist));
                    
                    if (draw_y_top < 0) draw_y_top = 0;
                    if (draw_y_bot > SCREEN_H) draw_y_bot = SCREEN_H;
                    
                    uint16_t col = get_block_color(block, side);
                    
                    for (int dy = draw_y_top; dy < draw_y_bot; dy++) {
                        if (dy >= y_bottom_limit && dy < y_top_limit) {
                            buffer_cols[x][dy] = col;
                        }
                    }
                    
                    if (draw_y_top < y_top_limit) y_top_limit = draw_y_top;
                    if (y_top_limit <= y_bottom_limit) break;
                }
            }
            hit++;
        }
    }
}

static void craft_update(int dt_ms) {
    (void)dt_ms;
    int speed = TO_FIX(3) / 10;
    int r_cos = fix_cos_table[player_angle];
    int r_sin = fix_sin_table[player_angle];
    
    if (kb_is_pressed('w') || kb_is_pressed('W')) {
        player_x += fix_mul(speed, r_cos);
        player_z += fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('s') || kb_is_pressed('S')) {
        player_x -= fix_mul(speed, r_cos);
        player_z -= fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('a') || kb_is_pressed('A')) {
        player_x += fix_mul(speed, r_sin);
        player_z -= fix_mul(speed, r_cos);
    }
    if (kb_is_pressed('d') || kb_is_pressed('D')) {
        player_x -= fix_mul(speed, r_sin);
        player_z += fix_mul(speed, r_cos);
    }
    if (kb_is_pressed(KB_RIGHT)) {
        player_angle = (player_angle + 5) % 360;
    }
    if (kb_is_pressed(KB_LEFT)) {
        player_angle = (player_angle - 5 + 360) % 360;
    }
    if (kb_is_pressed(KB_UP)) {
        player_pitch += 6;
        if (player_pitch > 80) player_pitch = 80;
    }
    if (kb_is_pressed(KB_DOWN)) {
        player_pitch -= 6;
        if (player_pitch < -80) player_pitch = -80;
    }
    
    /* Basic bounds check */
    int px = FROM_FIX(player_x);
    int pz = FROM_FIX(player_z);
    if (px < 1) player_x = TO_FIX(1);
    if (px >= MAP_W - 1) player_x = TO_FIX(MAP_W - 2);
    if (pz < 1) player_z = TO_FIX(1);
    if (pz >= MAP_D - 1) player_z = TO_FIX(MAP_D - 2);
}

void craft_update_window(struct window *win, int dt_ms) {
    (void)win;
    craft_update(dt_ms);
}

static void craft_draw_window_cb(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    craft_draw();
    
    uint16_t* vram = fb_get_buffer();
    if (!vram) return;
    
    uint32_t screen_w = fb_width;
    uint32_t screen_h = fb_height;
    
    int scale_x = cw / SCREEN_W;
    int scale_y = ch / SCREEN_H;
    if (scale_x < 1) scale_x = 1;
    if (scale_y < 1) scale_y = 1;
    
    for (int y = 0; y < SCREEN_H; y++) {
        for (int dy = 0; dy < scale_y; dy++) {
            int dest_y = cy + y * scale_y + dy;
            if (dest_y >= cy + ch || (uint32_t)dest_y >= screen_h) break;
            
            uint16_t *row = vram + dest_y * screen_w;
            for (int x = 0; x < SCREEN_W; x++) {
                uint16_t c = buffer_cols[x][y];
                for (int dx = 0; dx < scale_x; dx++) {
                    int dest_x = cx + x * scale_x + dx;
                    if (dest_x >= cx + cw || (uint32_t)dest_x >= screen_w) break;
                    row[dest_x] = c;
                }
            }
        }
    }
    
    /* Crosshair in center */
    int mid_x = cx + cw / 2;
    int mid_y = cy + ch / 2;
    fb_fillrect(mid_x - 4, mid_y, 9, 1, COLOR_WHITE);
    fb_fillrect(mid_x, mid_y - 4, 1, 9, COLOR_WHITE);

    /* HUD */
    draw_text(cx + 8, cy + 6, "STAX Craft 3D", COLOR_WHITE);
    draw_text(cx + 8, cy + ch - 20, "WASD: Move | Left/Right: Turn | Space: Place | R: Break", rgb565(220, 230, 245));
}

static void craft_key_event(struct window *win, char c) {
    (void)win;
    if (c == ' ' || c == 'R' || c == 'r') {
        int map_x = FROM_FIX(player_x + fix_mul(TO_FIX(2), fix_cos_table[player_angle]));
        int map_z = FROM_FIX(player_z + fix_mul(TO_FIX(2), fix_sin_table[player_angle]));
        int map_y = FROM_FIX(player_y);
        
        if (map_x >= 0 && map_x < MAP_W && map_z >= 0 && map_z < MAP_D && map_y >= 0 && map_y < MAP_H) {
            if (c == ' ') world[map_x][map_y][map_z] = B_WOOD;
            if (c == 'R' || c == 'r') world[map_x][map_y][map_z] = B_AIR;
        }
    }
}

void cmd_craft(int argc, char **argv) {
    (void)argc; (void)argv;
    
    extern struct window *window_list;
    struct window *curr = window_list;
    while (curr) {
        if (curr->update_client == (void*)craft_update_window) {
            curr->state = 0; 
            extern void wm_bring_to_front(struct window*);
            wm_bring_to_front(curr);
            return;
        }
        curr = curr->next;
    }
    
    craft_init();
    
    int win_w = 644;
    int win_h = 424;
    int win_x = ((int)fb_width > win_w) ? ((int)fb_width - win_w) / 2 : 10;
    int win_y = 38;
    
    window_t *win = wm_add_window(win_x, win_y, win_w, win_h, "STAX Craft 3D", craft_draw_window_cb);
    if (win) {
        win->update_client = craft_update_window;
        win->key_event = craft_key_event;
    }
}
