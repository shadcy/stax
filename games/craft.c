#include "craft.h"
#include "wm.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "font8x16.h"
#include "string.h"
#include "math_fixed.h"

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
static int player_vel_y = 0;
static int is_grounded = 0;

/* Mouse state */
static int last_mx = -1;
static int last_my = -1;

/* Settings & Features */
static int mouse_sens = 5;
static uint8_t active_block = B_WOOD;
static int menu_active = 0; /* 0 = game, 1 = settings */

/* Rendering constants */
#define SCREEN_W 320
#define SCREEN_H 200
#define FOV      256

/* Precomputed sky/floor colors (constant, no per-frame call overhead) */
#define COLOR_SKY   ((uint16_t)((135 >> 3) << 11 | (206 >> 2) << 5 | (235 >> 3)))
#define COLOR_FLOOR ((uint16_t)((60 >> 3) << 11  | (60 >> 2) << 5  | (60 >> 3)))

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
    menu_active = 0;
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
    
    static uint16_t buffer_cols[SCREEN_W][SCREEN_H];
    
    /* Draw sky and ground — hoist horizon out, use precomputed colors */
    int horizon = SCREEN_H/2 + player_pitch;
    if (horizon < 0) horizon = 0;
    if (horizon > SCREEN_H) horizon = SCREEN_H;
    for (int x = 0; x < SCREEN_W; x++) {
        uint16_t *col = buffer_cols[x];
        for (int y = 0; y < horizon; y++)  col[y] = COLOR_SKY;
        for (int y = horizon; y < SCREEN_H; y++) col[y] = COLOR_FLOOR;
    }
    
    /* Raycaster for 3D grid */
    for (int x = 0; x < SCREEN_W; x++) {
        int a_offset = (x - (SCREEN_W / 2)) * 60 / SCREEN_W; /* ~60 deg FOV */
        int ray_angle = (player_angle + a_offset) % 360;
        if (ray_angle < 0) ray_angle += 360;
        
        int r_cos = fix_cos_table[ray_angle];
        int r_sin = fix_sin_table[ray_angle];
        
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
        
        int y_top_limit = SCREEN_H;
        int y_bottom_limit = 0;
        
        while (hit < 16) { 
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
            
            if (side == 0) dist = side_dist_x - delta_dist_x;
            else           dist = side_dist_z - delta_dist_z;
            
            if (dist <= 0) dist = 1;
            
            int ca = (ray_angle - player_angle) % 360;
            if (ca < 0) ca += 360;
            int perp_dist = fix_mul(dist, fix_cos_table[ca]);
            if (perp_dist <= 0) perp_dist = 1;
            
            int inv_perp_dist = fix_div(TO_FIX(1), perp_dist);
            
            for (int y = MAP_H - 1; y >= 0; y--) {
                uint8_t block = world[map_x][y][map_z];
                if (block != B_AIR) {
                    int block_top = TO_FIX(y + 1);
                    int block_bot = TO_FIX(y);
                    
                    int h_top = fix_mul(block_top - player_y, inv_perp_dist);
                    int h_bot = fix_mul(block_bot - player_y, inv_perp_dist);
                    
                    int draw_y_top = (SCREEN_H / 2) + player_pitch - FROM_FIX(h_top * FOV);
                    int draw_y_bot = (SCREEN_H / 2) + player_pitch - FROM_FIX(h_bot * FOV);
                    
                    if (draw_y_top < 0) draw_y_top = 0;
                    if (draw_y_bot >= SCREEN_H) draw_y_bot = SCREEN_H - 1;
                    
                    if (draw_y_bot < y_bottom_limit || draw_y_top > y_top_limit) continue;
                    
                    uint16_t color = get_block_color(block, side);
                    
                    int draw_start = (draw_y_top > y_bottom_limit) ? draw_y_top : y_bottom_limit;
                    int draw_end = (draw_y_bot < y_top_limit - 1) ? draw_y_bot : y_top_limit - 1;
                    
                    for (int dy = draw_start; dy <= draw_end; dy++) {
                        buffer_cols[x][dy] = color;
                    }
                    
                    if (draw_y_top < y_top_limit) y_top_limit = draw_y_top;
                    if (y_top_limit <= y_bottom_limit) break;
                }
            }
            hit++;
        }
    }
    
    /* Draw Crosshair (in buffer320 space before upscaling) */
    int cx_screen = SCREEN_W / 2;
    int cy_screen = SCREEN_H / 2;
    for (int i = -3; i <= 3; i++) {
        if (cx_screen + i >= 0 && cx_screen + i < SCREEN_W) buffer_cols[cx_screen + i][cy_screen] = 65535;
        if (cy_screen + i >= 0 && cy_screen + i < SCREEN_H) buffer_cols[cx_screen][cy_screen + i] = 65535;
    }
    
    /* Now scale buffer_cols to the actual window */
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
                
                uint16_t c = buffer_cols[x][y];
                row1[dest_x1] = c; row1[dest_x2] = c;
                row2[dest_x1] = c; row2[dest_x2] = c;
            }
        }
        
        /* Draw Settings overlay if active */
        if (menu_active) {
            /* Dim screen slightly by drawing semi-transparent black boxes, or just a big rectangle */
            for (int y = 0; y < 150; y++) {
                int draw_y = cy + 24 + 50 + y;
                if (draw_y >= 480) break;
                for (int x = 0; x < 200; x++) {
                    int draw_x = cx + 220 + x;
                    if (draw_x >= 640) continue;
                    vram[draw_y * 640 + draw_x] = 0x0000;
                }
            }
        }
    }
    
    if (menu_active) {
        craft_draw_text(cx + 230, cy + 24 + 60, "--- SETTINGS ---", 65535);
        char buf[64];
        buf[0] = 'S'; buf[1] = 'e'; buf[2] = 'n'; buf[3] = 's'; buf[4] = ':'; buf[5] = ' ';
        buf[6] = (mouse_sens / 10) + '0';
        buf[7] = (mouse_sens % 10) + '0';
        buf[8] = '\0';
        craft_draw_text(cx + 230, cy + 24 + 90, buf, 65535);
        craft_draw_text(cx + 230, cy + 24 + 110, "[+] Increase", rgb565(0,255,0));
        craft_draw_text(cx + 230, cy + 24 + 130, "[-] Decrease", rgb565(255,0,0));
        craft_draw_text(cx + 230, cy + 24 + 160, "[ESC] Resume", 65535);
    } else {
        craft_draw_text(cx + 10, cy + 30, "STAX Craft", 65535);
        const char *bname = "Wood";
        if (active_block == B_GRASS) bname = "Grass";
        if (active_block == B_DIRT) bname = "Dirt";
        if (active_block == B_STONE) bname = "Stone";
        char buf[64];
        buf[0] = 'B'; buf[1] = 'l'; buf[2] = 'o'; buf[3] = 'c'; buf[4] = 'k'; buf[5] = ':'; buf[6] = ' '; buf[7] = '\0';
        char *d = buf + 7;
        while (*bname) *d++ = *bname++;
        *d = '\0';
        
        craft_draw_text(cx + 10, cy + 50, buf, 65535);
        craft_draw_text(cx + 10, cy + 70, "[Drag] Look | [L/R Click] Build", rgb565(200, 200, 200));
        craft_draw_text(cx + 10, cy + 90, "[Space] Jump | [1-4] Select | [ESC] Menu", rgb565(200, 200, 200));
    }
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

/* Collision Check */
static int is_solid(int x, int y, int z) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || z < 0 || z >= MAP_D) return 1; /* Boundaries are solid */
    return world[x][y][z] != B_AIR;
}

static void craft_update(int dt_ms) {
    (void)dt_ms;
    if (menu_active) return;
    
    int speed = TO_FIX(1) / 4; 
    int r_cos = fix_cos_table[player_angle];
    int r_sin = fix_sin_table[player_angle];
    
    int next_x = player_x;
    int next_z = player_z;
    
    if (kb_is_pressed('w')) {
        next_x += fix_mul(speed, r_cos);
        next_z += fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('s')) {
        next_x -= fix_mul(speed, r_cos);
        next_z -= fix_mul(speed, r_sin);
    }
    if (kb_is_pressed('a')) {
        next_x += fix_mul(speed, r_sin);
        next_z -= fix_mul(speed, r_cos);
    }
    if (kb_is_pressed('d')) {
        next_x -= fix_mul(speed, r_sin);
        next_z += fix_mul(speed, r_cos);
    }
    if (kb_is_pressed(KB_RIGHT)) { /* Right arrow */
        player_angle = (player_angle + 5) % 360;
    }
    if (kb_is_pressed(KB_LEFT)) { /* Left arrow */
        player_angle = (player_angle - 5 + 360) % 360;
    }
    if (kb_is_pressed(KB_UP)) { /* Look Up */
        player_pitch += 10;
        if (player_pitch > 100) player_pitch = 100;
    }
    if (kb_is_pressed(KB_DOWN)) { /* Look Down */
        player_pitch -= 10;
        if (player_pitch < -100) player_pitch = -100;
    }
    
    /* Jump */
    if (kb_is_pressed(' ') && is_grounded) {
        player_vel_y = TO_FIX(1) / 3; /* Jump strength */
        is_grounded = 0;
    }
    
    /* Gravity */
    player_vel_y -= TO_FIX(1) / 30; /* Gravity */
    if (player_vel_y < -TO_FIX(1)) player_vel_y = -TO_FIX(1); /* Terminal velocity */
    
    int next_y = player_y + player_vel_y;
    
    /* XZ Collision */
    int px = FROM_FIX(next_x);
    int pz = FROM_FIX(next_z);
    int py = FROM_FIX(player_y);
    int py_feet = FROM_FIX(player_y - TO_FIX(1)/2);
    
    if (!is_solid(px, py, pz) && !is_solid(px, py_feet, pz)) {
        player_x = next_x;
        player_z = next_z;
    }
    
    /* Y Collision */
    px = FROM_FIX(player_x);
    pz = FROM_FIX(player_z);
    int ny_head = FROM_FIX(next_y + TO_FIX(1)/4);
    int ny_feet = FROM_FIX(next_y - TO_FIX(1)/2);
    
    if (player_vel_y <= 0) { /* Falling or grounded */
        if (is_solid(px, ny_feet, pz)) {
            player_y = TO_FIX(ny_feet + 1) + TO_FIX(1)/2;
            player_vel_y = 0;
            is_grounded = 1;
        } else {
            player_y = next_y;
            is_grounded = 0;
        }
    } else if (player_vel_y > 0) { /* Jumping */
        if (is_solid(px, ny_head, pz)) {
            player_y = TO_FIX(ny_head) - TO_FIX(1)/4;
            player_vel_y = 0;
        } else {
            player_y = next_y;
        }
    }
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
    if (c == 27) { /* ESC */
        menu_active = !menu_active;
        if (menu_active) last_mx = -1; /* reset mouse on pause */
    }
    
    if (menu_active) {
        if (c == '=' || c == '+') {
            mouse_sens++;
            if (mouse_sens > 20) mouse_sens = 20;
        }
        if (c == '-' || c == '_') {
            mouse_sens--;
            if (mouse_sens < 1) mouse_sens = 1;
        }
        return;
    }
    
    if (c == '1') active_block = B_WOOD;
    if (c == '2') active_block = B_STONE;
    if (c == '3') active_block = B_DIRT;
    if (c == '4') active_block = B_GRASS;
}

/* Mouse click to place/break */
static void craft_mouse_click(struct window *win, int mx, int my, int button) {
    (void)win; (void)mx; (void)my;
    if (menu_active) return;
    
    /* Raycast to find block ahead */
    int map_x = FROM_FIX(player_x + fix_mul(TO_FIX(2), fix_cos_table[player_angle]));
    int map_z = FROM_FIX(player_z + fix_mul(TO_FIX(2), fix_sin_table[player_angle]));
    int map_y = FROM_FIX(player_y);
    
    if (map_x >= 0 && map_x < MAP_W && map_z >= 0 && map_z < MAP_D && map_y >= 0 && map_y < MAP_H) {
        if (button == 1) world[map_x][map_y][map_z] = B_AIR; /* Left click -> Break */
        if (button == 2) world[map_x][map_y][map_z] = active_block; /* Right click -> Place */
    }
}

/* Mouse drag to look around */
static void craft_mouse_drag(struct window *win, int mx, int my) {
    (void)win;
    if (menu_active) return;
    
    if (last_mx == -1) {
        last_mx = mx;
        last_my = my;
        return;
    }
    
    int dx = mx - last_mx;
    int dy = my - last_my;
    
    if (iabs(dx) > 100 || iabs(dy) > 100) {
        last_mx = mx;
        last_my = my;
        return;
    }
    
    /* Map delta to rotation */
    int yaw_change = (dx * mouse_sens) / 2;
    int pitch_change = (dy * mouse_sens) / 2;
    
    player_angle = (player_angle + yaw_change) % 360;
    if (player_angle < 0) player_angle += 360;
    
    player_pitch -= pitch_change; /* Inverted Y look */
    if (player_pitch > 100) player_pitch = 100;
    if (player_pitch < -100) player_pitch = -100;
    
    last_mx = mx;
    last_my = my;
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
        win->mouse_click = craft_mouse_click;
        win->mouse_drag = craft_mouse_drag;
    }
}
