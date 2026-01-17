#ifndef KHEAP_HPP
#define KHEAP_HPP

#include <stdint.h>
#include <stddef.h>

namespace MesaOS::Memory {

class KHeap {
public:
    static void initialize(uint32_t start_addr, uint32_t size);
    static void* malloc(size_t size);
    static void free(void* ptr);

private:
    static uint32_t current_addr;
    static uint32_t end_addr;
};

} // namespace MesaOS::Memory

extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);

#endif
