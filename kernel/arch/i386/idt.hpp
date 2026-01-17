#ifndef IDT_HPP
#define IDT_HPP

#include <stdint.h>

namespace MesaOS::Arch::x86 {

struct IDTEntry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct IDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

class IDT {
public:
    static void initialize();
    static void set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);

private:
    static IDTEntry entries[256];
    static IDTPointer pointer;
};

} // namespace MesaOS::Arch::x86

#endif
