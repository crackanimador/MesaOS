#ifndef GDT_HPP
#define GDT_HPP

#include <stdint.h>

namespace MesaOS::Arch::x86 {

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

class GDT {
public:
    static void initialize();

private:
    static void set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
    
    static GDTEntry entries[5];
    static GDTPointer pointer;
};

} // namespace MesaOS::Arch::i386

#endif
