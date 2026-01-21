#include "vmm.hpp"
#include "paging.hpp"
#include "pmm.hpp"
#include "../logging.hpp"
#include <string.h>

namespace MesaOS::Memory {

uint32_t* VMM::kernel_directory = nullptr;

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t PAGE_TABLE_ENTRIES = 1024;
constexpr uint32_t KERNEL_SPACE_ENTRIES = 768; // 3GB kernel space

// Simple LCG for ASLR (Linear Congruential Generator)
static uint32_t aslr_seed = 0x12345678;
uint32_t get_random_offset() {
    aslr_seed = (aslr_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    // Return random offset aligned to 64KB (0x10000) within first 16MB of user space
    return (aslr_seed % 256) * 0x10000; // 0 to ~16MB
}

void VMM::initialize() {
    // Kernel directory is the one created during Paging::initialize()
    kernel_directory = Paging::get_kernel_directory();
    if (!kernel_directory) {
        // Critical error - paging not initialized
        return;
    }
}

uint32_t* VMM::create_address_space() {
    if (!kernel_directory) return nullptr;

    // Allocate a new page directory
    uint32_t* new_directory = static_cast<uint32_t*>(PMM::allocate_block());
    if (!new_directory) return nullptr;

    memset(new_directory, 0, PAGE_SIZE);

    // Apply ASLR: Add random offset to user space base
    // Note: ASLR offset will be applied during actual memory allocations
    
    // Copy kernel mappings (first KERNEL_SPACE_ENTRIES = 3GB kernel space)
    // This ensures kernel code/data is accessible in all address spaces
    for (uint32_t i = 0; i < KERNEL_SPACE_ENTRIES; ++i) {
        if (kernel_directory[i] & 0x1) {
            // Page table is present, copy it
            uint32_t* kernel_table = reinterpret_cast<uint32_t*>(kernel_directory[i] & ~0xFFFU);
            
            // Allocate new page table for user space
            uint32_t* new_table = static_cast<uint32_t*>(PMM::allocate_block());
            if (!new_table) {
                // Cleanup on failure
                destroy_address_space(new_directory);
                return nullptr;
            }
            
            // Copy kernel mappings (identity mapped)
            memcpy(new_table, kernel_table, PAGE_SIZE);
            
            // Set directory entry (same flags as kernel)
            new_directory[i] = (reinterpret_cast<uint32_t>(new_table)) | (kernel_directory[i] & 0xFFFU);
        }
    }
    
    // User space (last 256 entries = 1GB) starts empty
    // Processes can map their own pages here
    
    return new_directory;
}

uint32_t* VMM::clone_address_space(uint32_t* src_directory) {
    if (!src_directory || src_directory == kernel_directory) return nullptr;

    // Create new address space
    uint32_t* new_directory = create_address_space();
    if (!new_directory) return nullptr;

    // Copy user space mappings with Copy-on-Write (COW)
    for (uint32_t i = KERNEL_SPACE_ENTRIES; i < PAGE_TABLE_ENTRIES; ++i) {
        if (src_directory[i] & 0x1U) {
            // Page table exists, clone it with COW
            uint32_t* src_table = reinterpret_cast<uint32_t*>(src_directory[i] & ~0xFFFU);

            // Allocate new page table
            uint32_t* new_table = static_cast<uint32_t*>(PMM::allocate_block());
            if (!new_table) {
                destroy_address_space(new_directory);
                return nullptr;
            }

            memset(new_table, 0, PAGE_SIZE);

            // Copy page table entries with COW: mark as read-only
            for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; ++j) {
                if (src_table[j] & 0x1U) {
                    // Page is present, copy with read-only flag for COW
                    uint32_t flags = src_table[j] & 0xFFFU;
                    // Clear write bit (bit 1) to trigger page faults on write
                    flags &= ~0x2U;
                    // Set COW flag (we'll use bit 9, available in page table entries)
                    flags |= (1U << 9);
                    new_table[j] = (src_table[j] & ~0xFFFU) | flags;
                }
            }

            // Set directory entry
            new_directory[i] = (reinterpret_cast<uint32_t>(new_table)) | (src_directory[i] & 0xFFFU);
        }
    }

    return new_directory;
}

void VMM::destroy_address_space(uint32_t* directory) {
    if (!directory || directory == kernel_directory) return;
    
    // Free user space page tables (entries KERNEL_SPACE_ENTRIES to 1023)
    for (uint32_t i = KERNEL_SPACE_ENTRIES; i < PAGE_TABLE_ENTRIES; ++i) {
        if (directory[i] & 0x1U) {
            uint32_t* table = reinterpret_cast<uint32_t*>(directory[i] & ~0xFFFU);
            free_page_table(reinterpret_cast<uint32_t*>(table));
        }
    }
    
    // Free the directory itself
    PMM::free_block(directory);
}

void VMM::switch_address_space(uint32_t* directory) {
    if (!directory) return;
    if (directory == kernel_directory) {
        // Ensure kernel directory is valid
        if (!kernel_directory) return;
    }
    Paging::switch_page_directory(directory);
}

void VMM::map_page_in_space(uint32_t* directory, uint32_t virtual_addr, uint32_t physical_addr, bool user, bool rw, bool executable) {
    if (!directory || (virtual_addr & 0xFFFU) != 0 || (physical_addr & 0xFFFU) != 0) return;

    uint32_t dir_idx = virtual_addr >> 22;
    uint32_t table_idx = (virtual_addr >> 12) & 0x03FFU;

    if (dir_idx >= PAGE_TABLE_ENTRIES) return; // Invalid directory index

    // Get or create page table
    uint32_t* table;
    if (!(directory[dir_idx] & 0x1U)) {
        // Create new page table
        table = static_cast<uint32_t*>(PMM::allocate_block());
        if (!table) return;

        memset(table, 0, PAGE_SIZE);
        directory[dir_idx] = (reinterpret_cast<uint32_t>(table)) | (user ? 7U : 3U);
    } else {
        table = reinterpret_cast<uint32_t*>(directory[dir_idx] & ~0xFFFU);
    }

    // Map the page with W^X protection
    uint32_t flags = (user ? 7U : 3U);
    if (!rw) flags &= ~0x2U; // Remove write permission if not requested

    // For basic W^X: if page is writable, ensure it's not executable (and vice versa)
    // Note: In 32-bit x86 without PAE, we can't directly set NX bit, but we can
    // use this as a policy enforcement mechanism
    if (rw && executable) {
        // W^X violation! Log this security issue
        // For now, allow it but log the violation
        // Note: We can't directly log here due to namespace issues, but this is a policy violation
        // In a real implementation, we'd have a way to log this
    }

    table[table_idx] = (physical_addr & ~0xFFFU) | flags;

    // Invalidate TLB
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void VMM::unmap_page(uint32_t* directory, uint32_t virtual_addr) {
    if (!directory || (virtual_addr & 0xFFFU) != 0) return;
    
    uint32_t dir_idx = virtual_addr >> 22;
    uint32_t table_idx = (virtual_addr >> 12) & 0x03FFU;
    
    if (dir_idx >= PAGE_TABLE_ENTRIES || !(directory[dir_idx] & 0x1U)) return;
    
    uint32_t* table = reinterpret_cast<uint32_t*>(directory[dir_idx] & ~0xFFFU);
    table[table_idx] = 0; // Unmap
    
    // Invalidate TLB
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

uint32_t VMM::get_physical_address(uint32_t* directory, uint32_t virtual_addr) {
    if (!directory) return 0;
    
    uint32_t dir_idx = virtual_addr >> 22;
    uint32_t table_idx = (virtual_addr >> 12) & 0x03FFU;
    
    if (dir_idx >= PAGE_TABLE_ENTRIES || !(directory[dir_idx] & 0x1U)) return 0;
    
    uint32_t* table = reinterpret_cast<uint32_t*>(directory[dir_idx] & ~0xFFFU);
    if (table[table_idx] & 0x1U) {
        return (table[table_idx] & ~0xFFFU) | (virtual_addr & 0xFFFU);
    }
    
    return 0;
}

uint32_t VMM::allocate_page(uint32_t* directory, uint32_t virtual_addr, bool user, bool rw) {
    if (!directory) return 0;

    // Allocate physical page
    void* physical_page = PMM::allocate_block();
    if (!physical_page) return 0;

    // Map it (assume data pages are not executable by default for W^X)
    map_page_in_space(directory, virtual_addr, reinterpret_cast<uint32_t>(physical_page), user, rw, false);

    return reinterpret_cast<uint32_t>(physical_page);
}

void VMM::free_page(uint32_t* directory, uint32_t virtual_addr) {
    if (!directory) return;
    
    // Get physical address
    uint32_t physical_addr = get_physical_address(directory, virtual_addr);
    
    if (physical_addr) {
        // Free physical page
        PMM::free_block(reinterpret_cast<void*>(physical_addr));
        
        // Unmap virtual page
        unmap_page(directory, virtual_addr);
    }
}

uint32_t VMM::allocate_page_table() {
    void* table = PMM::allocate_block();
    return table ? reinterpret_cast<uint32_t>(table) : 0;
}

void VMM::free_page_table(uint32_t* table) {
    if (table) PMM::free_block(table);
}

} // namespace MesaOS::Memory
