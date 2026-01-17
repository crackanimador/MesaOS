#include "gdt.hpp"

namespace MesaOS::Arch::x64 {

// The GDT itself is usually defined in assembly for the boot process, 
// but we can redefine it here specifically for 64-bit mode tasks.

void GDT64::initialize() {
    // 64-bit GDT is much simpler than 32-bit because many fields are ignored.
    // The base and limit are ignored for code/data segments.
    // We only need correct Access and Flags (Long Mode bit).
}

} // namespace MesaOS::Arch::x64
