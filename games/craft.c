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
static int last_time = 0;

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

static void craft_draw_text(int x_pos, int y_pos, const char *str, uint16_t color);

/* Render a single frame using 2.5D raycasting */
static void craft_draw(void) {
    extern uint16_t* fb_get_buffer(void);
    extern uint8_t gfx_backbuffer[];
    
    /* Use gfx_backbuffer as a 16-bit intermediate buffer, wait, gfx_backbuffer is 8-bit!
       We'll render directly to 16-bit window space using the window drawing context later, 
       but for performance and proper scaling, let's render to a local 320x200 array. */
    static uint16_t buffer320[SCREEN_W * SCREEN_H];
    
    /* Draw sky and ground (flat shading) */
    for (int y = 0; y < SCREEN_H; y++) {
        uint16_t color = (y < SCREEN_H/2 + player_pitch) ? rgb565(135, 206, 235) : rgb565(60, 60, 60);
        for (int x = 0; x < SCREEN_W; x++) {
            buffer320[y * SCREEN_W + x] = color;
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
        
        int hit = 0;
        int side = 0;
        int dist = 0;
        
        /* Keep track of occlusion (Y limits) to prevent overdraw */
        int y_top_limit = SCREEN_H;
        int y_bottom_limit = 0;
        
        while (hit < 16) { /* max steps */
            if (side_dist_x < side_dist_z) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_z += delta_dist_z;
                map_z += step_z;
                side = 1;
            }
            
            if (map_x < 0 || map_x >= MAP_W || map_z < 0 || map_z >= MAP_D) break;
            
            /* Calculate distance to this cell */
            if (side == 0) dist = side_dist_x - delta_dist_x;
            else           dist = side_dist_z - delta_dist_z;
            
            if (dist <= 0) dist = 1;
            
            /* Fix fisheye */
            int ca = (ray_angle - player_angle) % 360;
            if (ca < 0) ca += 360;
            int perp_dist = fix_mul(dist, fix_cos_table[ca]);
            if (perp_dist <= 0) perp_dist = 1;
            
            /* Check blocks in this column from top to bottom */
            for (int y = MAP_H - 1; y >= 0; y--) {
                uint8_t block = world[map_x][y][map_z];
                if (block != B_AIR) {
                    /* Project top and bottom of the block */
                    int block_top = TO_FIX(y + 1);
                    int block_bot = TO_FIX(y);
                    
                    int h_top = fix_div(block_top - player_y, perp_dist);
                    int h_bot = fix_div(block_bot - player_y, perp_dist);
                    
                    int draw_y_top = (SCREEN_H / 2) + player_pitch - FROM_FIX(h_top * FOV);
                    int draw_y_bot = (SCREEN_H / 2) + player_pitch - FROM_FIX(h_bot * FOV);
                    
                    if (draw_y_top < 0) draw_y_top = 0;
                    if (draw_y_bot >= SCREEN_H) draw_y_bot = SCREEN_H - 1;
                    
                    /* Occlusion culling */
                    if (draw_y_bot < y_bottom_limit || draw_y_top > y_top_limit) continue;
                    
                    uint16_t color = get_block_color(block, side);
                    
                    /* Draw vertical span */
                    for (int dy = draw_y_top; dy <= draw_y_bot; dy++) {
                        if (dy >= y_bottom_limit && dy < y_top_limit) {
                            buffer320[dy * SCREEN_W + x] = color;
                        }
                    }
                    
                    /* Update limits */
                    if (draw_y_top < y_top_limit) y_top_limit = draw_y_top;
                    
                    /* If we hit a block that occludes everything below, we stop down-casting */
                    if (y_top_limit <= y_bottom_limit) break;
                }
            }
            hit++;
        }
    }
    
    /* Now scale buffer320 to the actual window */
    extern window_t *window_list;
    window_t *curr = window_list;
    int cx = 0, cy = 0;
    while (curr) {
        extern void craft_update_window(struct window *, int);
        if (curr->update_client == (void*)craft_update_window) {
            cx = curr->x; cy = curr->y;
            break;
        }
        curr = curr->next;
    }
    
    uint16_t* vram = fb_get_buffer();
    if (vram) {
        for (int y = 0; y < SCREEN_H; y++) {
            int dest_y1 = cy + 24 + y * 2; /* 24 for titlebar */
            int dest_y2 = dest_y1 + 1;
            if (dest_y1 >= 480) break;
            
            uint16_t *row1 = vram + dest_y1 * 640;
            uint16_t *row2 = vram + dest_y2 * 640;
            
            for (int x = 0; x < SCREEN_W; x++) {
                int dest_x1 = cx + x * 2;
                int dest_x2 = dest_x1 + 1;
                if (dest_x1 >= 640) continue;
                
                uint16_t c = buffer320[y * SCREEN_W + x];
                row1[dest_x1] = c; row1[dest_x2] = c;
                row2[dest_x1] = c; row2[dest_x2] = c;
            }
        }
    }
    
    extern void font8x16_draw_string(int x, int y, const char *str, uint16_t color);
    craft_draw_text(cx + 10, cy + 30, "STAX Craft", 65535);
    craft_draw_text(cx + 10, cy + 50, "[W/A/S/D] Move | [< >] Rotate | [Space] Place | [Shift] Break", 65535);
}

static void craft_draw_text(int x_pos, int y_pos, const char *str, uint16_t color) {
    extern const unsigned char font8x16_data[256][16];
    extern uint16_t* fb_get_buffer(void);
    uint16_t* vram = fb_get_buffer();
    if (!vram) return;
    
    int xp = x_pos;
    const char *s = str;
    while (*s) {
        unsigned char c = *s++;
        for (int r = 0; r < 16; r++) {
            unsigned char bits = font8x16_data[c][r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80u >> b)) {
                    int draw_x = xp + b;
                    int draw_y = y_pos + r;
                    if (draw_x >= 0 && draw_x < 640 && draw_y >= 0 && draw_y < 480) {
                        vram[draw_y * 640 + draw_x] = color;
                    }
                }
            }
        }
        xp += 8;
    }
}


static void craft_update(int dt_ms) {
    (void)dt_ms;
    if (c_state == 1) return;
    
    int speed = TO_FIX(1) / 4; 
    int r_cos = fix_cos_table[player_angle];
    int r_sin = fix_sin_table[player_angle];
    
    if (kb_is_pressed('w')) {
        player_x += fix_mul(speed, r_cos);
        player_z += fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('s')) {
        player_x -= fix_mul(speed, r_cos);
        player_z -= fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('a')) {
        player_x += fix_mul(speed, r_sin);
        player_z -= fix_mul(speed, r_cos);
    }
    if (kb_is_pressed('d')) {
        player_x -= fix_mul(speed, r_sin);
        player_z += fix_mul(speed, r_cos);
    }
    if (kb_is_pressed(0x4F)) { /* Right arrow */
        player_angle = (player_angle + 5) % 360;
    }
    if (kb_is_pressed(0x50)) { /* Left arrow */
        player_angle = (player_angle - 5 + 360) % 360;
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
    (void)win; (void)cx; (void)cy; (void)cw; (void)ch;
    craft_draw();
}

static void craft_key_event(struct window *win, char c) {
    (void)win;
    /* Interact with blocks (simple raycast to find block ahead) */
    if (c == ' ' || c == 'R') {
        /* Simple place block */
        int map_x = FROM_FIX(player_x + fix_mul(TO_FIX(2), fix_cos_table[player_angle]));
        int map_z = FROM_FIX(player_z + fix_mul(TO_FIX(2), fix_sin_table[player_angle]));
        int map_y = FROM_FIX(player_y);
        
        if (map_x >= 0 && map_x < MAP_W && map_z >= 0 && map_z < MAP_D && map_y >= 0 && map_y < MAP_H) {
            if (c == ' ') world[map_x][map_y][map_z] = B_WOOD; /* Place */
            if (c == 'R' || c == 'r') world[map_x][map_y][map_z] = B_AIR; /* Break */
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
            return;
        }
        curr = curr->next;
    }
    
    craft_init();
    
    window_t *win = wm_add_window(0, 0, 640, 424, "STAX Craft 3D", craft_draw_window_cb);
    if (win) {
        win->update_client = craft_update_window;
        win->key_event = craft_key_event;
    }
}
