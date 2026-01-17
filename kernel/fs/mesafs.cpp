#include "mesafs.hpp"
#include "drivers/ide.hpp"
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::FS {

uint32_t MesaFS::base_lba = 0;
fs_node* MesaFS::root_node = 0;

static MesaFSEntry entries[32]; // Flat registry for the first 32 files/dirs

fs_node* MesaFS::initialize(uint32_t partition_lba) {
    base_lba = partition_lba;
    
    // Read the "Registry" sector (base_lba + 1)
    uint8_t buffer[512];
    MesaOS::Drivers::IDEDriver::read_sector(base_lba + 1, buffer);
    memcpy(entries, buffer, sizeof(entries));

    root_node = (fs_node*)kmalloc(sizeof(fs_node));
    memset(root_node, 0, sizeof(fs_node));
    strcpy(root_node->name, "disk");
    root_node->flags = FS_DIRECTORY;
    root_node->readdir = &MesaFS::readdir;
    root_node->finddir = &MesaFS::finddir;
    
    return root_node;
}

static dirent static_de;
dirent* MesaFS::readdir(fs_node* node, uint32_t index) {
    (void)node;
    if (index >= 32) return 0;
    if (!entries[index].present) return 0;

    strcpy(static_de.name, entries[index].name);
    static_de.ino = index;
    return &static_de;
}

fs_node* MesaFS::finddir(fs_node* node, const char* name) {
    (void)node;
    for (int i = 0; i < 32; i++) {
        if (entries[i].present && strcmp(entries[i].name, name) == 0) {
            fs_node* res = (fs_node*)kmalloc(sizeof(fs_node));
            memset(res, 0, sizeof(fs_node));
            strcpy(res->name, entries[i].name);
            res->inode = i;
            res->flags = (entries[i].flags == 2) ? FS_DIRECTORY : FS_FILE;
            res->read = &MesaFS::read;
            res->write = &MesaFS::write;
            res->readdir = &MesaFS::readdir;
            res->finddir = &MesaFS::finddir;
            return res;
        }
    }
    return 0;
}

uint32_t MesaFS::read(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t entry_idx = node->inode;
    MesaFSEntry& entry = entries[entry_idx];
    
    // Read sectors from disk
    uint32_t start_sector = base_lba + entry.lba_start;
    uint32_t num_sectors = (entry.length + 511) / 512;
    
    uint8_t sector_buf[512];
    uint32_t bytes_read = 0;
    
    for (uint32_t i = 0; i < num_sectors && bytes_read < size; i++) {
        MesaOS::Drivers::IDEDriver::read_sector(start_sector + i, sector_buf);
        uint32_t to_copy = 512;
        if (size - bytes_read < 512) to_copy = size - bytes_read;
        memcpy(buffer + bytes_read, sector_buf, to_copy);
        bytes_read += to_copy;
    }
    
    return bytes_read;
}

uint32_t MesaFS::write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t entry_idx = node->inode;
    MesaFSEntry& entry = entries[entry_idx];
    
    uint32_t start_sector = base_lba + entry.lba_start;
    uint8_t sector_buf[512];
    uint32_t bytes_written = 0;
    
    for (uint32_t i = 0; i < (size + 511) / 512; i++) {
        memset(sector_buf, 0, 512);
        uint32_t to_copy = 512;
        if (size - bytes_written < 512) to_copy = size - bytes_written;
        memcpy(sector_buf, buffer + bytes_written, to_copy);
        
        MesaOS::Drivers::IDEDriver::write_sector(start_sector + i, sector_buf);
        bytes_written += to_copy;
    }
    
    entry.length = size;
    // Commit registry to disk
    MesaOS::Drivers::IDEDriver::write_sector(base_lba + 1, (uint8_t*)entries);
    
    return bytes_written;
}

fs_node* MesaFS::create_file(const char* name) {
    for (int i = 0; i < 32; i++) {
        if (!entries[i].present) {
            entries[i].present = 1;
            strcpy(entries[i].name, name);
            entries[i].lba_start = 100 + (i * 10); // Simple fixed allocation
            entries[i].length = 0;
            entries[i].flags = 1; // File

            MesaOS::Drivers::IDEDriver::write_sector(base_lba + 1, (uint8_t*)entries);

            fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
            memset(node, 0, sizeof(fs_node));
            strcpy(node->name, name);
            node->inode = i;
            node->flags = FS_FILE;
            node->read = &MesaFS::read;
            node->write = &MesaFS::write;
            return node;
        }
    }
    return 0;
}

fs_node* MesaFS::create_dir(const char* name) {
    for (int i = 0; i < 32; i++) {
        if (!entries[i].present) {
            entries[i].present = 1;
            strcpy(entries[i].name, name);
            entries[i].lba_start = 0; // Directories don't need blocks in this flat model
            entries[i].length = 0;
            entries[i].flags = 2; // Directory

            MesaOS::Drivers::IDEDriver::write_sector(base_lba + 1, (uint8_t*)entries);

            fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
            memset(node, 0, sizeof(fs_node));
            strcpy(node->name, name);
            node->inode = i;
            node->flags = FS_DIRECTORY;
            node->readdir = &MesaFS::readdir;
            node->finddir = &MesaFS::finddir;
            return node;
        }
    }
    return 0;
}

} // namespace MesaOS::FS
