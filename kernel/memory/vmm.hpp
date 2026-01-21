#ifndef VMM_HPP
#define VMM_HPP

#include <stdint.h>

namespace MesaOS::Memory {

// Virtual Memory Manager - Manages per-process address spaces
class VMM {
public:
    static void initialize();
    
    // Create a new address space (page directory) for a process
    static uint32_t* create_address_space();
    
    // Clone an address space (for fork)
    static uint32_t* clone_address_space(uint32_t* src_directory);
    
    // Destroy an address space
    static void destroy_address_space(uint32_t* directory);
    
    // Switch to a different address space
    static void switch_address_space(uint32_t* directory);
    
    // Map a page in an address space
    static void map_page_in_space(uint32_t* directory, uint32_t virtual_addr, uint32_t physical_addr, bool user, bool rw, bool executable = false);
    
    // Unmap a page
    static void unmap_page(uint32_t* directory, uint32_t virtual_addr);
    
    // Get physical address from virtual (for kernel mappings)
    static uint32_t get_physical_address(uint32_t* directory, uint32_t virtual_addr);
    
    // Allocate a page in an address space
    static uint32_t allocate_page(uint32_t* directory, uint32_t virtual_addr, bool user, bool rw);
    
    // Free a page
    static void free_page(uint32_t* directory, uint32_t virtual_addr);
    
private:
    static uint32_t* kernel_directory; // Kernel's page directory (shared)
    static uint32_t allocate_page_table();
    static void free_page_table(uint32_t* table);
};

} // namespace MesaOS::Memory

#endif
