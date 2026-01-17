#include "vga.hpp"
#include "arch/i386/io_port.hpp"
#include "vga.hpp"
#include "arch/i386/io_port.hpp"
#include <string.h>

namespace MesaOS::Drivers {

size_t VGADriver::terminal_row = 0;
size_t VGADriver::terminal_column = 0;
uint8_t VGADriver::terminal_color = 0x07;
uint16_t* VGADriver::terminal_buffer = (uint16_t*)0xB8000;

int VGADriver::current_tty = 0;
uint16_t VGADriver::tty_buffers[NUM_TTYS][VGA_WIDTH * VGA_HEIGHT];
size_t VGADriver::tty_rows[NUM_TTYS] = {0};
size_t VGADriver::tty_columns[NUM_TTYS] = {0};
uint8_t VGADriver::tty_colors[NUM_TTYS] = {0x07, 0x07, 0x07, 0x07};

char VGADriver::scroll_buffer[200][80];
int VGADriver::scroll_write_line = 0;
int VGADriver::scroll_view_offset = 0;
bool VGADriver::scroll_mode = false;

void VGADriver::initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK);
    terminal_buffer = (uint16_t*) 0xB8000;
    
    // Clear all TTY buffers
    for(int t=0; t<NUM_TTYS; t++) {
        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                const size_t index = y * VGA_WIDTH + x;
                uint16_t entry = vga_entry(' ', vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK));
                tty_buffers[t][index] = entry;
                if (t == 0) terminal_buffer[index] = entry; // Init screen
            }
        }
    }
    current_tty = 0;
    
    // Initialize scroll buffer
    for(int line=0; line<200; line++) {
        for(int col=0; col<80; col++) {
            scroll_buffer[line][col] = ' ';
        }
    }
}

void VGADriver::switch_tty(int tty_id) {
    if (tty_id < 0 || tty_id >= NUM_TTYS || tty_id == current_tty) return;

    // Save current state
    tty_rows[current_tty] = terminal_row;
    tty_columns[current_tty] = terminal_column;
    tty_colors[current_tty] = terminal_color;
    memcpy(tty_buffers[current_tty], terminal_buffer, VGA_WIDTH * VGA_HEIGHT * 2);

    // Load new state
    current_tty = tty_id;
    terminal_row = tty_rows[current_tty];
    terminal_column = tty_columns[current_tty];
    terminal_color = tty_colors[current_tty];
    memcpy(terminal_buffer, tty_buffers[current_tty], VGA_WIDTH * VGA_HEIGHT * 2);
    
    // Update cursor
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    MesaOS::Arch::x86::outb(0x3D4, 0x0F);
    MesaOS::Arch::x86::outb(0x3D5, (uint8_t)(pos & 0xFF));
    MesaOS::Arch::x86::outb(0x3D4, 0x0E);
    MesaOS::Arch::x86::outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

int VGADriver::get_current_tty() { return current_tty; }

void VGADriver::add_line_to_buffer(const char* line) {
    for(int i=0; i<80; i++) {
        scroll_buffer[scroll_write_line][i] = (i < (int)strlen(line)) ? line[i] : ' ';
    }
    scroll_write_line = (scroll_write_line + 1) % 200;
}

void VGADriver::scroll_up() {
    if (scroll_view_offset < 175) {
        scroll_view_offset++;
        scroll_mode = true;
        render_view();
        
        // Visual feedback
        char msg[40];
        msg[0] = '['; msg[1] = 'S'; msg[2] = 'C'; msg[3] = 'R'; msg[4] = 'O';
        msg[5] = 'L'; msg[6] = 'L'; msg[7] = ' '; msg[8] = '-';
        msg[9] = '0' + (scroll_view_offset / 10);
        msg[10] = '0' + (scroll_view_offset % 10);
        msg[11] = ']'; msg[12] = 0;
        
        // Print at top-right
        for(int i=0; msg[i]; i++) {
            terminal_buffer[i + 68] = vga_entry(msg[i], vga_entry_color(VGAColor::BLACK, VGAColor::LIGHT_GREY));
        }
    }
}

void VGADriver::scroll_down() {
    if (scroll_view_offset > 0) {
        scroll_view_offset--;
        if (scroll_view_offset == 0) {
            scroll_mode = false;
            // Restore live view from TTY buffer
            for(size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                terminal_buffer[i] = tty_buffers[current_tty][i];
            }
            // Clear scroll indicator
            for(int i = 68; i < 80; i++) {
                terminal_buffer[i] = vga_entry(' ', terminal_color);
            }
        } else {
            render_view();
        }
    }
}

void VGADriver::render_view() {
    if (!scroll_mode) return; // Don't render if in live mode
    
    // Calculate starting line in circular buffer
    int start_line = (scroll_write_line - scroll_view_offset - 25 + 400) % 200;
    
    for(size_t row = 0; row < VGA_HEIGHT; row++) {
        int buffer_line = (start_line + row) % 200;
        for(size_t col = 0; col < VGA_WIDTH; col++) {
            char c = scroll_buffer[buffer_line][col];
            if (c == 0) c = ' ';
            uint16_t entry = vga_entry(c, terminal_color);
            size_t index = row * VGA_WIDTH + col;
            terminal_buffer[index] = entry;
            tty_buffers[current_tty][index] = entry;
        }
    }
}

void VGADriver::set_color(uint8_t color) {
    terminal_color = color;
}

void VGADriver::put_entry_at(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    uint16_t entry = vga_entry(c, color);
    terminal_buffer[index] = entry;
    // Mirror to backing buffer
    tty_buffers[current_tty][index] = entry;
}

void VGADriver::update_cursor() {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    MesaOS::Arch::x86::outb(0x3D4, 0x0F);
    MesaOS::Arch::x86::outb(0x3D5, (uint8_t)(pos & 0xFF));
    MesaOS::Arch::x86::outb(0x3D4, 0x0E);
    MesaOS::Arch::x86::outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void VGADriver::scroll() {
    uint8_t blank = vga_entry_color(VGAColor::LIGHT_GREY, VGAColor::BLACK);
    uint16_t blank_entry = vga_entry(' ', blank);

    if (terminal_row >= VGA_HEIGHT) {
        int i;
        for (i = 0 * VGA_WIDTH; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            terminal_buffer[i] = terminal_buffer[i + VGA_WIDTH];
        }

        for (i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            terminal_buffer[i] = blank_entry;
        }
        terminal_row = VGA_HEIGHT - 1;
    }
}

void VGADriver::put_char(char c) {
    // If in scroll mode, suppress live output (user is viewing history)
    if (scroll_mode) {
        // Still capture newlines to buffer for when they return
        if (c == '\n') {
            char line[81];
            for(size_t i=0; i<VGA_WIDTH; i++) {
                uint16_t entry = terminal_buffer[terminal_row * VGA_WIDTH + i];
                line[i] = (char)(entry & 0xFF);
            }
            line[80] = 0;
            add_line_to_buffer(line);
        }
        return; // Don't render anything while scrolling
    }
    
    if (c == '\n') {
        // Capture current line to scroll buffer
        char line[81];
        for(size_t i=0; i<VGA_WIDTH; i++) {
            uint16_t entry = terminal_buffer[terminal_row * VGA_WIDTH + i];
            line[i] = (char)(entry & 0xFF);
        }
        line[80] = 0;
        add_line_to_buffer(line);
        
        terminal_column = 0;
        terminal_row++;
        
        // Exit scroll mode on new output
        if (scroll_mode) {
            scroll_mode = false;
            scroll_view_offset = 0;
        }
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
        }
    } else if (c >= ' ') {
        put_entry_at(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    scroll();
    update_cursor();
}

void VGADriver::write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        put_char(data[i]);
}

void VGADriver::write_string(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++)
        put_char(data[i]);
}

} // namespace MesaOS::Drivers
