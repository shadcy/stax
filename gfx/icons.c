/* ============================================================================
 * STAX — icons.c
 * Industry-Standard Vector & Geometric Icon Renderer
 * (Ubuntu Yaru / Papirus / SF Symbols Design Aesthetics)
 * ============================================================================ */

#include "icons.h"
#include "framebuffer.h"
#include "string.h"

static file_icon_type_t get_file_type(const char *filename, int is_dir) {
    if (is_dir) return ICON_FILE_FOLDER;
    if (!filename) return ICON_FILE_GENERIC;

    int len = 0;
    while (filename[len]) len++;
    if (len < 4) return ICON_FILE_GENERIC;

    const char *ext = filename + (len - 4);
    if (ext[0] == '.') {
        char e1 = ext[1] | 0x20;
        char e2 = ext[2] | 0x20;
        char e3 = ext[3] | 0x20;

        if (e1 == 't' && e2 == 'x' && e3 == 't') return ICON_FILE_TEXT;
        if (e1 == 'c' && e2 == 'f' && e3 == 'g') return ICON_FILE_TEXT;
        if (e1 == 'b' && e2 == 'i' && e3 == 'n') return ICON_FILE_EXEC;
        if (e1 == 'e' && e2 == 'l' && e3 == 'f') return ICON_FILE_EXEC;
        if (e1 == 'b' && e2 == 'm' && e3 == 'p') return ICON_FILE_IMAGE;
        if (e1 == 'w' && e2 == 'a' && e3 == 'v') return ICON_FILE_AUDIO;
        if (e1 == 'w' && e2 == 'a' && e3 == 'd') return ICON_FILE_PACKAGE;
    }
    if (len >= 5 && (filename[len-5] == '.')) {
        if ((filename[len-4]|0x20) == 's' && (filename[len-3]|0x20) == 't' &&
            (filename[len-2]|0x20) == 'a' && (filename[len-1]|0x20) == 'x') {
            return ICON_FILE_FIRMWARE;
        }
    }
    if (len >= 7 && (filename[len-7] == '.')) {
        if ((filename[len-6]|0x20) == 'l' && (filename[len-5]|0x20) == 'a' &&
            (filename[len-4]|0x20) == 'u' && (filename[len-3]|0x20) == 'n' &&
            (filename[len-2]|0x20) == 'c' && (filename[len-1]|0x20) == 'h') {
            return ICON_FILE_PACKAGE;
        }
    }
    return ICON_FILE_TEXT;
}

/* ── 1. Application Icons (44x44) ─────────────────────────────────────────── */
void icon_draw_app(int ix, int iy, app_icon_id_t id) {
    int x = ix + 10;
    int y = iy + 4;
    int w = 44;
    int h = 44;

    switch (id) {
    case ICON_APP_BROWSER: {
        /* Modern Safari/Chrome Compass Badge */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(32, 130, 235));
        fb_draw_hline(x + 4, y, w - 8, rgb565(110, 185, 255));
        
        /* Outer dial circle */
        int cx = x + 22, cy = y + 22;
        for (int r = 14; r <= 15; r++) {
            fb_draw_hline(cx - 10, cy - r, 20, rgb565(180, 220, 255));
            fb_draw_hline(cx - 10, cy + r, 20, rgb565(180, 220, 255));
        }
        /* Crosshairs / Grid */
        fb_draw_hline(cx - 12, cy, 24, rgb565(180, 220, 255));
        fb_draw_vline(cx, cy - 12, 24, rgb565(180, 220, 255));
        
        /* Compass Needle (Red North / White South) */
        fb_fillrect(cx - 2, cy - 10, 4, 10, rgb565(235, 60, 60));
        fb_fillrect(cx - 2, cy, 4, 10, COLOR_WHITE);
        fb_fillrect(cx - 3, cy - 3, 6, 6, rgb565(255, 215, 0)); /* Gold center pivot */
        break;
    }

    case ICON_APP_TERMINAL: {
        /* Modern Dark Squircle Console with Glowing Emerald Prompt */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(30, 32, 40));
        fb_draw_hline(x + 4, y, w - 8, rgb565(75, 82, 102));
        fb_fill_rounded_rect(x + 3, y + 3, w - 6, 8, 3, rgb565(42, 45, 56));
        
        /* Window dots in titlebar */
        fb_fillrect(x + 6, y + 6, 3, 3, rgb565(235, 84, 32));
        fb_fillrect(x + 11, y + 6, 3, 3, rgb565(250, 190, 40));
        fb_fillrect(x + 16, y + 6, 3, 3, rgb565(40, 210, 100));

        /* Glowing Emerald >_ prompt */
        uint16_t col_green = rgb565(46, 230, 120);
        fb_drawline(x + 10, y + 20, x + 16, y + 26, col_green);
        fb_drawline(x + 11, y + 20, x + 17, y + 26, col_green);
        fb_drawline(x + 10, y + 32, x + 16, y + 26, col_green);
        fb_drawline(x + 11, y + 32, x + 17, y + 26, col_green);
        
        /* Underscore cursor */
        fb_fillrect(x + 22, y + 30, 8, 3, col_green);
        break;
    }

    case ICON_APP_FILES: {
        /* Ubuntu Yaru Two-Tone Folder */
        fb_fill_rounded_rect(x + 2, y + 6, 18, 8, 3, rgb565(22, 92, 170));
        fb_fill_rounded_rect(x + 2, y + 10, 40, 30, 4, rgb565(22, 92, 170));
        
        /* Clean white paper preview insert */
        fb_fill_rounded_rect(x + 6, y + 12, 32, 8, 2, rgb565(250, 252, 255));
        
        /* Front pocket (bright azure gradient) */
        fb_fill_rounded_rect(x + 2, y + 16, 40, 24, 4, rgb565(38, 132, 226));
        fb_draw_hline(x + 4, y + 16, 36, rgb565(130, 195, 255));
        fb_draw_hline(x + 14, y + 28, 16, rgb565(100, 175, 250));
        break;
    }

    case ICON_APP_EDITOR: {
        /* Modern Off-White Document Card with Orange Accent */
        fb_fill_rounded_rect(x + 4, y + 2, 36, 40, 4, rgb565(248, 250, 254));
        fb_draw_hline(x + 6, y + 2, 32, rgb565(220, 225, 235));
        
        /* Folded top right corner */
        fb_fillrect(x + 30, y + 2, 10, 10, rgb565(215, 220, 230));
        fb_drawline(x + 30, y + 2, x + 40, y + 12, rgb565(180, 185, 200));

        /* Code syntax lines */
        fb_fillrect(x + 10, y + 14, 16, 3, rgb565(235, 95, 30)); /* Keyword (Orange) */
        fb_fillrect(x + 10, y + 20, 22, 2, rgb565(50, 120, 220)); /* Identifier (Blue) */
        fb_fillrect(x + 10, y + 25, 18, 2, rgb565(120, 130, 145));/* String */
        fb_fillrect(x + 10, y + 30, 14, 2, rgb565(120, 130, 145));

        /* Drafting Pencil Badge */
        fb_fill_rounded_rect(x + 26, y + 26, 14, 14, 3, rgb565(235, 95, 30));
        fb_drawline(x + 28, y + 36, x + 36, y + 28, COLOR_WHITE);
        break;
    }

    case ICON_APP_CALCULATOR: {
        /* Matte Charcoal Keypad with Emerald LCD Matrix */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(36, 38, 46));
        fb_draw_hline(x + 4, y, w - 8, rgb565(70, 75, 90));
        
        /* Emerald LCD Screen */
        fb_fill_rounded_rect(x + 5, y + 5, 34, 11, 2, rgb565(24, 75, 45));
        fb_fillrect(x + 24, y + 8, 12, 3, rgb565(80, 240, 140));
        
        /* 3x3 Keypad Grid */
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                fb_fill_rounded_rect(x + 6 + c * 8, y + 19 + r * 7, 6, 5, 1, rgb565(75, 80, 95));
            }
        }
        /* Orange Action Key (=) */
        fb_fill_rounded_rect(x + 31, y + 19, 8, 19, 2, rgb565(235, 95, 30));
        fb_draw_hline(x + 33, y + 26, 4, COLOR_WHITE);
        fb_draw_hline(x + 33, y + 30, 4, COLOR_WHITE);
        break;
    }

    case ICON_APP_SYSINFO: {
        /* Precision Silicon CPU Die with Gold Pins */
        fb_fill_rounded_rect(x + 4, y + 4, 36, 36, 4, rgb565(24, 70, 160));
        fb_draw_hline(x + 6, y + 4, 32, rgb565(80, 150, 255));
        
        /* Central Dark Die */
        fb_fill_rounded_rect(x + 12, y + 12, 20, 20, 3, rgb565(12, 36, 90));
        
        /* Gold Boundary Pins */
        uint16_t col_gold = rgb565(255, 215, 60);
        for (int p = 0; p < 4; p++) {
            fb_draw_vline(x + 10 + p * 6, y, 4, col_gold);      /* Top pins */
            fb_draw_vline(x + 10 + p * 6, y + 40, 4, col_gold); /* Bottom pins */
            fb_draw_hline(x, y + 10 + p * 6, 4, col_gold);      /* Left pins */
            fb_draw_hline(x + 40, y + 10 + p * 6, 4, col_gold); /* Right pins */
        }
        
        /* Microcircuit Core Emblem */
        fb_draw_hline(x + 16, y + 22, 12, col_gold);
        fb_draw_vline(x + 22, y + 16, 12, col_gold);
        break;
    }

    case ICON_APP_TASKMGR: {
        /* Activity Monitor with Glowing Neon Pulse Waveform */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(28, 30, 38));
        fb_draw_hline(x + 4, y, w - 8, rgb565(60, 65, 80));
        
        /* Grid Lines */
        fb_draw_hline(x + 4, y + 22, w - 8, rgb565(40, 44, 56));
        fb_draw_vline(x + 22, y + 4, h - 8, rgb565(40, 44, 56));
        
        /* Glowing ECG Waveform */
        uint16_t col_neon = rgb565(50, 240, 120);
        fb_drawline(x + 6,  y + 22, x + 14, y + 22, col_neon);
        fb_drawline(x + 14, y + 22, x + 18, y + 10, col_neon);
        fb_drawline(x + 18, y + 10, x + 23, y + 34, col_neon);
        fb_drawline(x + 23, y + 34, x + 27, y + 18, col_neon);
        fb_drawline(x + 27, y + 18, x + 31, y + 22, col_neon);
        fb_drawline(x + 31, y + 22, x + 38, y + 22, col_neon);
        break;
    }

    case ICON_APP_DOOM: {
        /* Classic Gaming Controller Badge */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(150, 28, 38));
        fb_draw_hline(x + 4, y, w - 8, rgb565(215, 65, 75));
        
        /* D-Pad */
        fb_fillrect(x + 10, y + 20, 12, 4, COLOR_WHITE);
        fb_fillrect(x + 14, y + 16, 4, 12, COLOR_WHITE);
        
        /* Dual Action Buttons (Orange & Gold) */
        fb_fill_rounded_rect(x + 28, y + 16, 6, 6, 2, rgb565(240, 100, 30));
        fb_fill_rounded_rect(x + 34, y + 22, 6, 6, 2, rgb565(255, 215, 0));
        break;
    }

    case ICON_APP_SETTINGS: {
        /* Precision Dual Concentric Gears */
        fb_fill_rounded_rect(x, y, w, h, 8, rgb565(54, 58, 70));
        fb_draw_hline(x + 4, y, w - 8, rgb565(95, 100, 115));
        
        int cx = x + 22, cy = y + 22;
        /* Gear Teeth (Crosses) */
        fb_fill_rounded_rect(cx - 4, cy - 14, 8, 28, 2, rgb565(210, 215, 225));
        fb_fill_rounded_rect(cx - 14, cy - 4, 28, 8, 2, rgb565(210, 215, 225));
        
        /* Diagonals */
        fb_drawline(cx - 9, cy - 9, cx + 9, cy + 9, rgb565(210, 215, 225));
        fb_drawline(cx - 9, cy + 9, cx + 9, cy - 9, rgb565(210, 215, 225));
        
        /* Outer Gear Body */
        fb_fill_rounded_rect(cx - 10, cy - 10, 20, 20, 5, rgb565(175, 180, 195));
        /* Inner Hollow Hole */
        fb_fill_rounded_rect(cx - 4, cy - 4, 8, 8, 3, rgb565(36, 40, 48));
        break;
    }
    }
}

/* ── 2. Full-Size Desktop File Icons (36x40) ──────────────────────────────── */
void icon_draw_desktop_file(int ix, int iy, const char *filename, int is_dir) {
    file_icon_type_t t = get_file_type(filename, is_dir);
    int x = ix + 14;
    int y = iy + 8;

    switch (t) {
    case ICON_FILE_FOLDER: {
        /* Clean Ubuntu Yaru Tall Folder */
        fb_fill_rounded_rect(x, y, 16, 6, 2, rgb565(22, 92, 170));
        fb_fill_rounded_rect(x, y + 4, 36, 34, 4, rgb565(22, 92, 170));
        fb_fill_rounded_rect(x + 4, y + 6, 28, 6, 2, rgb565(250, 252, 255));
        fb_fill_rounded_rect(x, y + 10, 36, 28, 4, rgb565(38, 132, 226));
        fb_draw_hline(x + 2, y + 10, 32, rgb565(120, 190, 255));
        break;
    }

    case ICON_FILE_EXEC: {
        /* Dark Slate Executable Card with Orange Chevron */
        fb_fill_rounded_rect(x + 2, y + 2, 32, 36, 4, rgb565(36, 38, 48));
        fb_draw_hline(x + 4, y + 2, 28, rgb565(75, 80, 95));
        
        /* Terminal prompt chevron >_ */
        uint16_t col_act = rgb565(235, 95, 30);
        fb_drawline(x + 10, y + 16, x + 16, y + 22, col_act);
        fb_drawline(x + 10, y + 28, x + 16, y + 22, col_act);
        fb_fillrect(x + 20, y + 26, 6, 2, col_act);
        break;
    }

    case ICON_FILE_IMAGE: {
        /* Photo Canvas with Mountain & Sun */
        fb_fill_rounded_rect(x + 2, y + 2, 32, 36, 4, rgb565(248, 250, 254));
        fb_draw_hline(x + 4, y + 2, 28, rgb565(210, 215, 225));
        
        /* Sun Circle */
        fb_fill_rounded_rect(x + 20, y + 8, 6, 6, 2, rgb565(255, 195, 40));
        /* Emerald Mountain Peak */
        fb_fill_rounded_rect(x + 6, y + 20, 24, 14, 2, rgb565(40, 150, 90));
        fb_drawline(x + 6, y + 20, x + 16, y + 12, rgb565(60, 180, 110));
        fb_drawline(x + 16, y + 12, x + 26, y + 20, rgb565(60, 180, 110));
        break;
    }

    case ICON_FILE_FIRMWARE:
    case ICON_FILE_PACKAGE: {
        /* ROM / Firmware Cartridge */
        fb_fill_rounded_rect(x + 2, y + 2, 32, 36, 4, rgb565(120, 30, 45));
        fb_draw_hline(x + 4, y + 2, 28, rgb565(190, 60, 75));
        fb_fill_rounded_rect(x + 8, y + 10, 20, 14, 2, rgb565(255, 215, 60));
        fb_fillrect(x + 12, y + 28, 12, 4, COLOR_WHITE);
        break;
    }

    default: {
        /* Clean Document Card with Folded Corner & Syntax Lines */
        fb_fill_rounded_rect(x + 2, y + 2, 32, 36, 4, rgb565(248, 250, 254));
        fb_draw_hline(x + 4, y + 2, 28, rgb565(210, 215, 225));
        
        /* Folded corner */
        fb_fillrect(x + 24, y + 2, 10, 10, rgb565(200, 205, 220));
        
        /* Horizontal text lines */
        fb_draw_hline(x + 8, y + 16, 16, rgb565(130, 140, 160));
        fb_draw_hline(x + 8, y + 22, 20, rgb565(130, 140, 160));
        fb_draw_hline(x + 8, y + 28, 12, rgb565(130, 140, 160));
        break;
    }
    }
}

/* ── 3. Compact File Manager Row Icons (16x16) ────────────────────────────── */
void icon_draw_file_mini(int x, int y, const char *filename, int is_dir) {
    file_icon_type_t t = get_file_type(filename, is_dir);

    switch (t) {
    case ICON_FILE_FOLDER:
        fb_fill_rounded_rect(x, y + 1, 8, 4, 1, rgb565(22, 92, 170));
        fb_fill_rounded_rect(x, y + 3, 16, 12, 2, rgb565(38, 132, 226));
        fb_draw_hline(x + 1, y + 3, 14, rgb565(120, 190, 255));
        break;

    case ICON_FILE_EXEC:
        fb_fill_rounded_rect(x + 1, y + 1, 14, 14, 2, rgb565(36, 38, 48));
        fb_drawline(x + 4, y + 5, x + 7, y + 8, rgb565(235, 95, 30));
        fb_drawline(x + 4, y + 11, x + 7, y + 8, rgb565(235, 95, 30));
        fb_fillrect(x + 9, y + 10, 3, 1, rgb565(235, 95, 30));
        break;

    case ICON_FILE_IMAGE:
        fb_fill_rounded_rect(x + 1, y + 1, 14, 14, 2, rgb565(248, 250, 254));
        fb_fill_rounded_rect(x + 3, y + 7, 10, 6, 1, rgb565(40, 150, 90));
        fb_fillrect(x + 9, y + 3, 2, 2, rgb565(255, 195, 40));
        break;

    case ICON_FILE_FIRMWARE:
    case ICON_FILE_PACKAGE:
        fb_fill_rounded_rect(x + 1, y + 1, 14, 14, 2, rgb565(140, 30, 45));
        fb_fillrect(x + 5, y + 5, 6, 6, rgb565(255, 215, 60));
        break;

    default:
        /* Document */
        fb_fill_rounded_rect(x + 2, y + 1, 12, 14, 2, rgb565(248, 250, 254));
        fb_draw_hline(x + 4, y + 5, 6, rgb565(100, 110, 130));
        fb_draw_hline(x + 4, y + 8, 8, rgb565(100, 110, 130));
        fb_draw_hline(x + 4, y + 11, 5, rgb565(100, 110, 130));
        break;
    }
}
