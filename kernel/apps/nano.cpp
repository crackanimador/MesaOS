#include "nano.hpp"
#include "drivers/vga.hpp"
#include "drivers/keyboard.hpp"
#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"
#include "fs/mesafs.hpp"
#include <string.h>

namespace MesaOS::Apps {

char Nano::buffer[4096];
char Nano::clipboard[512];
char Nano::current_file[128];
size_t Nano::cursor_pos = 0;
bool Nano::running = false;
bool Nano::show_help = false;

void Nano::run(const char* filename) {
    running = true;
    show_help = false;
    memset(buffer, 0, sizeof(buffer));
    memset(clipboard, 0, sizeof(clipboard));
    memset(current_file, 0, sizeof(current_file));
    cursor_pos = 0;
    
    MesaOS::Drivers::VGADriver vga;
    vga.initialize(); // Clear shell content

    if (filename) {
        strcpy(current_file, filename);
        MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, filename);
        if (node) {
            MesaOS::FS::read_fs(node, 0, node->length, (uint8_t*)buffer);
            cursor_pos = strlen(buffer);
        }
    }

    refresh_screen();
}

void Nano::save() {
    if (strlen(current_file) == 0) return;
    
    MesaOS::FS::fs_node* node = MesaOS::FS::find_path_fs(MesaOS::FS::fs_root, current_file);
    if (!node) {
        if (strncmp(current_file, "/disk/", 6) == 0 || strncmp(current_file, "disk/", 5) == 0) {
            const char* name = (current_file[0] == '/') ? current_file + 6 : current_file + 5;
            node = MesaOS::FS::MesaFS::create_file(name);
        } else {
            node = MesaOS::FS::RAMFS::create_file(current_file, "");
        }
    }
    
    if (node) {
        MesaOS::FS::write_fs(node, 0, strlen(buffer), (uint8_t*)buffer);
    }
}

void Nano::refresh_screen() {
    MesaOS::Drivers::VGADriver vga;
    vga.initialize(); // Clear screen
    
    // Header
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::BLACK, MesaOS::Drivers::VGAColor::LIGHT_GREY));
    vga.write_string(" ^O Save | ^X Exit | ^G Help | ^K Cut | ^U Paste | ^C Pos | ^W Search      ");
    
    if (show_help) {
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLUE));
        vga.write_string("\n --- NANO HELP ---\n");
        vga.write_string(" ^O: Save current file\n");
        vga.write_string(" ^X: Exit editor\n");
        vga.write_string(" ^K: Delete ('Cut') until end of line\n");
        vga.write_string(" ^U: Paste last cut text\n");
        vga.write_string(" ^G: Toggle this help\n");
        vga.write_string(" ^C: Show current cursor position\n");
        vga.write_string(" ^W: Search (Placeholder functionality)\n");
        vga.write_string(" Press ^G to return to editor.\n");
    } else {
        // Content
        vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::BLACK));
        vga.write_string("\n");
        vga.write_string(buffer);
        // Draw cursor (simple representation if possible, but put_char handles hardware cursor)
    }
}

void Nano::cut_line() {
    // Basic implementation: cut from cursor to next \n
    size_t i = 0;
    while (buffer[cursor_pos] != '\0' && buffer[cursor_pos] != '\n' && i < 511) {
        clipboard[i++] = buffer[cursor_pos];
        // Shift buffer left
        for(size_t j = cursor_pos; buffer[j] != '\0'; j++) {
            buffer[j] = buffer[j+1];
        }
    }
    clipboard[i] = '\0';
}

void Nano::paste_line() {
    size_t len = strlen(clipboard);
    if (cursor_pos + len >= 4095) return;
    
    // Shift buffer right
    size_t buf_len = strlen(buffer);
    for (int j = (int)buf_len; j >= (int)cursor_pos; j--) {
        buffer[j + len] = buffer[j];
    }
    
    // Insert clipboard
    for (size_t i = 0; i < len; i++) {
        buffer[cursor_pos + i] = clipboard[i];
    }
    cursor_pos += len;
}

void Nano::show_cursor_pos() {
    MesaOS::Drivers::VGADriver vga;
    char buf[32];
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::MAGENTA));
    vga.write_string("\n[ Cursor at position: ");
    vga.write_string(itoa(cursor_pos, buf, 10));
    vga.write_string(" ]");
}

void Nano::search() {
    // Real search logic would require a prompt. 
    // Since we don't have a sub-terminal prompt yet, let's at least show it's registered.
    MesaOS::Drivers::VGADriver vga;
    vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::RED));
    vga.write_string("\n[ Search not yet implemented: Need input prompt ]");
}

void Nano::handle_input(char c) {
    if (!running) return;

    if (MesaOS::Drivers::Keyboard::is_ctrl_pressed()) {
        char upper_c = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
        if (upper_c == 'O') {
            save();
            refresh_screen();
            MesaOS::Drivers::VGADriver vga;
            vga.set_color(MesaOS::Drivers::vga_entry_color(MesaOS::Drivers::VGAColor::WHITE, MesaOS::Drivers::VGAColor::GREEN));
            vga.write_string("\n[ File saved successfully ]");
            return;
        } else if (upper_c == 'X') {
            running = false;
            MesaOS::Drivers::VGADriver vga;
            vga.initialize(); // Clear nano content on exit
            return;
        } else if (upper_c == 'G') {
            show_help = !show_help;
            refresh_screen();
            return;
        } else if (upper_c == 'K') {
            cut_line();
            refresh_screen();
            return;
        } else if (upper_c == 'U') {
            paste_line();
            refresh_screen();
            return;
        } else if (upper_c == 'C') {
            show_cursor_pos();
            return;
        } else if (upper_c == 'W') {
            search();
            return;
        }
    }

    if (show_help) return; // Don't type while help is open

    if (c == 27) { // ESC still works as a fallback
        save();
        running = false;
        return;
    }

    if (c == '\b') {
        if (cursor_pos > 0) {
            buffer[--cursor_pos] = '\0';
        }
    } else if (cursor_pos < sizeof(buffer) - 1) {
        buffer[cursor_pos++] = c;
        buffer[cursor_pos] = '\0';
    }
    
    refresh_screen();
}

bool Nano::is_running() {
    return running;
}

} // namespace MesaOS::Apps
