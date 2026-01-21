#include "paging.hpp"
#include "pmm.hpp"
#include <string.h>

namespace MesaOS::Memory {

uint32_t* Paging::page_directory = 0;

void Paging::initialize() {
    // Allocate a page-aligned page directory
    page_directory = (uint32_t*)PMM::allocate_block();
    memset(page_directory, 0, 4096);

    // Identity map the first 64MB (16 page tables)
    for (uint32_t j = 0; j < 16; j++) {
        uint32_t* page_table = (uint32_t*)PMM::allocate_block();
        memset(page_table, 0, 4096);

        for (uint32_t i = 0; i < 1024; i++) {
            // Physical address = (j * 1024 + i) * 4096
            page_table[i] = ((j * 1024 + i) * 4096) | 3; 
        }

        page_directory[j] = ((uint32_t)page_table) | 3;
    }

    // Switch and Enable Paging
    switch_page_directory(page_directory);
    
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Enable paging bit
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void Paging::switch_page_directory(uint32_t* directory) {
    asm volatile("mov %0, %%cr3" : : "r"(directory));
}

void Paging::map_page(uint32_t virtual_addr, uint32_t physical_addr, bool user, bool rw) {
    uint32_t dir_idx = virtual_addr >> 22;
    uint32_t table_idx = (virtual_addr >> 12) & 0x03FF;

    uint32_t* table;
    if (!(page_directory[dir_idx] & 0x1)) {
        // Create table if not present
        table = (uint32_t*)PMM::allocate_block();
        memset(table, 0, 4096);
        page_directory[dir_idx] = ((uint32_t)table) | (user ? 7 : 3);
    } else {
        table = (uint32_t*)(page_directory[dir_idx] & ~0xFFF);
    }

    table[table_idx] = (physical_addr & ~0xFFF) | (user ? 7 : 3);
    if (!rw) table[table_idx] &= ~0x02;
    
    // Invalidate TLB for this address
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

uint32_t* Paging::get_kernel_directory() {
    return page_directory;
}

} // namespace MesaOS::Memory
