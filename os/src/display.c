// COSMO-32 Display Driver
// 640x400 @ 4bpp Terminal (80x50 characters)

#include <stdint.h>
#include "const.h"
#include "font8x8.h"

#define COLS 80
#define ROWS 50

// Forward declarations
void display_clear(void);
void display_pset(int x, int y, uint8_t color);
static void draw_char(int col, int row, char c, uint8_t fg, uint8_t bg);
static void scroll(void);

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t fg_color = 15;  // White
static uint8_t bg_color = 0;   // Black

// Framebuffer pointer
static volatile uint8_t *fb = (volatile uint8_t *)FRAMEBUF_ADDR;

// Display control registers
static volatile uint32_t *disp_mode = (volatile uint32_t *)(DISPLAY_BASE + DISP_MODE);
static volatile uint16_t *disp_palette = (volatile uint16_t *)(DISPLAY_BASE + DISP_PALETTE);

// RGB565: RRRRR GGGGGG BBBBB
static uint16_t make_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void display_init(void) {
    // Set mode 0: 640x400 @ 4bpp
    *disp_mode = DISP_MODE_640x400_4BPP;

    // Setup palette: CGA-style 16 colors
    disp_palette[0]  = make_rgb565(0x00, 0x00, 0x00);  // Black
    disp_palette[1]  = make_rgb565(0x00, 0x00, 0xAA);  // Blue
    disp_palette[2]  = make_rgb565(0x00, 0xAA, 0x00);  // Green
    disp_palette[3]  = make_rgb565(0x00, 0xAA, 0xAA);  // Cyan
    disp_palette[4]  = make_rgb565(0xAA, 0x00, 0x00);  // Red
    disp_palette[5]  = make_rgb565(0xAA, 0x00, 0xAA);  // Magenta
    disp_palette[6]  = make_rgb565(0xAA, 0x55, 0x00);  // Brown
    disp_palette[7]  = make_rgb565(0xAA, 0xAA, 0xAA);  // Light Gray
    disp_palette[8]  = make_rgb565(0x55, 0x55, 0x55);  // Dark Gray
    disp_palette[9]  = make_rgb565(0x55, 0x55, 0xFF);  // Light Blue
    disp_palette[10] = make_rgb565(0x55, 0xFF, 0x55);  // Light Green
    disp_palette[11] = make_rgb565(0x55, 0xFF, 0xFF);  // Light Cyan
    disp_palette[12] = make_rgb565(0xFF, 0x55, 0x55);  // Light Red
    disp_palette[13] = make_rgb565(0xFF, 0x55, 0xFF);  // Light Magenta
    disp_palette[14] = make_rgb565(0xFF, 0xFF, 0x55);  // Yellow
    disp_palette[15] = make_rgb565(0xFF, 0xFF, 0xFF);  // White

    display_clear();
}

// Set pixel at (x, y) to color index (0-15)
void display_pset(int x, int y, uint8_t color) {
    if (x < 0 || x >= 640 || y < 0 || y >= 400) return;

    int byte_offset = (y * 640 + x) / 2;
    uint8_t byte = fb[byte_offset];

    if (x & 1) {
        // Odd pixel: high nibble
        byte = (byte & 0x0F) | (color << 4);
    } else {
        // Even pixel: low nibble
        byte = (byte & 0xF0) | (color & 0x0F);
    }

    fb[byte_offset] = byte;
}

// Draw line using Bresenham's algorithm
void display_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    dx = dx >= 0 ? dx : -dx;
    dy = dy >= 0 ? dy : -dy;

    if (dx > dy) {
        int err = dx / 2;
        while (x0 != x1) {
            display_pset(x0, y0, color);
            err -= dy;
            if (err < 0) { y0 += sy; err += dx; }
            x0 += sx;
        }
    } else {
        int err = dy / 2;
        while (y0 != y1) {
            display_pset(x0, y0, color);
            err -= dx;
            if (err < 0) { x0 += sx; err += dy; }
            y0 += sy;
        }
    }
    display_pset(x1, y1, color);
}

// Draw circle outline using midpoint algorithm
void display_circle(int cx, int cy, int r, uint8_t color) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        display_pset(cx + x, cy + y, color);
        display_pset(cx - x, cy + y, color);
        display_pset(cx + x, cy - y, color);
        display_pset(cx - x, cy - y, color);
        display_pset(cx + y, cy + x, color);
        display_pset(cx - y, cy + x, color);
        display_pset(cx + y, cy - x, color);
        display_pset(cx - y, cy - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

// Draw filled circle
void display_fill_circle(int cx, int cy, int r, uint8_t color) {
    for (int y = -r; y <= r; y++) {
        int dx = 0;
        while (dx * dx + y * y <= r * r) dx++;
        for (int x = -dx + 1; x < dx; x++) {
            display_pset(cx + x, cy + y, color);
        }
    }
}

// Flood fill (simple scanline, bounded area)
void display_paint(int x, int y, uint8_t fill_color, uint8_t border_color) {
    if (x < 0 || x >= 640 || y < 0 || y >= 400) return;

    // Get current pixel color
    int byte_offset = (y * 640 + x) / 2;
    uint8_t byte = fb[byte_offset];
    uint8_t current = (x & 1) ? (byte >> 4) : (byte & 0x0F);

    if (current == fill_color || current == border_color) return;

    // Simple recursive fill (stack-limited, use for small areas)
    display_pset(x, y, fill_color);
    display_paint(x + 1, y, fill_color, border_color);
    display_paint(x - 1, y, fill_color, border_color);
    display_paint(x, y + 1, fill_color, border_color);
    display_paint(x, y - 1, fill_color, border_color);
}

// Draw character at character position (col, row)
static void draw_char(int col, int row, char c, uint8_t fg, uint8_t bg) {
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return;
    if (c < 32 || c > 127) c = ' ';

    const uint8_t *glyph = font8x8[c - 32];
    int px = col * 8;
    int py = row * 8;

    for (int y = 0; y < 8; y++) {
        uint8_t bits = glyph[y];
        for (int x = 0; x < 8; x++) {
            uint8_t color = (bits & 0x80) ? fg : bg;
            display_pset(px + x, py + y, color);
            bits <<= 1;
        }
    }
}

void display_clear(void) {
    // Fill framebuffer with background color
    // 640*400/2 = 128000 bytes
    uint8_t fill = (bg_color << 4) | bg_color;
    for (int i = 0; i < 128000; i++) {
        fb[i] = fill;
    }
    cursor_x = 0;
    cursor_y = 0;
}

// Scroll screen up by one line
static void scroll(void) {
    // Move lines 1-49 to 0-48
    // Each line: 640 * 8 pixels / 2 = 2560 bytes
    int line_bytes = 2560;
    for (int i = 0; i < (ROWS - 1) * line_bytes; i++) {
        fb[i] = fb[i + line_bytes];
    }
    // Clear last line
    uint8_t fill = (bg_color << 4) | bg_color;
    int last_line_start = (ROWS - 1) * line_bytes;
    for (int i = 0; i < line_bytes; i++) {
        fb[last_line_start + i] = fill;
    }
}

void display_putchar(int c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            draw_char(cursor_x, cursor_y, ' ', fg_color, bg_color);
        }
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c >= 32 && c < 127) {
        draw_char(cursor_x, cursor_y, c, fg_color, bg_color);
        cursor_x++;
    }

    // Line wrap
    if (cursor_x >= COLS) {
        cursor_x = 0;
        cursor_y++;
    }

    // Scroll if needed
    if (cursor_y >= ROWS) {
        scroll();
        cursor_y = ROWS - 1;
    }
}

void display_set_color(uint8_t fg, uint8_t bg) {
    fg_color = fg & 0x0F;
    bg_color = bg & 0x0F;
}

void display_set_cursor(int x, int y) {
    if (x >= 0 && x < COLS) cursor_x = x;
    if (y >= 0 && y < ROWS) cursor_y = y;
}

int display_get_cursor_x(void) { return cursor_x; }
int display_get_cursor_y(void) { return cursor_y; }
