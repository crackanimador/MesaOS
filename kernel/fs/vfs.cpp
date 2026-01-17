#include "vfs.hpp"
#include <string.h>

namespace MesaOS::FS {

fs_node *fs_root = 0;

uint32_t read_fs(fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node->read != 0)
        return node->read(node, offset, size, buffer);
    else
        return 0;
}

uint32_t write_fs(fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node->write != 0)
        return node->write(node, offset, size, buffer);
    else
        return 0;
}

void open_fs(fs_node *node, uint8_t read, uint8_t write) {
    (void)read; (void)write;
    if (node->open != 0)
        return node->open(node);
}

void close_fs(fs_node *node) {
    if (node->close != 0)
        return node->close(node);
}

struct dirent *readdir_fs(fs_node *node, uint32_t index) {
    if ((node->flags & 0x07) == FS_DIRECTORY && node->readdir != 0)
        return node->readdir(node, index);
    else
        return 0;
}

fs_node *finddir_fs(fs_node *node, const char *name) {
    if ((node->flags & 0x07) == FS_DIRECTORY && node->finddir != 0)
        return node->finddir(node, name);
    else
        return 0;
}

fs_node *find_path_fs(fs_node *root, const char *path) {
    if (!path || path[0] == '\0') return root;
    if (strcmp(path, "/") == 0) return root;

    // Skip leading slash
    const char* p = path;
    if (p[0] == '/') p++;

    char name[32];
    uint32_t i = 0;
    while (p[i] != '/' && p[i] != '\0' && i < 31) {
        name[i] = p[i];
        i++;
    }
    name[i] = '\0';

    fs_node* next = finddir_fs(root, name);
    if (!next) return 0;

    if (p[i] == '/') {
        // Recurse for remaining path
        return find_path_fs(next, p + i + 1);
    }
    
    return next;
}

} // namespace MesaOS::FS
