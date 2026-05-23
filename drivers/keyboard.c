/* ============================================================================
 * TIOS — keyboard.c
 * PL050 KMI PS/2 keyboard driver (polling, scan-code set 2)
 *
 * VersatilePB keyboard KMI base: 0x10006000
 * Reference: ARM PL050 TRM, QEMU hw/input/pl050.c
 * ============================================================================ */

#include "keyboard.h"
#include <stdint.h>

/* ── PL050 register map ──────────────────────────────────────────────────── */
#define KMI_BASE    0x10006000u
#define KMI_CR      (*(volatile uint32_t *)(KMI_BASE + 0x00)) /* control      */
#define KMI_STAT    (*(volatile uint32_t *)(KMI_BASE + 0x04)) /* status       */
#define KMI_DATA    (*(volatile uint32_t *)(KMI_BASE + 0x08)) /* data         */

#define KMI_CR_EN   (1u << 2)   /* enable KMI                                */
#define KMI_STAT_RXFULL (1u << 4) /* RX data available                       */

/* ── PS/2 scan-code set 2 → ASCII (unshifted) ───────────────────────────── */
static const unsigned char sc2_normal[256] = {
/*00*/  0,    0,    0,    0,    0,    0,    0,    0,
/*08*/  0,    0,    0,    0,    0,   '\t',  '`',   0,
/*10*/  0,    0,    0,    0,    0,   'q',   '1',   0,
/*18*/  0,    0,   'z',  's',  'a',  'w',  '2',   0,
/*20*/  0,   'c',  'x',  'd',  'e',  '4',  '3',   0,
/*28*/  0,   ' ',  'v',  'f',  't',  'r',  '5',   0,
/*30*/  0,   'n',  'b',  'h',  'g',  'y',  '6',   0,
/*38*/  0,    0,   'm',  'j',  'u',  '7',  '8',   0,
/*40*/  0,   ',',  'k',  'i',  'o',  '0',  '9',   0,
/*48*/  0,   '.',  '/',  'l',  ';',  'p',  '-',   0,
/*50*/  0,    0,  '\'',  0,   '[',  '=',   0,    0,
/*58*/  0,    0,  '\n',  ']',   0,  '\\',  0,    0,
/*60*/  0,    0,    0,    0,    0,    0,  '\b',   0,
/*66*/ '\b',  0,    0,   '1',   0,   '4',  '7',   0,
/*6e*/  0,    0,   '0',  '.',  '2',  '5',  '6',  '8',
/*76*/'\x1b', 0,    0,   '+',  '3',  '-',  '*',  '9',
/*7e*/  0,    0,    0,    0,    0,    0,    0,    0,
/* 0x80-0xFF: not used for basic ASCII */
};

/* ── PS/2 scan-code set 2 → ASCII (shifted) ─────────────────────────────── */
static const unsigned char sc2_shifted[256] = {
/*00*/  0,    0,    0,    0,    0,    0,    0,    0,
/*08*/  0,    0,    0,    0,    0,   '\t',  '~',   0,
/*10*/  0,    0,    0,    0,    0,   'Q',   '!',   0,
/*18*/  0,    0,   'Z',  'S',  'A',  'W',  '@',   0,
/*20*/  0,   'C',  'X',  'D',  'E',  '$',  '#',   0,
/*28*/  0,   ' ',  'V',  'F',  'T',  'R',  '%',   0,
/*30*/  0,   'N',  'B',  'H',  'G',  'Y',  '^',   0,
/*38*/  0,    0,   'M',  'J',  'U',  '&',  '*',   0,
/*40*/  0,   '<',  'K',  'I',  'O',  ')',  '(',   0,
/*48*/  0,   '>',  '?',  'L',  ':',  'P',  '_',   0,
/*50*/  0,    0,   '"',   0,   '{',  '+',   0,    0,
/*58*/  0,    0,  '\n',  '}',   0,   '|',   0,    0,
/*60*/  0,    0,    0,    0,    0,    0,  '\b',   0,
/*66*/ '\b',  0,    0,   '1',   0,   '4',  '7',   0,
/*6e*/  0,    0,   '0',  '.',  '2',  '5',  '6',  '8',
/*76*/'\x1b', 0,    0,   '+',  '3',  '-',  '*',  '9',
/*7e*/  0,    0,    0,    0,    0,    0,    0,    0,
};

/* ── internal PS/2 state ─────────────────────────────────────────────────── */
static int shift_held = 0;  /* 1 when L/R shift is pressed   */
static int skip_next  = 0;  /* 1 after 0xF0 (break prefix)   */
static int extended   = 0;  /* 1 after 0xE0 (extended prefix) */

/* ── Ring buffer for IRQ-driven key capture ──────────────────────────────── */
#define KB_BUF_SIZE 16u
static volatile unsigned char kb_buf[KB_BUF_SIZE];
static volatile unsigned int  kb_head = 0;  /* write index (producer: timer ISR) */
static volatile unsigned int  kb_tail = 0;  /* read  index (consumer: game loop) */

/* ── internal: decode one raw scancode from hardware ───────────────────── */
static char kb_decode_raw(void)
{
    if (!(KMI_STAT & KMI_STAT_RXFULL))
        return 0;

    unsigned char sc = (unsigned char)(KMI_DATA & 0xFFu);

    if (sc == 0xE0) { extended = 1; return 0; }
    if (sc == 0xF0) { skip_next = 1; return 0; }

    if (skip_next) {
        skip_next = 0;
        if (sc == 0x12 || sc == 0x59) shift_held = 0;
        extended = 0;
        return 0;
    }

    if (extended) { extended = 0; return 0; }

    if (sc == 0x12 || sc == 0x59) { shift_held = 1; return 0; }

    unsigned char ascii = shift_held ? sc2_shifted[sc] : sc2_normal[sc];
    return (char)ascii;
}

/* ── public functions ────────────────────────────────────────────────────── */

void kb_init(void)
{
    KMI_CR = KMI_CR_EN;
}

/*
 * kb_poll — drain the PL050 FIFO into the ring buffer.
 * Call this from the timer ISR every tick so no key press is ever dropped.
 */
void kb_poll(void)
{
    /* Drain all bytes currently in the PL050 FIFO */
    while (KMI_STAT & KMI_STAT_RXFULL) {
        char c = kb_decode_raw();
        if (c == 0) continue;   /* prefix byte or unmapped scancode */

        unsigned int next = (kb_head + 1u) % KB_BUF_SIZE;
        if (next != kb_tail) {  /* drop silently if buffer full */
            kb_buf[kb_head] = (unsigned char)c;
            kb_head = next;
        }
    }
}

/*
 * kb_getc — read one character from the ring buffer.
 * Returns 0 if the buffer is empty (non-blocking).
 */
char kb_getc(void)
{
    if (kb_head == kb_tail)
        return 0;   /* buffer empty */

    char c = (char)kb_buf[kb_tail];
    kb_tail = (kb_tail + 1u) % KB_BUF_SIZE;
    return c;
}
