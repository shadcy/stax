/* ============================================================================
 * TIOS — doom.c
 * Bare minimum ASCII DOOM-like raycasting game
 * 
 * Controls:
 *   W/S - Move forward/backward
 *   A/D - Rotate left/right
 *   Q   - Quit game
 * 
 * This is a simplified raycasting engine that renders a 3D-like view
 * using ASCII characters in the console.
 * ============================================================================ */

#include "doom.h"
#include "console.h"

/* Screen dimensions */
#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 20

/* Map dimensions */
#define MAP_WIDTH  8
#define MAP_HEIGHT 8

/* Fixed-point math (to avoid floating point) */
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

/* Player state */
static int player_x;  /* Fixed-point */
static int player_y;  /* Fixed-point */
static int player_angle; /* 0-359 degrees */

/* Simple map (1 = wall, 0 = empty) */
static const char map[MAP_HEIGHT][MAP_WIDTH] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 1, 1, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 0, 1, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1}
};

/* Simple sine/cosine lookup tables (0-359 degrees) */
static int sin_table[360];
static int cos_table[360];

/* Simple integer division helper */
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
    /* Approximate sine/cosine using Taylor series (first few terms) */
    /* For simplicity, we'll use a rough approximation */
    for (int i = 0; i < 360; i++) {
        /* Convert to radians-ish (scaled) */
        int angle = i;
        
        /* Very rough sine/cosine approximation */
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
            /* First quadrant */
            sin_table[i] = idiv(angle * FIXED_ONE, 90);
            cos_table[i] = FIXED_ONE - idiv(angle * FIXED_ONE, 90);
        } else if (angle < 180) {
            /* Second quadrant */
            int a = angle - 90;
            sin_table[i] = FIXED_ONE - idiv(a * FIXED_ONE, 90);
            cos_table[i] = -idiv(a * FIXED_ONE, 90);
        } else if (angle < 270) {
            /* Third quadrant */
            int a = angle - 180;
            sin_table[i] = -idiv(a * FIXED_ONE, 90);
            cos_table[i] = -FIXED_ONE + idiv(a * FIXED_ONE, 90);
        } else {
            /* Fourth quadrant */
            int a = angle - 270;
            sin_table[i] = -FIXED_ONE + idiv(a * FIXED_ONE, 90);
            cos_table[i] = idiv(a * FIXED_ONE, 90);
        }
    }
}

/* Get sine value */
static int get_sin(int angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return sin_table[angle];
}

/* Get cosine value */
static int get_cos(int angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return cos_table[angle];
}

/* Check if position is valid (not in wall) */
static int is_valid_pos(int x, int y)
{
    int map_x = x >> FIXED_SHIFT;
    int map_y = y >> FIXED_SHIFT;
    
    if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT)
        return 0;
    
    return map[map_y][map_x] == 0;
}

/* Cast a ray and return distance to wall */
static int cast_ray(int angle)
{
    int ray_x = player_x;
    int ray_y = player_y;
    int dx = idiv(get_cos(angle), 4);  /* Step size */
    int dy = idiv(get_sin(angle), 4);
    
    /* March ray until we hit a wall */
    for (int i = 0; i < 64; i++) {
        ray_x += dx;
        ray_y += dy;
        
        int map_x = ray_x >> FIXED_SHIFT;
        int map_y = ray_y >> FIXED_SHIFT;
        
        if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT)
            return i;
        
        if (map[map_y][map_x] == 1)
            return i;
    }
    
    return 64;
}

/* Render the 3D view */
static void render_view(void)
{
    char screen[SCREEN_HEIGHT][SCREEN_WIDTH + 1];
    
    /* Clear screen buffer */
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x] = ' ';
        }
        screen[y][SCREEN_WIDTH] = '\0';
    }
    
    /* Cast rays for each column */
    int fov = 60;  /* Field of view */
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int ray_angle = player_angle - idiv(fov, 2) + idiv(x * fov, SCREEN_WIDTH);
        int distance = cast_ray(ray_angle);
        
        /* Calculate wall height based on distance */
        int wall_height = idiv(SCREEN_HEIGHT * 8, distance + 1);
        if (wall_height > SCREEN_HEIGHT) wall_height = SCREEN_HEIGHT;
        
        /* Draw vertical line */
        int start = idiv(SCREEN_HEIGHT - wall_height, 2);
        int end = start + wall_height;
        
        /* Choose character based on distance */
        char wall_char;
        if (distance < 8) wall_char = '#';
        else if (distance < 16) wall_char = '@';
        else if (distance < 24) wall_char = '%';
        else if (distance < 32) wall_char = '+';
        else if (distance < 40) wall_char = '=';
        else wall_char = '.';
        
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            if (y < start) {
                screen[y][x] = ' ';  /* Ceiling */
            } else if (y >= start && y < end) {
                screen[y][x] = wall_char;  /* Wall */
            } else {
                screen[y][x] = '.';  /* Floor */
            }
        }
    }
    
    /* Clear screen and render */
    kputs("\033[2J\033[H");  /* ANSI clear and home */
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        kputs(screen[y]);
        kputc('\n');
    }
    
    /* Show controls */
    kputs("----------------------------------------\n");
    kputs("DOOM | W/S:Move A/D:Turn Q:Quit\n");
}

/* Render mini-map */
static void render_minimap(void)
{
    kputs("\nMini-map:\n");
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int px = player_x >> FIXED_SHIFT;
            int py = player_y >> FIXED_SHIFT;
            
            if (x == px && y == py) {
                kputc('P');  /* Player */
            } else if (map[y][x] == 1) {
                kputc('#');  /* Wall */
            } else {
                kputc(' ');  /* Empty */
            }
        }
        kputc('\n');
    }
}

/* Main game loop */
void doom_run(void)
{
    /* Initialize */
    init_trig_tables();
    
    /* Set starting position */
    player_x = (3 << FIXED_SHIFT) + idiv(FIXED_ONE, 2);
    player_y = (3 << FIXED_SHIFT) + idiv(FIXED_ONE, 2);
    player_angle = 0;
    
    kputs("Loading DOOM...\n");
    kputs("Initializing raycaster...\n");
    
    /* Small delay */
    for (volatile int i = 0; i < 1000000; i++);
    
    int running = 1;
    while (running) {
        /* Render current view */
        render_view();
        
        /* Get input */
        char c = 0;
        int timeout = 100000;
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
                /* Move forward */
                new_x += idiv(get_cos(player_angle), 2);
                new_y += idiv(get_sin(player_angle), 2);
                if (is_valid_pos(new_x, new_y)) {
                    player_x = new_x;
                    player_y = new_y;
                }
                break;
                
            case 's':
            case 'S':
                /* Move backward */
                new_x -= idiv(get_cos(player_angle), 2);
                new_y -= idiv(get_sin(player_angle), 2);
                if (is_valid_pos(new_x, new_y)) {
                    player_x = new_x;
                    player_y = new_y;
                }
                break;
                
            case 'a':
            case 'A':
                /* Rotate left */
                player_angle -= 15;
                if (player_angle < 0) player_angle += 360;
                break;
                
            case 'd':
            case 'D':
                /* Rotate right */
                player_angle += 15;
                if (player_angle >= 360) player_angle -= 360;
                break;
                
            case 'q':
            case 'Q':
                /* Quit */
                running = 0;
                break;
                
            case 'm':
            case 'M':
                /* Show mini-map */
                render_minimap();
                /* Wait for key */
                while (kgetc() == 0);
                break;
        }
    }
    
    /* Clean up */
    kputs("\033[2J\033[H");
    kputs("Thanks for playing DOOM!\n");
}
