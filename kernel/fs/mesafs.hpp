#ifndef MESAFS_HPP
#define MESAFS_HPP

#include <stdint.h>
#include "vfs.hpp"
#include "crypto.hpp"

namespace MesaOS::FS {

struct MesaFSHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t root_dir_lba;
    uint32_t total_blocks;
    uint8_t metadata_hmac[32]; // HMAC for encrypted metadata
};

struct MesaFSEntry {
    char name[64];
    uint32_t lba_start;
    uint32_t length;
    uint32_t uid;      // Owner user ID
    uint32_t gid;      // Owner group ID
    uint32_t mode;     // File permissions (Unix-style)
    uint8_t flags;     // 1 = File, 2 = Directory
    uint8_t present;
    uint8_t hmac[32];  // HMAC for this entry's data
};

class MesaFS {
public:
    static fs_node* initialize(uint32_t partition_lba);
    static fs_node* create_file(const char* name, uint32_t uid = 0, uint32_t gid = 0, uint32_t mode = 0644);
    static fs_node* create_dir(const char* name, uint32_t uid = 0, uint32_t gid = 0, uint32_t mode = 0755);
    static uint32_t read(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static uint32_t write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static dirent* readdir(fs_node* node, uint32_t index);
    static fs_node* finddir(fs_node* node, const char* name);
    static bool check_permission(fs_node* node, uint32_t uid, uint32_t required_perm);

private:
    static uint32_t base_lba;
    static fs_node* root_node;
    static bool metadata_valid; // Track if metadata integrity is verified

    static bool load_metadata();
    static bool save_metadata();
    static bool verify_entry_integrity(MesaFSEntry* entry);
};

} // namespace MesaOS::FS

#endif
