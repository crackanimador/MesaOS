#include "pit.hpp"
#include "arch/i386/io_port.hpp"
#include "scheduler.hpp"

namespace MesaOS::Drivers {

uint32_t PIT::ticks = 0;

void PIT::initialize(uint32_t frequency) {
    // Register our timer callback
    MesaOS::Arch::x86::register_irq_handler(IRQ0, PIT::callback);

    // The value we send to the PIT is the value to divide its input clock
    // (1193180 Hz) by, to get our required frequency.
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte.
    MesaOS::Arch::x86::outb(0x43, 0x36);

    // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    // Send the frequency divisor.
    MesaOS::Arch::x86::outb(0x40, l);
    MesaOS::Arch::x86::outb(0x40, h);
}

void PIT::callback(MesaOS::Arch::x86::Registers* regs) {
    (void)regs;
    ticks++;
}

uint32_t PIT::get_ticks() {
    return ticks;
}

} // namespace MesaOS::Drivers
