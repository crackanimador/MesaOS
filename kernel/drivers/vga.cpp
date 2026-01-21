#include "vga.hpp"
#include "arch/i386/io_port.hpp"
#include <string.h>

namespace MesaOS::Drivers {

constexpr size_t VGA_BUFFER_SIZE = 80 * 25;
constexpr size_t SCROLL_BUFFER_LINES = 200;
constexpr size_t SCROLL_BUFFER_COLS = 80;

size_t VGADriver::terminal_row = 0;
size_t VGADriver::terminal_column = 0;
uint8_t VGADriver::terminal_color = 0x07;
uint16_t* VGADriver::terminal_buffer = reinterpret_cast<uint16_t*>(0xB8000);

int VGADriver::current_tty = 0;
uint16_t VGADriver::tty_buffers[4][2000];
size_t VGADriver::tty_rows[NUM_TTYS] = {0};
size_t VGADriver::tty_columns[NUM_TTYS] = {0};
uint8_t VGADriver::tty_colors[NUM_TTYS] = {0x07, 0x07, 0x07, 0x07};

char VGADriver::scroll_buffer[SCROLL_BUFFER_LINES][SCROLL_BUFFER_COLS];
int VGADriver::scroll_write_line = 0;
int VGADriver::scroll_view_offset = 0;
bool VGADriver::scroll_mode = false;

void VGADriver::initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK);
    terminal_buffer = reinterpret_cast<uint16_t*>(0xB8000);
    
    // Clear all TTY buffers
    for(int t = 0; t < 4; ++t) {
        for (size_t y = 0; y < 25; ++y) {
            for (size_t x = 0; x < 80; ++x) {
                const size_t index = y * 80 + x;
                if (index >= VGA_BUFFER_SIZE) continue; // Bounds check
                uint16_t entry = vga_entry(' ', vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK));
                tty_buffers[t][index] = entry;
                if (t == 0 && terminal_buffer) terminal_buffer[index] = entry; // Init screen
            }
        }
    }
    current_tty = 0;
    
    // Initialize scroll buffer
    memset(scroll_buffer, ' ', sizeof(scroll_buffer));
    scroll_write_line = 0;
    scroll_view_offset = 0;
    scroll_mode = false;
}

void VGADriver::switch_tty(int tty_id) {
    if (tty_id < 0 || tty_id >= 4 || tty_id == current_tty) return;

    // Save current state
    tty_rows[current_tty] = terminal_row;
    tty_columns[current_tty] = terminal_column;
    tty_colors[current_tty] = terminal_color;
    memcpy(tty_buffers[current_tty], terminal_buffer, 80 * 25 * 2);

    // Load new state
    current_tty = tty_id;
    terminal_row = tty_rows[current_tty];
    terminal_column = tty_columns[current_tty];
    terminal_color = tty_colors[current_tty];
    memcpy(terminal_buffer, tty_buffers[current_tty], 80 * 25 * 2);

    // Update cursor
    uint16_t pos = static_cast<uint16_t>(terminal_row * 80 + terminal_column);
    MesaOS::Arch::x86::outb(0x3D4, 0x0F);
    MesaOS::Arch::x86::outb(0x3D5, static_cast<uint8_t>(pos & 0xFF));
    MesaOS::Arch::x86::outb(0x3D4, 0x0E);
    MesaOS::Arch::x86::outb(0x3D5, static_cast<uint8_t>((pos >> 8) & 0xFF));
}

int VGADriver::get_current_tty() { return current_tty; }

void VGADriver::add_line_to_buffer(const char* line) {
    if (!line) return;
    size_t line_len = strlen(line);
    for(size_t i = 0; i < SCROLL_BUFFER_COLS; ++i) {
        scroll_buffer[scroll_write_line][i] = (i < line_len) ? line[i] : ' ';
    }
    scroll_write_line = (scroll_write_line + 1) % SCROLL_BUFFER_LINES;
}

void VGADriver::scroll_up() {
    if (scroll_view_offset >= static_cast<size_t>(SCROLL_BUFFER_LINES - 25 - 1)) return; // Prevent overflow
    scroll_view_offset++;
    scroll_mode = true;
    render_view();

    // Visual feedback - bounds checked
    if (terminal_buffer) {
        char msg[16] = "[SCROLL-  ]";
        if (scroll_view_offset < 100) {
            msg[8] = '0' + (scroll_view_offset / 10);
            msg[9] = '0' + (scroll_view_offset % 10);
        }

        // Print at top-right
        for(size_t i = 0; msg[i] && (68 + i) < VGA_BUFFER_SIZE; ++i) {
            terminal_buffer[68 + i] = vga_entry(msg[i], vga_entry_color(VGAColor::BLACK, VGAColor::LIGHT_GREY));
        }
    }
}

void VGADriver::scroll_down() {
    if (scroll_view_offset == 0) return;
    scroll_view_offset--;
    if (scroll_view_offset == 0) {
        scroll_mode = false;
        // Restore live view from TTY buffer
        if (terminal_buffer && current_tty >= 0 && current_tty < 4) {
            memcpy(terminal_buffer, tty_buffers[current_tty], VGA_BUFFER_SIZE * sizeof(uint16_t));
        }
    } else {
        render_view();
    }
}

void VGADriver::render_view() {
    if (!scroll_mode || !terminal_buffer || current_tty < 0 || current_tty >= 4) return;

    // Calculate starting line in circular buffer
    int start_line = (scroll_write_line - scroll_view_offset - 25 + SCROLL_BUFFER_LINES) % SCROLL_BUFFER_LINES;

    for(size_t row = 0; row < 25; ++row) {
        size_t buffer_line = (static_cast<size_t>(start_line) + row) % SCROLL_BUFFER_LINES;
        for(size_t col = 0; col < 80; ++col) {
            if (col >= SCROLL_BUFFER_COLS) continue;
            char c = scroll_buffer[buffer_line][col];
            if (c == '\0') c = ' ';
            uint16_t entry = vga_entry(c, terminal_color);
            size_t index = row * 80 + col;
            if (index < VGA_BUFFER_SIZE) {
                terminal_buffer[index] = entry;
                tty_buffers[current_tty][index] = entry;
            }
        }
    }
}

void VGADriver::set_color(uint8_t color) {
    terminal_color = color;
}

void VGADriver::put_entry_at(char c, uint8_t color, size_t x, size_t y) {
    if (!terminal_buffer || x >= 80 || y >= 25 || current_tty < 0 || current_tty >= 4) return;
    const size_t index = y * 80 + x;
    if (index >= VGA_BUFFER_SIZE) return;
    uint16_t entry = vga_entry(c, color);
    terminal_buffer[index] = entry;
    // Mirror to backing buffer
    tty_buffers[current_tty][index] = entry;
}

void VGADriver::update_cursor() {
    if (terminal_row >= 25 || terminal_column >= 80) return;
    uint16_t pos = static_cast<uint16_t>(terminal_row * 80 + terminal_column);
    MesaOS::Arch::x86::outb(0x3D4, 0x0F);
    MesaOS::Arch::x86::outb(0x3D5, static_cast<uint8_t>(pos & 0xFF));
    MesaOS::Arch::x86::outb(0x3D4, 0x0E);
    MesaOS::Arch::x86::outb(0x3D5, static_cast<uint8_t>((pos >> 8) & 0xFF));
}

void VGADriver::scroll() {
    if (!terminal_buffer || terminal_row < 25) return;

    uint8_t blank = vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK);
    uint16_t blank_entry = vga_entry(' ', blank);

    // Scroll up by one line
    size_t bytes_to_move = (25 - 1) * 80 * sizeof(uint16_t);
    if (bytes_to_move > 0) {
        memmove(terminal_buffer, terminal_buffer + 80, bytes_to_move);
    }

    // Clear the last line
    for (size_t i = (25 - 1) * 80; i < VGA_BUFFER_SIZE; ++i) {
        terminal_buffer[i] = blank_entry;
    }

    terminal_row = 25 - 1;
}

// ANSI escape sequence state
static bool in_escape = false;
static char escape_buffer[16];
static int escape_index = 0;

void VGADriver::put_char(char c) {
    if (scroll_mode) {
        // Still capture newlines to buffer for when they return
        if (c == '\n') {
            char line[SCROLL_BUFFER_COLS + 1] = {0};
            for(size_t i = 0; i < 80 && i < SCROLL_BUFFER_COLS; ++i) {
                size_t index = terminal_row * 80 + i;
                if (index < VGA_BUFFER_SIZE && terminal_buffer) {
                    line[i] = static_cast<char>(terminal_buffer[index] & 0xFF);
                }
            }
            add_line_to_buffer(line);
        }
        return; // Don't render anything while scrolling
    }

    // ANSI escape sequence handling
    if (c == '\x1B') { // ESC
        in_escape = true;
        escape_index = 0;
        escape_buffer[escape_index++] = c;
        return;
    }

    if (in_escape) {
        escape_buffer[escape_index++] = c;

        if (escape_index >= 15) { // Buffer overflow protection
            in_escape = false;
            return;
        }

        // Check for complete ANSI sequence
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '@' || c == '`') {
            // Process the escape sequence
            process_ansi_sequence(escape_buffer, escape_index);
            in_escape = false;
            return;
        }
        return; // Still collecting sequence
    }

    if (c == '\n') {
        // Capture current line to scroll buffer
        char line[SCROLL_BUFFER_COLS + 1] = {0};
        for(size_t i = 0; i < 80 && i < SCROLL_BUFFER_COLS; ++i) {
            size_t index = terminal_row * 80 + i;
            if (index < VGA_BUFFER_SIZE && terminal_buffer) {
                line[i] = static_cast<char>(terminal_buffer[index] & 0xFF);
            }
        }
        add_line_to_buffer(line);

        terminal_column = 0;
        ++terminal_row;

        // Exit scroll mode on new output
        if (scroll_mode) {
            scroll_mode = false;
            scroll_view_offset = 0;
        }
    } else if (c == '\b') {
        if (terminal_column > 0) {
            --terminal_column;
        }
    } else if (static_cast<unsigned char>(c) >= 32) { // Printable characters
        put_entry_at(c, terminal_color, terminal_column, terminal_row);
        ++terminal_column;
        if (terminal_column >= 80) {
            terminal_column = 0;
            ++terminal_row;
        }
    }

    scroll();
    update_cursor();
}

// Process ANSI escape sequences for color support
void VGADriver::process_ansi_sequence(const char* sequence, int length) {
    if (length < 3 || sequence[0] != '\x1B' || sequence[1] != '[') return;

    // Parse ANSI color codes (basic support)
    if (sequence[length-1] == 'm') {
        // Color code sequence like \x1B[31m
        int code = 0;
        bool parsing = false;

        for (int i = 2; i < length - 1; ++i) {
            if (sequence[i] >= '0' && sequence[i] <= '9') {
                code = code * 10 + (sequence[i] - '0');
                parsing = true;
            } else if (sequence[i] == ';') {
                // Process current code and reset for next
                set_color_from_ansi(code);
                code = 0;
                parsing = false;
            }
        }

        if (parsing) {
            set_color_from_ansi(code);
        }
    }
}

// Convert ANSI color codes to VGA colors
void VGADriver::set_color_from_ansi(int code) {
    VGAColor fg = VGAColor::LIGHT_GREY;
    VGAColor bg = VGAColor::BLACK;

    switch (code) {
        case 0:  // Reset
            fg = VGAColor::LIGHT_GREY;
            bg = VGAColor::BLACK;
            break;
        case 30: fg = VGAColor::BLACK; break;
        case 31: fg = VGAColor::RED; break;
        case 32: fg = VGAColor::GREEN; break;
        case 33: fg = VGAColor::BROWN; break;
        case 34: fg = VGAColor::BLUE; break;
        case 35: fg = VGAColor::MAGENTA; break;
        case 36: fg = VGAColor::CYAN; break;
        case 37: fg = VGAColor::LIGHT_GREY; break;
        case 40: bg = VGAColor::BLACK; break;
        case 41: bg = VGAColor::RED; break;
        case 42: bg = VGAColor::GREEN; break;
        case 43: bg = VGAColor::BROWN; break;
        case 44: bg = VGAColor::BLUE; break;
        case 45: bg = VGAColor::MAGENTA; break;
        case 46: bg = VGAColor::CYAN; break;
        case 47: bg = VGAColor::LIGHT_GREY; break;
        // Bright colors
        case 90: fg = VGAColor::DARK_GREY; break;
        case 91: fg = VGAColor::LIGHT_RED; break;
        case 92: fg = VGAColor::LIGHT_GREEN; break;
        case 93: fg = VGAColor::LIGHT_BROWN; break;
        case 94: fg = VGAColor::LIGHT_BLUE; break;
        case 95: fg = VGAColor::LIGHT_MAGENTA; break;
        case 96: fg = VGAColor::LIGHT_CYAN; break;
        case 97: fg = VGAColor::WHITE; break;
        case 100: bg = VGAColor::DARK_GREY; break;
        case 101: bg = VGAColor::LIGHT_RED; break;
        case 102: bg = VGAColor::LIGHT_GREEN; break;
        case 103: bg = VGAColor::LIGHT_BROWN; break;
        case 104: bg = VGAColor::LIGHT_BLUE; break;
        case 105: bg = VGAColor::LIGHT_MAGENTA; break;
        case 106: bg = VGAColor::LIGHT_CYAN; break;
        case 107: bg = VGAColor::WHITE; break;
    }

    terminal_color = vga_entry_color(fg, bg);
}

void VGADriver::write(const char* data, size_t size) {
    if (!data) return;
    for (size_t i = 0; i < size; ++i) {
        put_char(data[i]);
    }
}

void VGADriver::write_string(const char* data) {
    if (!data) return;
    while (*data) {
        put_char(*data++);
    }
}

} // namespace MesaOS::Drivers
