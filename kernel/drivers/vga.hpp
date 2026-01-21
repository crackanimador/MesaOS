#ifndef VGA_HPP
#define VGA_HPP

#include <stdint.h>
#include <stddef.h>

namespace MesaOS::Drivers {

enum class VGAColor : uint8_t {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GREY = 7,
    DARK_GREY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14,
    WHITE = 15,
};

static inline uint8_t vga_entry_color(VGAColor fg, VGAColor bg) {
    return (uint8_t)fg | (uint8_t)bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

class VGADriver {
public:
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;
    static const int NUM_TTYS = 4;

    void initialize();
    static void switch_tty(int tty_id);
    int get_current_tty();
    void set_color(uint8_t color);
    void put_entry_at(char c, uint8_t color, size_t x, size_t y);
    void put_char(char c);
    void write(const char* data, size_t size);
    void write_string(const char* data);
    void update_cursor();
    void scroll();
    static void scroll_up();
    static void scroll_down();
    static void add_line_to_buffer(const char* line);
    static void render_view();

    // ANSI escape sequence support
    static void process_ansi_sequence(const char* sequence, int length);
    static void set_color_from_ansi(int code);

private:
    static size_t terminal_row;
    static size_t terminal_column;
    static uint8_t terminal_color;
    static uint16_t* terminal_buffer; // Always points to 0xB8000
    
    // TTY State
    static int current_tty;
    static uint16_t tty_buffers[NUM_TTYS][VGA_WIDTH * VGA_HEIGHT];
    static size_t tty_rows[NUM_TTYS];
    static size_t tty_columns[NUM_TTYS];
    static uint8_t tty_colors[NUM_TTYS];
    
    // Scroll Buffer
    static char scroll_buffer[200][80];
    static int scroll_write_line;
    static int scroll_view_offset; // 0 = live view, >0 = scrolled back
    static bool scroll_mode;
};

} // namespace MesaOS::Drivers

#endif
