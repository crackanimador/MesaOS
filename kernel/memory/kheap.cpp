#include "kheap.hpp"
#include <string.h>

namespace MesaOS::Memory {

uint32_t KHeap::current_addr = 0;
uint32_t KHeap::end_addr = 0;

void KHeap::initialize(uint32_t start_addr, uint32_t size) {
    current_addr = start_addr;
    end_addr = start_addr + size;
}

void* KHeap::malloc(size_t size) {
    // Basic alignment
    if (current_addr % 8 != 0) {
        current_addr += (8 - (current_addr % 8));
    }

    if (current_addr + size > end_addr) {
        return 0; // Out of memory
    }

    void* ptr = (void*)current_addr;
    current_addr += size;
    
    // Clear the memory
    memset(ptr, 0, size);
    
    return ptr;
}

void KHeap::free(void* ptr) {
    (void)ptr;
    // Basic bump allocator doesn't free. 
    // In a 'real' OS we would use a more complex allocator, 
    // but for now, this allows multiple small allocations.
}

} // namespace MesaOS::Memory

extern "C" void* kmalloc(size_t size) {
    return MesaOS::Memory::KHeap::malloc(size);
}

extern "C" void kfree(void* ptr) {
    MesaOS::Memory::KHeap::free(ptr);
}
