#ifndef RAMFS_HPP
#define RAMFS_HPP

#include "vfs.hpp"

namespace MesaOS::FS {

class RAMFS {
public:
    static fs_node* initialize();
    static fs_node* create_file(const char* name, const char* content);
    static fs_node* create_dir(const char* name);
    static void mount(fs_node* node);
    static bool delete_file(const char* name);

private:
    static fs_node* root;
    static fs_node* files[64];
    static uint8_t* file_contents[64];
    static uint32_t file_count;

    static uint32_t read(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static uint32_t write(fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static dirent* readdir(fs_node* node, uint32_t index);
    static fs_node* finddir(fs_node* node, const char* name);
};

} // namespace MesaOS::FS

#endif
