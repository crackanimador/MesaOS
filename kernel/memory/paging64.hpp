#ifndef PAGING64_HPP
#define PAGING64_HPP

#include <stdint.h>

namespace MesaOS::Memory::x64 {

class Paging64 {
public:
    static void initialize();
    static void switch_to_long_mode();

private:
    static uint64_t* pml4;
};

} // namespace MesaOS::Memory::x64

#endif
