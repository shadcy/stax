#include "engine2d.h"
#include "framebuffer.h"
#include "keyboard.h"

extern volatile unsigned int tick_count; // From kernel.c (1 tick = 1ms)

void engine2d_run(EngineApp *app)
{
    if (!app) return;
    
    /* Initialize the game */
    if (app->init) app->init();
    
    /* Enable double buffering */
    fb_set_double_buffering(1);
    
    unsigned int last_tick = tick_count;
    
    /* Target FPS: ~50 -> 20ms per frame.
     * We wait until tick_count advances by 20. */
     
    while (1) {
        /* Check for exit */
        if (kb_is_pressed('q') || kb_is_pressed('Q')) {
            break; /* Exit engine */
        }
        
        unsigned int current_tick = tick_count;
        int dt_ms = current_tick - last_tick;
        
        /* Cap framerate (50 FPS = 20ms) */
        if (dt_ms < 20) {
            asm volatile ("mcr p15, 0, %0, c7, c0, 4" : : "r" (0)); /* Yield to emulator */
            continue; /* Wait for next frame */
        }
        
        last_tick = current_tick;
        
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
