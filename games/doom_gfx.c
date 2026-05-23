/* ============================================================================
 * TIOS — doom_gfx.c
 * Graphical DOOM-like raycasting game using framebuffer
 * 
 * Controls:
 *   W/S - Move forward/backward
 *   A/D - Rotate left/right
 *   Q   - Quit game
 * ============================================================================ */

#include "doom.h"
#include "framebuffer.h"
#include "console.h"

/* Screen dimensions */
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

/* Map dimensions */
#define MAP_WIDTH  16
#define MAP_HEIGHT 16

/* Fixed-point math */
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

/* Player state */
static int player_x;
static int player_y;
static int player_angle;

/* Larger, more interesting map */
static const char map[MAP_HEIGHT][MAP_WIDTH] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

/* Trig tables */
static int sin_table[360];
static int cos_table[360];

/* Integer division */
static int idiv(int a, int b)
{
    if (b == 0) return 0;
    int neg = 0;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }
    
    int result = 0;
    while (a >= b) {
        a -= b;
        result++;
    }
    return neg ? -result : result;
}

/* Initialize trig tables */
static void init_trig_tables(void)
{
    for (int i = 0; i < 360; i++) {
        int angle = i;
        
        if (angle == 0) {
            sin_table[i] = 0;
            cos_table[i] = FIXED_ONE;
        } else if (angle == 90) {
            sin_table[i] = FIXED_ONE;
            cos_table[i] = 0;
        } else if (angle == 180) {
            sin_table[i] = 0;
            cos_table[i] = -FIXED_ONE;
        } else if (angle == 270) {
            sin_table[i] = -FIXED_ONE;
            cos_table[i] = 0;
        } else if (angle < 90) {
            sin_table[i] = idiv(angle * FIXED_ONE, 90);
            cos_table[i] = FIXED_ONE - idiv(angle * FIXED_ONE, 90);
        } else if (angle < 180) {
            int a = angle - 90;
            sin_table[i] = FIXED_ONE - idiv(a * FIXED_ONE, 90);
            cos_table[i] = -idiv(a * FIXED_ONE, 90);
        } else if (angle < 270) {
            int a = angle - 180;
            sin_table[i] = -idiv(a * FIXED_ONE, 90);
            cos_table[i] = -FIXED_ONE + idiv(a * FIXED_ONE, 90);
        } else {
            int a = angle - 270;
            sin_table[i] = -FIXED_ONE + idiv(a * FIXED_ONE, 90);
            cos_table[i] = idiv(a * FIXED_ONE, 90);
        }
    }
}

static int get_sin(int angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return sin_table[angle];
}

static int get_cos(int angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return cos_table[angle];
}

/* Check if position is valid */
static int is_valid_pos(int x, int y)
{
    int map_x = x >> FIXED_SHIFT;
    int map_y = y >> FIXED_SHIFT;
    
    if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT)
        return 0;
    
    return map[map_y][map_x] == 0;
}

/* Cast a ray and return distance */
static int cast_ray(int angle, int *hit_side)
{
    int ray_x = player_x;
    int ray_y = player_y;
    int dx = idiv(get_cos(angle), 8);
    int dy = idiv(get_sin(angle), 8);
    
    for (int i = 0; i < 128; i++) {
        ray_x += dx;
        ray_y += dy;
        
        int map_x = ray_x >> FIXED_SHIFT;
        int map_y = ray_y >> FIXED_SHIFT;
        
        if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT) {
            *hit_side = 0;
            return i;
        }
        
        if (map[map_y][map_x] == 1) {
            /* Determine which side was hit */
            int prev_x = (ray_x - dx) >> FIXED_SHIFT;
            int prev_y = (ray_y - dy) >> FIXED_SHIFT;
            *hit_side = (prev_x != map_x) ? 0 : 1;  /* 0=vertical, 1=horizontal */
            return i;
        }
    }
    
    *hit_side = 0;
    return 128;
}

/* Render 3D view */
static void render_view(void)
{
    uint16_t *fb = fb_get_buffer();
    
    /* Draw ceiling and floor */
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        uint16_t color = (y < idiv(SCREEN_HEIGHT, 2)) ? rgb565(64, 64, 64) : rgb565(32, 32, 32);
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            fb[y * SCREEN_WIDTH + x] = color;
        }
    }
    
    /* Cast rays for each column */
    int fov = 60;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int ray_angle = player_angle - idiv(fov, 2) + idiv(x * fov, SCREEN_WIDTH);
        int hit_side;
        int distance = cast_ray(ray_angle, &hit_side);
        
        /* Calculate wall height */
        int wall_height = idiv(SCREEN_HEIGHT * 16, distance + 1);
        if (wall_height > SCREEN_HEIGHT) wall_height = SCREEN_HEIGHT;
        
        int start = idiv(SCREEN_HEIGHT - wall_height, 2);
        int end = start + wall_height;
        
        /* Choose color based on distance and side */
        uint16_t wall_color;
        if (distance < 16) {
            wall_color = hit_side ? rgb565(200, 0, 0) : rgb565(255, 0, 0);
        } else if (distance < 32) {
            wall_color = hit_side ? rgb565(150, 0, 0) : rgb565(200, 0, 0);
        } else if (distance < 48) {
            wall_color = hit_side ? rgb565(100, 0, 0) : rgb565(150, 0, 0);
        } else if (distance < 64) {
            wall_color = hit_side ? rgb565(50, 0, 0) : rgb565(100, 0, 0);
        } else {
            wall_color = hit_side ? rgb565(25, 0, 0) : rgb565(50, 0, 0);
        }
        
        /* Draw wall column */
        for (int y = start; y < end && y < SCREEN_HEIGHT; y++) {
            if (y >= 0) {
                fb[y * SCREEN_WIDTH + x] = wall_color;
            }
        }
    }
    
    /* Draw simple HUD */
    fb_fillrect(10, 10, 100, 20, COLOR_BLACK);
    /* Could add text here if we had a font */
}

/* Main graphical DOOM */
void doom_gfx_run(void)
{
    kputs("Initializing graphical DOOM...\n");
    
    /* Initialize framebuffer */
    if (fb_init() != 0) {
        kputs("Failed to initialize framebuffer!\n");
        return;
    }
    
    /* Initialize trig tables */
    init_trig_tables();
    
    /* Set starting position */
    player_x = (8 << FIXED_SHIFT);
    player_y = (8 << FIXED_SHIFT);
    player_angle = 0;
    
    /* Draw loading screen */
    fb_clear(COLOR_BLACK);
    fb_fillrect(200, 200, 240, 80, COLOR_RED);
    
    kputs("DOOM initialized. Use W/A/S/D to move, Q to quit\n");
    
    /* Small delay */
    for (volatile int i = 0; i < 5000000; i++);
    
    int running = 1;
    while (running) {
        /* Render current view */
        render_view();
        
        /* Get input */
        char c = 0;
        int timeout = 50000;
        while (timeout-- > 0 && c == 0) {
            c = kgetc();
        }
        
        if (c == 0) continue;
        
        /* Process input */
        int new_x = player_x;
        int new_y = player_y;
        
        switch (c) {
            case 'w':
            case 'W':
                new_x += idiv(get_cos(player_angle), 2);
                new_y += idiv(get_sin(player_angle), 2);
                if (is_valid_pos(new_x, new_y)) {
                    player_x = new_x;
                    player_y = new_y;
                }
                break;
                
            case 's':
            case 'S':
                new_x -= idiv(get_cos(player_angle), 2);
                new_y -= idiv(get_sin(player_angle), 2);
                if (is_valid_pos(new_x, new_y)) {
                    player_x = new_x;
                    player_y = new_y;
                }
                break;
                
            case 'a':
            case 'A':
                player_angle -= 10;
                if (player_angle < 0) player_angle += 360;
                break;
                
            case 'd':
            case 'D':
                player_angle += 10;
                if (player_angle >= 360) player_angle -= 360;
                break;
                
            case 'q':
            case 'Q':
                running = 0;
                break;
        }
    }
    
    /* Clean up */
    fb_clear(COLOR_BLACK);
    kputs("Thanks for playing graphical DOOM!\n");
}
