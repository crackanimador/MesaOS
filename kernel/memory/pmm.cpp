#include "pmm.hpp"
#include <string.h>

struct multiboot_mmap_entry {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((packed));

namespace MesaOS::Memory {

uint32_t* PMM::bitmap = 0;
uint32_t PMM::max_blocks = 0;
uint32_t PMM::used_blocks = 0;

#define BLOCK_SIZE 4096
#define BLOCKS_PER_BUCKET 32

void PMM::set_block(uint32_t bit) {
    if (!test_block(bit)) {
        bitmap[bit / 32] |= (1 << (bit % 32));
        used_blocks++;
    }
}

void PMM::unset_block(uint32_t bit) {
    if (test_block(bit)) {
        bitmap[bit / 32] &= ~(1 << (bit % 32));
        used_blocks--;
    }
}

bool PMM::test_block(uint32_t bit) {
    return bitmap[bit / 32] & (1 << (bit % 32));
}

int PMM::first_free_block() {
    for (uint32_t i = 0; i < max_blocks / 32; i++) {
        if (bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                int bit = 1 << j;
                if (!(bitmap[i] & bit)) {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1;
}

void PMM::initialize(uint32_t start_addr, uint32_t size) {
    bitmap = (uint32_t*)start_addr;
    max_blocks = size / BLOCK_SIZE;
    used_blocks = max_blocks;

    // Initially mark everything as used
    memset(bitmap, 0xFF, max_blocks / 8);
}

void* PMM::allocate_block() {
    int free_block = first_free_block();
    if (free_block == -1) return 0;

    set_block(free_block);
    return (void*)(free_block * BLOCK_SIZE);
}

void PMM::free_block(void* addr) {
    uint32_t block = (uint32_t)addr / BLOCK_SIZE;
    unset_block(block);
}

void PMM::load_memory_map(struct ::multiboot_info* mbt) {
    multiboot_mmap_entry* mmap = (multiboot_mmap_entry*)mbt->mmap_addr;
    while ((uint32_t)mmap < mbt->mmap_addr + mbt->mmap_length) {
        if (mmap->type == 1) { // Available RAM
            uint32_t addr = mmap->addr_low;
            uint32_t len = mmap->len_low;
            for (uint32_t i = 0; i < len; i += BLOCK_SIZE) {
                free_block((void*)(addr + i));
            }
        }
        mmap = (multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    
    // Lock the first page (usually contains BIOS data)
    set_block(0);
}

} // namespace MesaOS::Memory
