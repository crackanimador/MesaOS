#ifndef MBR_HPP
#define MBR_HPP

#include <stdint.h>

namespace MesaOS::FS {

struct PartitionEntry {
    uint8_t attributes;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed));

struct MBR {
    uint8_t bootstrap[446];
    PartitionEntry partitions[4];
    uint16_t signature;
} __attribute__((packed));

class PartitionManager {
public:
    static void initialize();
    static void list_partitions();
};

} // namespace MesaOS::FS

#endif
