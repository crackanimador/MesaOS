#include "keyboard.hpp"
#include "arch/i386/io_port.hpp"
#include "drivers/vga.hpp"
#include "shell.hpp"

namespace MesaOS::Drivers {

bool Keyboard::shift_pressed = false;
bool Keyboard::ctrl_pressed = false;
bool Keyboard::alt_pressed = false;
bool Keyboard::e0_escape = false;

static const char kbd_us[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

static const char kbd_us_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

void Keyboard::initialize() {
    MesaOS::Arch::x86::register_irq_handler(IRQ1, Keyboard::callback);
}

void Keyboard::callback(MesaOS::Arch::x86::Registers* regs) {
    (void)regs;
    uint8_t scancode = MesaOS::Arch::x86::inb(0x60);
    handle_scancode(scancode);
}

bool Keyboard::is_ctrl_pressed() {
    return ctrl_pressed;
}

void Keyboard::handle_scancode(uint8_t scancode) {
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }
    if (scancode == 0xE0) {
        e0_escape = true;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = true;
        e0_escape = false;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = false;
        e0_escape = false;
        return;
    }
    
    // Extended keys (Arrows)
    if (e0_escape) {
        if (scancode == 0x48) { // Up
            if (ctrl_pressed) {
                // Ctrl+Up = Scroll Up
                MesaOS::Drivers::VGADriver::scroll_up();
            } else {
                // Normal Up = History
                MesaOS::System::Shell::handle_input(0x11);
            }
        } else if (scancode == 0x50) { // Down
            if (ctrl_pressed) {
                // Ctrl+Down = Scroll Down
                MesaOS::Drivers::VGADriver::scroll_down();
            } else {
                // Normal Down = History
                MesaOS::System::Shell::handle_input(0x12);
            }
        }
        e0_escape = false;
        return;
    }
    
    if (scancode == 0x38) { alt_pressed = true; return; }
    if (scancode == 0xB8) { alt_pressed = false; return; }

    if (alt_pressed) {
        if (scancode >= 0x3B && scancode <= 0x3E) {
            // F1 (0x3B) -> TTY 0
            // F2 (0x3C) -> TTY 1 ...
            MesaOS::Drivers::VGADriver::switch_tty(scancode - 0x3B);
            return;
        }
    }
    
    e0_escape = false; // Reset if it wasn't a handled e0 sequence

    // Check Ctrl+C
    if (ctrl_pressed && scancode == 0x2E) {
        MesaOS::System::Shell::handle_signal(2); // SIGINT
        return;
    }

    if (scancode & 0x80) {
        // Key released
    } else {
        // Key pressed
        if (scancode < sizeof(kbd_us)) {
            char c = shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
            if (c != 0) {
                MesaOS::System::Shell::handle_input(c);
            }
        }
    }
}

} // namespace MesaOS::Drivers
