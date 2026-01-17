#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#include "arch/i386/isr.hpp"

namespace MesaOS::Drivers {

class Keyboard {
public:
    static void initialize();
    static void callback(MesaOS::Arch::x86::Registers* regs);
    static bool is_ctrl_pressed();

private:
    static bool shift_pressed;
    static bool ctrl_pressed;
    static bool alt_pressed;
    static bool e0_escape;
    static void handle_scancode(uint8_t scancode);
};

} // namespace MesaOS::Drivers

#endif
