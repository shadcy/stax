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
/*68*/  0,   '1',   0,   '4',  '7',   0,    0,    0,
/*70*/ '0',  '.',  '2',  '5',  '6',  '8', '\x1b', 0,
/*78*/  0,   '+',  '3',  '-',  '*',  '9',   0,    0,
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
/*68*/  0,   '1',   0,   '4',  '7',   0,    0,    0,
/*70*/ '0',  '.',  '2',  '5',  '6',  '8', '\x1b', 0,
/*78*/  0,   '+',  '3',  '-',  '*',  '9',   0,    0,
};

/* ── internal PS/2 state ─────────────────────────────────────────────────────── */
static int shift_held = 0;  /* 1 when L/R shift is pressed   */
static int ctrl_held  = 0;  /* 1 when L Ctrl is pressed      */
static int skip_next  = 0;  /* 1 after 0xF0 (break prefix)   */
static int extended   = 0;  /* 1 after 0xE0 (extended prefix) */

extern volatile int fs_abort_flag;

/* ── Ring buffer: press = raw char (1–127), release = char | 0x80 (129–254) ── */
#define KB_BUF_SIZE 32u
static volatile unsigned char kb_buf[KB_BUF_SIZE];
static volatile unsigned int  kb_head = 0;
static volatile unsigned int  kb_tail = 0;

/* ── Continuous Key State ── */
volatile int kb_state[256] = {0};

static char kb_decode_sc(unsigned char sc, int is_break)
{
    if (sc == 0x12 || sc == 0x59) {    /* shift */
        shift_held = is_break ? 0 : 1;
        return KB_SHIFT;
    }
    if (sc == 0x14) {                  /* ctrl */
        ctrl_held = is_break ? 0 : 1;
        return KB_CTRL;
    }
    if (sc == 0x11) {                  /* alt */
        return KB_ALT;
    }
    if (extended) { extended = 0; return 0; }   /* ignore extended for now */
    unsigned char ascii = shift_held ? sc2_shifted[sc] : sc2_normal[sc];
    
    /* Handle Ctrl+Q (0x11 is ASCII Device Control 1) */
    if (ctrl_held && (ascii == 'q' || ascii == 'Q')) {
        if (!is_break) {
            fs_abort_flag = 1;
        }
        return 0x11;
    }
    
    return (char)ascii;
}

/* ── public functions ────────────────────────────────────────────────────────────────────────────── */

void kb_init(void)
{
    KMI_CR = KMI_CR_EN;
}

/*
 * kb_poll — drain the PL050 FIFO into the ring buffer.
 * Press events are stored as-is (char value 1–127).
 * Release events are stored with bit 7 set (char | 0x80).
 * Called from the timer ISR every 10 ms.
 */
void kb_poll(void)
{
    while (KMI_STAT & KMI_STAT_RXFULL) {
        unsigned char sc = (unsigned char)(KMI_DATA & 0xFFu);

        if (sc == 0xE0) { extended = 1; continue; }
        if (sc == 0xF0) { skip_next = 1; continue; }

        int is_break = skip_next;
        skip_next = 0;

        /* Handle extended codes (E0-prefixed: arrow keys etc.) */
        if (extended) {
            extended = 0;
            unsigned char entry = 0;
            if      (sc == 0x75) entry = KB_UP;
            else if (sc == 0x72) entry = KB_DOWN;
            else if (sc == 0x6B) entry = KB_LEFT;
            else if (sc == 0x74) entry = KB_RIGHT;
            else if (sc == 0x11) entry = KB_ALT;
            else if (sc == 0x14) entry = KB_CTRL;
            
            if (entry) {
                kb_state[entry] = is_break ? 0 : 1;
                unsigned char buf_entry = entry | (is_break ? 0x80u : 0);
                unsigned int next = (kb_head + 1u) % KB_BUF_SIZE;
                if (next != kb_tail) { kb_buf[kb_head] = buf_entry; kb_head = next; }
            }
            continue;
        }

        char c = kb_decode_sc(sc, is_break);
        if (c == 0) continue;

        kb_state[(unsigned char)c] = is_break ? 0 : 1;

        /* Encode: press = c, release = c | 0x80 */
        unsigned char entry = (unsigned char)c;
        if (is_break) entry |= 0x80u;


        unsigned int next = (kb_head + 1u) % KB_BUF_SIZE;
        if (next != kb_tail)
        {
            kb_buf[kb_head] = entry;
            kb_head = next;
        }
    }
}

/*
 * kb_getc — read next PRESS from the ring buffer (skips releases).
 * Returns 0 if empty. Used by console and snake.
 */
char kb_getc(void)
{
    while (kb_head != kb_tail) {
        unsigned char ev = kb_buf[kb_tail];
        kb_tail = (kb_tail + 1u) % KB_BUF_SIZE;
        if (!(ev & 0x80u))
            return (char)ev;   /* press event */
        /* release event — skip for console use */
    }
    return 0;
}

/*
 * kb_getevent — read next press OR release from the ring buffer.
 * Returns +char for key press, -char for key release, 0 if empty.
 * Used by DOOM's I_StartTic for proper held-key movement.
 */
int kb_getevent(void)
{
    if (kb_head == kb_tail)
        return 0;
    unsigned char ev = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1u) % KB_BUF_SIZE;
    if (ev & 0x80u)
        return -(int)(ev & 0x7Fu);   /* release */
    return (int)ev;                  /* press   */
}

int kb_is_pressed(char key)
{
    return kb_state[(unsigned char)key];
}
