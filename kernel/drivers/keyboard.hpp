#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#include <stdint.h>
#include "arch/i386/idt.hpp"  // Para Registers
#include "arch/i386/isr.hpp"  // Para IRQ1

namespace MesaOS::Drivers {

class Keyboard {
public:
    static void initialize();
    static void callback(MesaOS::Arch::x86::Registers* regs);
    static bool is_ctrl_pressed();

    // Input handler system
    typedef void (*InputHandler)(char c);
    static void set_input_handler(InputHandler handler);
    static void set_dummy_handler(); // Set handler that ignores input

private:
    static void handle_scancode(uint8_t scancode);
    static InputHandler current_input_handler;
    static bool shift_pressed;
    static bool ctrl_pressed;
    static bool alt_pressed;
    static bool e0_escape;
};

} // namespace MesaOS::Drivers

#endif
