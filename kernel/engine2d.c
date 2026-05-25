#include "engine2d.h"
#include "framebuffer.h"
#include "keyboard.h"

extern volatile unsigned int tick_count; // From kernel.c (1 tick = 10ms)

void engine2d_run(EngineApp *app)
{
    if (!app) return;
    
    /* Initialize the game */
    if (app->init) app->init();
    
    /* Enable double buffering */
    fb_set_double_buffering(1);
    
    unsigned int last_tick = tick_count;
    
    /* Target FPS: ~60 -> ~16.6ms per frame -> let's say 2 ticks (20ms) since our timer is 10ms.
     * We'll run at 50 FPS for simplicity, waiting until tick_count advances by 2.
     * Or better yet, wait until tick_count advances by at least 1, giving up to 100 FPS
     * but we don't have v-sync so we just limit it. */
     
    while (1) {
        /* Check for exit */
        if (kb_is_pressed('q') || kb_is_pressed('Q')) {
            break; /* Exit engine */
        }
        
        unsigned int current_tick = tick_count;
        int dt_ticks = current_tick - last_tick;
        
        /* Cap framerate (approx 50 FPS = 20ms = 2 ticks) */
        if (dt_ticks < 2) {
            continue; /* Wait for next frame */
        }
        
        last_tick = current_tick;
        int dt_ms = dt_ticks * 10;
        
        /* Update game logic */
        if (app->update) app->update(dt_ms);
        
        /* Render frame to backbuffer */
        if (app->draw) app->draw();
        
        /* Swap to front buffer */
        fb_swap();
    }
    
    /* Disable double buffering and return to OS */
    fb_set_double_buffering(0);
}
