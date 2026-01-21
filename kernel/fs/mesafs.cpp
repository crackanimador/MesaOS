#include "mesafs.hpp"
#include "drivers/ide.hpp"
#include "memory/kheap.hpp"
#include "crypto.hpp"
#include <string.h>

namespace MesaOS::FS {

uint32_t MesaFS::base_lba = 0;
fs_node* MesaFS::root_node = 0;
bool MesaFS::metadata_valid = false;

static MesaFSEntry entries[8]; // Flat registry for 8 files/dirs (fits in 512-byte sector with header)

fs_node* MesaFS::initialize(uint32_t partition_lba) {
    base_lba = partition_lba;

    // Initialize encryption keys
    Crypto::KeyManager::initialize();

    // Try to load and verify metadata
    if (!load_metadata()) {
        // If metadata is invalid or doesn't exist, initialize with empty registry
        memset(entries, 0, sizeof(entries));
        metadata_valid = true; // Start with valid empty state
        // Don't save metadata immediately to avoid potential issues on first boot
    }

    root_node = (fs_node*)kmalloc(sizeof(fs_node));
    memset(root_node, 0, sizeof(fs_node));
    strcpy(root_node->name, "disk");
    root_node->flags = FS_DIRECTORY;
    root_node->uid = 0; // root
    root_node->gid = 0; // root
    root_node->mask = 0755; // rwxr-xr-x
    root_node->readdir = &MesaFS::readdir;
    root_node->finddir = &MesaFS::finddir;

    return root_node;
}

bool MesaFS::load_metadata() {
    // For now, always start fresh to avoid corrupted metadata issues
    // TODO: Re-enable metadata loading once filesystem is stable
    return false;
}

bool MesaFS::save_metadata() {
    if (!metadata_valid) return false;

    uint8_t buffer[512];
    memset(buffer, 0, 512);

    MesaFSHeader header;
    header.magic = 0x5341534D; // "MSAF"
    header.version = 1;
    header.root_dir_lba = 0; // Not used in flat model
    header.total_blocks = 1000; // Placeholder

    // Encrypt the metadata entries (simple encryption for now)
    uint8_t* encrypted_entries = buffer + sizeof(MesaFSHeader);
    size_t entries_size = sizeof(entries);

    memcpy(encrypted_entries, entries, entries_size);
    // Disable metadata encryption for now so user can see file names
    // Crypto::AES128::encrypt_buffer(encrypted_entries, entries_size, Crypto::KeyManager::get_master_key());

    // Copy header to buffer
    memcpy(buffer, &header, sizeof(MesaFSHeader));

    // Write the encrypted metadata sector
    MesaOS::Drivers::IDEDriver::write_sector(base_lba + 1, buffer);
    return true;
}

static dirent static_de;
dirent* MesaFS::readdir(fs_node* node, uint32_t index) {
    (void)node;
    if (index >= 8) return 0;
    if (!entries[index].present) return 0;

    strcpy(static_de.name, entries[index].name);
    static_de.ino = index;
    return &static_de;
}

fs_node* MesaFS::finddir(fs_node* node, const char* name) {
    (void)node;
    for (int i = 0; i < 8; i++) {
        if (entries[i].present && strcmp(entries[i].name, name) == 0) {
            fs_node* res = (fs_node*)kmalloc(sizeof(fs_node));
            memset(res, 0, sizeof(fs_node));
            strcpy(res->name, entries[i].name);
            res->inode = i;
            res->flags = (entries[i].flags == 2) ? FS_DIRECTORY : FS_FILE;
            res->uid = entries[i].uid;
            res->gid = entries[i].gid;
            res->mask = entries[i].mode;
            res->length = entries[i].length;
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

    if (offset >= entry.length) return 0;
    if (offset + size > entry.length) size = entry.length - offset;

    // Read sectors from disk
    uint32_t start_sector = base_lba + entry.lba_start;
    uint32_t file_offset = offset; // Track remaining offset within file
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t sector_index = file_offset / 512;
        uint32_t sector_offset = file_offset % 512;

        uint8_t sector_buf[512];
        MesaOS::Drivers::IDEDriver::read_sector(start_sector + sector_index, sector_buf);

        uint32_t remaining_in_sector = 512 - sector_offset;
        uint32_t to_copy = size - bytes_read;
        if (to_copy > remaining_in_sector) to_copy = remaining_in_sector;

        memcpy(buffer + bytes_read, sector_buf + sector_offset, to_copy);
        bytes_read += to_copy;
        file_offset += to_copy;
    }

    return bytes_read;
}

uint32_t MesaFS::write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t entry_idx = node->inode;
    MesaFSEntry& entry = entries[entry_idx];

    uint32_t start_sector = base_lba + entry.lba_start;
    uint32_t file_offset = offset; // Track offset within file
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t sector_index = file_offset / 512;
        uint32_t sector_offset = file_offset % 512;

        uint8_t sector_buf[512];
        // Read existing sector first (if we're not writing from offset 0)
        if (sector_offset > 0 || (size - bytes_written) < 512) {
            MesaOS::Drivers::IDEDriver::read_sector(start_sector + sector_index, sector_buf);
        } else {
            memset(sector_buf, 0, 512);
        }

        uint32_t to_copy = 512 - sector_offset;
        if (size - bytes_written < to_copy) to_copy = size - bytes_written;

        memcpy(sector_buf + sector_offset, buffer + bytes_written, to_copy);

        // Disable encryption for now so user can see files
        // Crypto::AES128::encrypt_buffer(sector_buf, 512, Crypto::KeyManager::get_master_key());

        MesaOS::Drivers::IDEDriver::write_sector(start_sector + sector_index, sector_buf);
        bytes_written += to_copy;
        file_offset += to_copy;
    }

    entry.length = (offset + size > entry.length) ? offset + size : entry.length;

    // Save updated metadata
    save_metadata();

    return bytes_written;
}

fs_node* MesaFS::create_file(const char* name, uint32_t uid, uint32_t gid, uint32_t mode) {
    for (int i = 0; i < 8; i++) {
        if (!entries[i].present) {
            entries[i].present = 1;
            strcpy(entries[i].name, name);
            entries[i].lba_start = 100 + (i * 10); // Simple fixed allocation
            entries[i].length = 0;
            entries[i].flags = 1; // File
            entries[i].uid = uid;
            entries[i].gid = gid;
            entries[i].mode = mode;
            memset(entries[i].hmac, 0, 32); // Initialize HMAC

            // Force save updated metadata immediately
            metadata_valid = true;
            if (!save_metadata()) {
                // If save fails, mark as invalid and return null
                entries[i].present = 0;
                return 0;
            }

            fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
            memset(node, 0, sizeof(fs_node));
            strcpy(node->name, name);
            node->inode = i;
            node->flags = FS_FILE;
            node->uid = uid;
            node->gid = gid;
            node->mask = mode;
            node->read = &MesaFS::read;
            node->write = &MesaFS::write;
            return node;
        }
    }
    return 0;
}

fs_node* MesaFS::create_dir(const char* name, uint32_t uid, uint32_t gid, uint32_t mode) {
    for (int i = 0; i < 8; i++) {
        if (!entries[i].present) {
            entries[i].present = 1;
            strcpy(entries[i].name, name);
            entries[i].lba_start = 0; // Directories don't need blocks in this flat model
            entries[i].length = 0;
            entries[i].flags = 2; // Directory
            entries[i].uid = uid;
            entries[i].gid = gid;
            entries[i].mode = mode;
            memset(entries[i].hmac, 0, 32); // Initialize HMAC

            // Save updated metadata
            save_metadata();

            fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
            memset(node, 0, sizeof(fs_node));
            strcpy(node->name, name);
            node->inode = i;
            node->flags = FS_DIRECTORY;
            node->uid = uid;
            node->gid = gid;
            node->mask = mode;
            node->readdir = &MesaFS::readdir;
            node->finddir = &MesaFS::finddir;
            return node;
        }
    }
    return 0;
}

bool MesaFS::verify_entry_integrity(MesaFSEntry* entry) {
    if (!entry->present) return true; // Empty entries are valid

    // For files with data, verify HMAC
    if (entry->length > 0 && entry->flags == 1) {
        // Read the file data and verify HMAC
        uint32_t start_sector = base_lba + entry->lba_start;
        uint32_t num_sectors = (entry->length + 511) / 512;
        uint8_t file_data[4096]; // Max file size for verification
        uint32_t data_read = 0;

        for (uint32_t i = 0; i < num_sectors && data_read < sizeof(file_data); i++) {
            uint8_t sector_buf[512];
            MesaOS::Drivers::IDEDriver::read_sector(start_sector + i, sector_buf);

            // Decrypt sector
            if (!Crypto::AuthenticatedAES::decrypt_buffer(sector_buf, 512,
                                                          Crypto::KeyManager::get_master_key(),
                                                          entry->hmac)) {
                return false;
            }

            uint32_t to_copy = 512;
            if (data_read + to_copy > entry->length) to_copy = entry->length - data_read;
            memcpy(file_data + data_read, sector_buf, to_copy);
            data_read += to_copy;
        }

        // Verify HMAC over the decrypted data
        return Crypto::HMAC_SHA256::verify(Crypto::KeyManager::get_hmac_key(), 32,
                                          file_data, entry->length, entry->hmac);
    }

    return true; // Directories or empty files are considered valid
}

bool MesaFS::check_permission(fs_node* node, uint32_t uid, uint32_t required_perm) {
    if (!node) return false;

    // Root can do anything
    if (uid == 0) return true;

    // Check if user owns the file
    if (node->uid == uid) {
        // Check user permissions
        return (node->mask & (required_perm << 6)) != 0;
    }

    // Check group permissions
    // For now, simplified - just check if user is in the group
    if (node->gid == 0) { // Assume gid 0 is accessible to all
        return (node->mask & (required_perm << 3)) != 0;
    }

    // Check other permissions
    return (node->mask & required_perm) != 0;
}

} // namespace MesaOS::FS
