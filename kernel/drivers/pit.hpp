#ifndef PIT_HPP
#define PIT_HPP

#include "arch/i386/isr.hpp"

namespace MesaOS::Drivers {

class PIT {
public:
    static void initialize(uint32_t frequency);
    static void callback(MesaOS::Arch::x86::Registers* regs);
    static uint32_t get_ticks();

private:
    static uint32_t ticks;
};

} // namespace MesaOS::Drivers

#endif
