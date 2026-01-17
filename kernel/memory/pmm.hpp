#ifndef PMM_HPP
#define PMM_HPP

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

namespace MesaOS::Memory {

class PMM {
public:
    static void initialize(uint32_t start_addr, uint32_t size);
    static void* allocate_block();
    static void free_block(void* addr);
    static void load_memory_map(struct ::multiboot_info* mbt);
    
    static void set_block(uint32_t bit);
    static void unset_block(uint32_t bit);
    static bool test_block(uint32_t bit);

private:
    static uint32_t* bitmap;
    static uint32_t max_blocks;
    static uint32_t used_blocks;

    static int first_free_block();
};

} // namespace MesaOS::Memory

#endif
