#ifndef GDT64_HPP
#define GDT64_HPP

#include <stdint.h>

namespace MesaOS::Arch::x64 {

struct GDT64Entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GDT64Pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

class GDT64 {
public:
    static void initialize();
};

} // namespace MesaOS::Arch::x64

#endif
