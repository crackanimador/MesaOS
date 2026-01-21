#include "kheap.hpp"
#include <string.h>
#include "panic.hpp"
#include "scheduler.hpp"
#include "logging.hpp"

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
    void* ptr = MesaOS::Memory::KHeap::malloc(size);
    if (!ptr) {
        // OOM Killer: Try to free memory by killing processes
        MesaOS::System::Logging::warn("Kernel heap allocation failed - activating OOM Killer");

        // Get current process
        MesaOS::System::Process* current_proc = MesaOS::System::Scheduler::get_current();
        if (current_proc && current_proc->pid != 0) { // Don't kill kernel
            MesaOS::System::Logging::info("OOM Killer terminating current process");
            MesaOS::System::Scheduler::remove_process(current_proc->pid);

            // Try allocation again after killing a process
            ptr = MesaOS::Memory::KHeap::malloc(size);
        }

        if (!ptr) {
            // Still no memory - kernel panic
            MesaOS::System::KernelPanic::panic("Out of kernel memory - OOM Killer failed");
        }
    }
    return ptr;
}

extern "C" void kfree(void* ptr) {
    MesaOS::Memory::KHeap::free(ptr);
}
