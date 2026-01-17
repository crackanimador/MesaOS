#include "ramfs.hpp"
#include <string.h>
#include "memory/pmm.hpp"
#include "memory/kheap.hpp"

namespace MesaOS::FS {

fs_node* RAMFS::root = 0;
fs_node* RAMFS::files[64];
uint8_t* RAMFS::file_contents[64];
uint32_t RAMFS::file_count = 0;

static struct dirent static_dirent;

uint32_t RAMFS::read(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t id = node->inode;
    if (offset > node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
    memcpy(buffer, file_contents[id] + offset, size);
    return size;
}

uint32_t RAMFS::write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t id = node->inode;
    // Basic write for now, doesn't expand memory
    if (offset + size > 4096) size = 4096 - offset; 
    memcpy(file_contents[id] + offset, buffer, size);
    if (offset + size > node->length) node->length = offset + size;
    return size;
}

dirent* RAMFS::readdir(fs_node* node, uint32_t index) {
    (void)node;
    if (index >= file_count) return 0;
    strcpy(static_dirent.name, files[index]->name);
    static_dirent.ino = files[index]->inode;
    return &static_dirent;
}

fs_node* RAMFS::finddir(fs_node* node, const char* name) {
    (void)node;
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(name, files[i]->name) == 0) return files[i];
    }
    return 0;
}

fs_node* RAMFS::initialize() {
    root = (fs_node*)kmalloc(sizeof(fs_node));
    memset(root, 0, sizeof(fs_node));
    strcpy(root->name, "root");
    root->flags = FS_DIRECTORY;
    root->readdir = &RAMFS::readdir;
    root->finddir = &RAMFS::finddir;
    
    file_count = 0;
    return root;
}

fs_node* RAMFS::create_file(const char* name, const char* content) {
    if (file_count >= 64) return 0;
    
    fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
    memset(node, 0, sizeof(fs_node));
    strcpy(node->name, name);
    node->flags = FS_FILE;
    node->inode = file_count;
    node->read = &RAMFS::read;
    node->write = &RAMFS::write;
    
    uint8_t* buffer = (uint8_t*)MesaOS::Memory::PMM::allocate_block();
    memset(buffer, 0, 4096);
    if (content) {
        strcpy((char*)buffer, content);
        node->length = strlen(content);
    } else {
        node->length = 0;
    }
    
    file_contents[file_count] = buffer;
    files[file_count] = node;
    file_count++;
    
    return node;
}

fs_node* RAMFS::create_dir(const char* name) {
    if (file_count >= 64) return 0;
    fs_node* node = (fs_node*)kmalloc(sizeof(fs_node));
    memset(node, 0, sizeof(fs_node));
    strcpy(node->name, name);
    node->flags = FS_DIRECTORY;
    node->readdir = &RAMFS::readdir;
    node->finddir = &RAMFS::finddir;
    files[file_count++] = node;
    return node;
}

void RAMFS::mount(fs_node* node) {
    if (file_count < 64) {
        files[file_count++] = node;
    }
}

bool RAMFS::delete_file(const char* name) {
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(name, files[i]->name) == 0) {
            // Memory is not actually freed in this simple PMM-based RAMFS
            // We'd just shift the array for now
            for (uint32_t j = i; j < file_count - 1; j++) {
                files[j] = files[j+1];
                file_contents[j] = file_contents[j+1];
            }
            file_count--;
            return true;
        }
    }
    return false;
}

} // namespace MesaOS::FS
