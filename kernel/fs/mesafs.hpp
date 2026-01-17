#ifndef MESAFS_HPP
#define MESAFS_HPP

#include <stdint.h>
#include "vfs.hpp"

namespace MesaOS::FS {

struct MesaFSHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t root_dir_lba;
    uint32_t total_blocks;
};

struct MesaFSEntry {
    char name[64];
    uint32_t lba_start;
    uint32_t length;
    uint8_t flags; // 1 = File, 2 = Directory
    uint8_t present;
};

class MesaFS {
public:
    static fs_node* initialize(uint32_t partition_lba);
    static fs_node* create_file(const char* name);
    static fs_node* create_dir(const char* name);
    static uint32_t read(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static uint32_t write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static dirent* readdir(fs_node* node, uint32_t index);
    static fs_node* finddir(fs_node* node, const char* name);

private:
    static uint32_t base_lba;
    static fs_node* root_node;
};

} // namespace MesaOS::FS

#endif
