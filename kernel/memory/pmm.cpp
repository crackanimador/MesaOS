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

uint32_t* PMM::bitmap = nullptr;
uint32_t PMM::max_blocks = 0;
uint32_t PMM::used_blocks = 0;

constexpr uint32_t BLOCK_SIZE = 4096;
constexpr uint32_t BLOCKS_PER_BUCKET = 32;

void PMM::set_block(uint32_t bit) {
    if (!bitmap || bit >= max_blocks) return;
    if (!test_block(bit)) {
        bitmap[bit / BLOCKS_PER_BUCKET] |= (1U << (bit % BLOCKS_PER_BUCKET));
        used_blocks++;
    }
}

void PMM::unset_block(uint32_t bit) {
    if (!bitmap || bit >= max_blocks) return;
    if (test_block(bit)) {
        bitmap[bit / BLOCKS_PER_BUCKET] &= ~(1U << (bit % BLOCKS_PER_BUCKET));
        used_blocks--;
    }
}

bool PMM::test_block(uint32_t bit) {
    if (!bitmap || bit >= max_blocks) return false;
    return bitmap[bit / BLOCKS_PER_BUCKET] & (1U << (bit % BLOCKS_PER_BUCKET));
}

int PMM::first_free_block() {
    if (!bitmap || max_blocks == 0) return -1;
    uint32_t num_buckets = max_blocks / BLOCKS_PER_BUCKET;
    for (uint32_t i = 0; i < num_buckets; ++i) {
        if (bitmap[i] != 0xFFFFFFFFU) {
            for (uint32_t j = 0; j < BLOCKS_PER_BUCKET; ++j) {
                uint32_t bit = 1U << j;
                if (!(bitmap[i] & bit)) {
                    uint32_t block = i * BLOCKS_PER_BUCKET + j;
                    if (block < max_blocks) {
                        return static_cast<int>(block);
                    }
                }
            }
        }
    }
    return -1;
}

void PMM::initialize(uint32_t start_addr, uint32_t size) {
    if (start_addr == 0 || size < BLOCK_SIZE) return; // Invalid parameters
    bitmap = reinterpret_cast<uint32_t*>(start_addr);
    max_blocks = size / BLOCK_SIZE;
    used_blocks = max_blocks;

    // Initially mark everything as used
    memset(bitmap, 0xFF, (max_blocks + 7) / 8); // Ensure full coverage

    // Lock the bitmap blocks themselves
    uint32_t bitmap_blocks = (max_blocks + BLOCKS_PER_BUCKET - 1) / BLOCKS_PER_BUCKET;
    uint32_t bitmap_start_block = start_addr / BLOCK_SIZE;
    for (uint32_t i = 0; i < bitmap_blocks; ++i) {
        if (bitmap_start_block + i < max_blocks) {
            set_block(bitmap_start_block + i);
        }
    }
}

void* PMM::allocate_block() {
    if (!bitmap) return nullptr;
    int free_block = first_free_block();
    if (free_block == -1 || static_cast<uint32_t>(free_block) >= max_blocks) return nullptr;

    set_block(static_cast<uint32_t>(free_block));
    return reinterpret_cast<void*>(static_cast<uintptr_t>(free_block) * BLOCK_SIZE);
}

void PMM::free_block(void* addr) {
    if (!addr || !bitmap) return;
    uintptr_t addr_val = reinterpret_cast<uintptr_t>(addr);
    if (addr_val % BLOCK_SIZE != 0) return; // Not aligned
    uint32_t block = static_cast<uint32_t>(addr_val / BLOCK_SIZE);
    if (block >= max_blocks) return; // Out of bounds
    unset_block(block);
}

void PMM::load_memory_map(struct ::multiboot_info* mbt) {
    if (!mbt || !mbt->mmap_addr || mbt->mmap_length == 0) return;
    
    multiboot_mmap_entry* mmap = reinterpret_cast<multiboot_mmap_entry*>(mbt->mmap_addr);
    uintptr_t end_addr = static_cast<uintptr_t>(mbt->mmap_addr) + mbt->mmap_length;
    
    while (reinterpret_cast<uintptr_t>(mmap) + sizeof(multiboot_mmap_entry) <= end_addr) {
        if (mmap->type == 1 && mmap->len_low > 0) { // Available RAM
            uint32_t addr = mmap->addr_low;
            uint32_t len = mmap->len_low;
            for (uint32_t i = 0; i < len && addr + i < 0xFFFFFFFFU - BLOCK_SIZE; i += BLOCK_SIZE) {
                free_block(reinterpret_cast<void*>(addr + i));
            }
        }
        uintptr_t next_addr = reinterpret_cast<uintptr_t>(mmap) + mmap->size + sizeof(mmap->size);
        if (next_addr <= reinterpret_cast<uintptr_t>(mmap)) break; // Prevent infinite loop
        mmap = reinterpret_cast<multiboot_mmap_entry*>(next_addr);
    }
    
    // Lock the first page (usually contains BIOS data)
    set_block(0);
}

} // namespace MesaOS::Memory
