/* ============================================================================
 * TIOS — mouse.c
 * PL050 KMI1 PS/2 mouse driver
 * ============================================================================ */

#include "mouse.h"
#include "framebuffer.h" /* For FB_WIDTH and FB_HEIGHT */

#define KMI1_BASE 0x10007000u
#define KMI_CR   (*(volatile uint32_t*)(KMI1_BASE + 0x00))
#define KMI_STAT (*(volatile uint32_t*)(KMI1_BASE + 0x04))
#define KMI_DATA (*(volatile uint32_t*)(KMI1_BASE + 0x08))

#define KMI_CR_EN      (1u << 2)
#define KMI_CR_TXEN    (1u << 3)
#define KMI_STAT_RXFULL (1u << 4)
#define KMI_STAT_TXEMPTY (1u << 6)

volatile int mouse_x = FB_WIDTH / 2;
volatile int mouse_y = FB_HEIGHT / 2;
volatile int mouse_buttons = 0;
volatile int mouse_changed = 0;

static int mouse_cycle = 0;
static uint8_t mouse_byte[3];

static void mouse_write(uint8_t data) {
    while ((KMI_STAT & KMI_STAT_TXEMPTY) == 0);
    KMI_DATA = data;
}

static uint8_t mouse_read(void) {
    while ((KMI_STAT & KMI_STAT_RXFULL) == 0);
    return KMI_DATA & 0xFF;
}

void mouse_init(void) {
    /* Enable KMI and TX */
    KMI_CR = KMI_CR_EN | KMI_CR_TXEN;
    
    /* Send 'Enable Data Reporting' command */
    mouse_write(0xF4);
    
    /* Read ACK */
    mouse_read();
    
    mouse_cycle = 0;
}

void mouse_poll(void) {
    while (KMI_STAT & KMI_STAT_RXFULL) {
        uint8_t data = KMI_DATA & 0xFF;
        
        switch (mouse_cycle) {
            case 0:
                /* Bit 3 of byte 0 is typically 1. Used for synchronization. */
                if (data & 0x08) {
                    mouse_byte[0] = data;
                    mouse_cycle++;
                }
                break;
            case 1:
                mouse_byte[1] = data;
                mouse_cycle++;
                break;
            case 2:
                mouse_byte[2] = data;
                mouse_cycle = 0;
                
                int rel_x = mouse_byte[1];
                int rel_y = mouse_byte[2];
                
                /* Handle sign extensions */
                if (mouse_byte[0] & 0x10) rel_x -= 256;
                if (mouse_byte[0] & 0x20) rel_y -= 256;
                
                /* Apply 2x multiplier to help reach edges when QEMU host captures mouse */
                mouse_x += (rel_x * 2);
                mouse_y -= (rel_y * 2); /* PS/2 y is up */
                
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= FB_WIDTH) mouse_x = FB_WIDTH - 1;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= FB_HEIGHT) mouse_y = FB_HEIGHT - 1;
                
                mouse_buttons = mouse_byte[0] & 0x07;
                mouse_changed = 1;
                break;
        }
    }
}
